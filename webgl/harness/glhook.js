// Harness-only WebGL instrumentation, injected into webgl/dist/index.html.
// Reports, per texture object: the uploaded mip chain, the min/mag filter, the
// anisotropy actually applied, and (once) the shader sources the GL emulation
// compiles -- including their precision qualifiers, which decide how accurately
// interpolated texture coordinates survive.  The game's own texture state lives
// in C++ the page cannot read, so this is the only way to see what the floor
// textures really do inside the fixed-function emulation.  Never shipped.
(function () {
  var P = window.WebGLRenderingContext && WebGLRenderingContext.prototype;
  if (!P) { console.log('GLHOOK no WebGLRenderingContext'); return; }

  var MIN_FILTER = 0x2801, MAG_FILTER = 0x2800;
  var TEXTURE_2D = 0x0de1;
  var MAX_ANISO = 0x84fe, MIN_LOD = 0x884b;   // EXT_texture_filter_anisotropic
  var names = {};
  names[0x2600] = 'NEAREST'; names[0x2601] = 'LINEAR';
  names[0x2700] = 'NEAREST_MIPMAP_NEAREST'; names[0x2701] = 'LINEAR_MIPMAP_NEAREST';
  names[0x2702] = 'NEAREST_MIPMAP_LINEAR'; names[0x2703] = 'LINEAR_MIPMAP_LINEAR';
  names[0x2901] = 'REPEAT'; names[0x812f] = 'CLAMP_TO_EDGE';

  var id = 0, info = new WeakMap(), bound = {};
  function rec(tex) {
    var e = info.get(tex);
    if (!e) { e = {n: ++id, levels: [], w: 0, h: 0, min: '?', mag: '?', aniso: '?', lod: '?'}; info.set(tex, e); }
    return e;
  }
  function line(e) {
    return 'GLHOOK tex#' + e.n + ' ' + e.w + 'x' + e.h + ' levels[' +
           e.levels.join(',') + '] min=' + e.min + ' mag=' + e.mag +
           ' aniso=' + e.aniso + ' lod=' + e.lod;
  }

  var orig = {};
  function wrap(name, after) {
    orig[name] = P[name];
    P[name] = function () {
      var r = orig[name].apply(this, arguments);
      try { after(this, arguments, r); } catch (e) {}
      return r;
    };
  }

  wrap('bindTexture', function (gl, a) {
    if (a[0] === TEXTURE_2D) {
      gl.__bt2d = a[1];
      if (a[1]) rec(a[1]);
    }
  });
  wrap('texImage2D', function (gl, a) {
    var tex = gl.__bt2d; if (!tex) return;
    var e = rec(tex);
    // 9-arg form: target level internalformat width height border format type pixels
    if (a.length >= 6) {
      var lvl = a[1] | 0, w = a[3] | 0, h = a[4] | 0;
      if (lvl === 0) { e.w = w; e.h = h; e.levels = []; }
      e.levels.push(lvl + ':' + w + 'x' + h);
    }
  });
  function noteParam(gl, a) {
    var tex = gl.__bt2d; if (!tex) return;
    var e = rec(tex), pname = a[1], val = a[2];
    if (pname === MIN_FILTER) e.min = names[val] || ('0x' + val.toString(16));
    else if (pname === MAG_FILTER) e.mag = names[val] || ('0x' + val.toString(16));
    else if (pname === MAX_ANISO) e.aniso = val;
    else if (pname === MIN_LOD) e.lod = val;
    else return;
    // report once the filtering is fully known; aniso usually lands later, so
    // re-report when it changes.
    if (e.min !== '?' && e.mag !== '?') {
      if (!e.reported) { e.reported = 1; console.log(line(e)); }
      else if (pname === MAX_ANISO) { console.log(line(e) + ' *'); }
    }
  }
  wrap('texParameteri', function (gl, a) { noteParam(gl, a); });
  wrap('texParameterf', function (gl, a) { noteParam(gl, a); });
  wrap('generateMipmap', function (gl, a) {
    var tex = gl.__bt2d; if (!tex) return;
    var e = rec(tex); e.mipgen = 1;
    console.log('GLHOOK tex#' + e.n + ' glGenerateMipmap');
  });

  // Shader sources: precision qualifiers and varyings are what decide whether a
  // texture coordinate derived from world position keeps enough mantissa to land
  // on the right texel of a 1-texel-wide line profile.
  var shown = 0;
  wrap('shaderSource', function (gl, a, r) {
    if (shown >= 8) return;
    var src = a[1] || '';
    var keep = [];
    var lines = src.split('\n');
    for (var i = 0; i < lines.length; i++) {
      var L = lines[i];
      if (/precision|varying|^uniform.*(vec[234])|attribute/.test(L) && L.length < 130) keep.push(L.trim());
    }
    shown++;
    console.log('GLHOOK SHADER#' + shown + ' (' + lines.length + ' lines) ' +
                lines.join(' ; ').slice(0, 1900));
  });

  // Report the framebuffer sample count once the game has a drawing buffer.
  setTimeout(function () {
    var c = document.getElementById('canvas');
    if (!c) return;
    var gl = c.getContext('webgl') || c.getContext('experimental-webgl');
    if (!gl) { console.log('GLHOOK no context on canvas'); return; }
    var ext = gl.getExtension('EXT_texture_filter_anisotropic') ||
              gl.getExtension('WEBKIT_EXT_texture_filter_anisotropic');
    var hp = gl.getShaderPrecisionFormat(gl.FRAGMENT_SHADER, gl.HIGH_FLOAT);
    console.log('GLHOOK ctx samples=' + gl.getParameter(gl.SAMPLES) +
                ' samples-qual=' + gl.getParameter(gl.SAMPLE_BUFFERS) +
                ' max-aliased-line=' + JSON.stringify(gl.getParameter(gl.ALIASED_LINE_WIDTH_RANGE)) +
                ' max-texture=' + gl.getParameter(gl.MAX_TEXTURE_SIZE) +
                ' renderer=' + gl.getParameter(gl.RENDERER) +
                ' aniso-ext=' + !!ext +
                ' aniso-max=' + (ext ? gl.getParameter(ext.MAX_TEXTURE_MAX_ANISOTROPY_EXT) : '-') +
                ' frag-highp=' + (hp ? hp.precision + '/' + hp.rangeMin : '-'));
  }, 6000);
})();
