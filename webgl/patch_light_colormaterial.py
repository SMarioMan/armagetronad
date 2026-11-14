#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Puts the vertex colour into the ambient, diffuse, emission and material-alpha
terms of the generated per-vertex lighting pass -- but only for the draws that
actually feed a colour array, which is the case the desktop build agrees with.

Why the distinction matters.  The emulation carries the lighting result in the
same varying that carries the flat colour, so a_color has to exist even for
geometry that has no colour array; when the array is not bound the emulation
fills the attribute with the last immediate-mode colour instead:

    ... } else if (this.hasColor) {
          GLctx.disableVertexAttribArray(this.colorLocation);
          GLctx.vertexAttrib4fv(this.colorLocation, GLImmediate.clientColor); }

Per-draw snapshots taken in the browser build (harness/bodyhook.js, on the
frame-exact perimeter pair -- camera pose byte-identical to the desktop) show
every cycle and model draw arriving that way: 24 draws on the lit program with
a_color off and generic value (1, 0, 1, 1).  Nothing about that triple describes
the mesh; it is whatever colour the last glBegin/glEnd pass happened to leave
behind.  Substituting a_color unconditionally therefore multiplied the whole
lighting sum of every light cycle by (1, 0, 1): the green channel went to zero
and the alpha came from the same stale colour.  Green carries 0.587 of
luminance, so the AI bikes -- drawn from models/cycle_*.mod through rModel.cpp,
always with the colour array off -- came out nearly black against the arena
floor while the walls and the HUD, which do carry vertex colours, looked right.

The desktop build needs no such substitution for that geometry, and the same
measurements say so: the emulation's material and light uniforms are already
exactly what the desktop reports at that draw.  The per-draw state probe in
src/tron/gCycle.cpp (PF LIGHT / PF BODY) reads material ambient (0.2, 0.2,
0.2, 1), material diffuse (1, 1, 1, 1), emission 0, shininess 0, material
specular (1, 1, 1, 1), light ambient 0, light diffuse (1, .7, .7) and
(.7, .7, 1), light model ambient (0.2, 0.2, 0.2) and both light positions with
w = 0, which is directional and is also what webgl/patch_light_positional.py
computes for w < 0.5.  Same inputs, same formula, so the un-substituted terms
are the reference behaviour for those draws and the conditional below restores
them.

What the vertex-colour half is still doing, and why it stays.  Measured on the
perp2 replay with the two builds at the same gate (desktop gt 2.6912, port
gt 2.6519, identical pose), whole-frame agreement against the desktop frame,
mean absolute channel difference / fraction of pixels differing by more than 12:

    variant                       meanAbs   frac>12   blue-dominant bright px
    all five terms unconditional    3.73     4.13 %              635
    no substitution at all          4.63     5.89 %              708
    ambient + diffuse only          3.59     3.93 %              447
    alpha term only                 3.99     4.27 %              520
    desktop reference                 --       --                630

So the immediate-mode half -- where a_color is the real per-vertex colour the
game sets with glColor4f -- is what buys that agreement; the model half is what
destroyed the cycles.  Why taming the vertex colour helps immediate-mode lit
geometry at all, when the desktop ignores the vertex colour with
GL_COLOR_MATERIAL off (measured: the desktop's material ambient stays 0.2 with
the current colour at (1, 1, 1)), is still not pinned down; the earlier claim in
this file that GL_COLOR_MATERIAL was on was wrong and is gone.  What is measured
is that these draws match the desktop better with the substitution, so the
conditional keeps it exactly where it was measured to help.

The flag is read from the WebGL attribute state at that very draw
(getVertexAttrib(colorLocation, ATTRIB_ARRAY_ENABLED)), not from
GLImmediate.colorAttr: a model draw out of VBOs never touches the immediate
renderer that sets that variable, so the flag would report some earlier pass.
Page-side per-draw snapshots confirm the state they key on: every cycle model
draw is drawElements TRIANGLES n123/n243 with a_normal on, a_color off and a
generic a_color of (1,0,1,1), while the desktop probe at the same point in
src/tron/gCycle.cpp reports the current colour as the team colour (0,0.7,0) --
the body draw has no colour array, and the generic value is whatever an
unrelated pass left behind.  Conditional variant, same pose-matched pair:
meanAbs 3.40, frac>12 3.73 %, bright neutral body-like pixels 2714 against the
desktop's 2939, whole-frame magenta-signature pixels 118 against the desktop 138.

SPECULAR stays on u_materialSpecular for every draw, and
webgl/patch_light_specular.py already handles shininess zero.
"""
import sys

PATCHES = [
    # The switch: does this draw feed a colour array or not?  Uploaded below
    # from GLImmediate.colorAttr, the very value the draw code branches on when
    # it decides between vertexAttribPointer and the stale clientColor.
    ('vsLightingDefs+="uniform vec4 u_materialEmission;";',
     'vsLightingDefs+="uniform vec4 u_materialEmission;";'
     'vsLightingDefs+="uniform int u_colorFromArray;";'),

    # global ambient * material ambient  ->  global ambient * vertex colour
    ('vsLightingPass+="  v_color.xyz += u_lightModelAmbient.xyz * u_materialAmbient.xyz;";',
     'vsLightingPass+="  v_color.xyz += u_lightModelAmbient.xyz * '
     '((u_colorFromArray != 0) ? a_color.xyz : u_materialAmbient.xyz);";'),

    # per-light ambient
    ('vsLightingPass+="    vec3 ambient = u_lightAmbient"+lightId+".xyz * u_materialAmbient.xyz;";',
     'vsLightingPass+="    vec3 ambient = u_lightAmbient"+lightId+".xyz * '
     '((u_colorFromArray != 0) ? a_color.xyz : u_materialAmbient.xyz);";'),

    # per-light diffuse
    ('vsLightingPass+="    vec3 diffuse = diffuseI * u_lightDiffuse"+lightId+".xyz * u_materialDiffuse.xyz;";',
     'vsLightingPass+="    vec3 diffuse = diffuseI * u_lightDiffuse"+lightId+".xyz * '
     '((u_colorFromArray != 0) ? a_color.xyz : u_materialDiffuse.xyz);";'),

    # emission: a vertex-coloured primitive carries its own colour where no
    # light reaches it (the trail fade); a model keeps the material emission,
    # which is what the desktop uses.
    ('vsLightingPass+="  v_color.xyz = u_materialEmission.xyz;";',
     'vsLightingPass+="  v_color.xyz = '
     '((u_colorFromArray != 0) ? a_color.xyz : u_materialEmission.xyz);";'),

    # material alpha of the tracked components follows the vertex colour, which
    # is what lets trails fade out along their length; models keep the
    # material alpha, measured as 1 on both builds.
    ('vsLightingPass+="  v_color.w = u_materialDiffuse.w;";',
     'vsLightingPass+="  v_color.w = '
     '((u_colorFromArray != 0) ? a_color.w : u_materialDiffuse.w);";'),

    # Location of the switch in the generated program.
    ('this.hasLighting=GLEmulation.lightingEnabled;',
     'this.hasLighting=GLEmulation.lightingEnabled;'
     'this.colorFromArrayLocation=GLctx.getUniformLocation(this.program,"u_colorFromArray");'),

    # And its value, once per draw, next to the other lighting uploads.  The
    # query reads the WebGL attribute state at that very draw, which is what the
    # per-draw snapshots in harness/bodyhook.js record: model geometry arrives
    # with the colour attribute disabled and would otherwise be multiplied by
    # the emulation's stale GLImmediate.clientColor.  GLImmediate.colorAttr
    # alone is not enough -- a model draw from VBOs bypasses the immediate
    # renderer that sets it, so the flag would report an earlier pass.
    ('if(this.hasLighting){if(this.lightModelAmbientLocation)'
     'GLctx.uniform4fv(this.lightModelAmbientLocation,GLEmulation.lightModelAmbient);',
     'if(this.hasLighting){if(this.colorFromArrayLocation)'
     'GLctx.uniform1i(this.colorFromArrayLocation,(this.colorLocation!=null&&'
     'GLctx.getVertexAttrib(this.colorLocation,GLctx.ATTRIB_ARRAY_ENABLED))?1:0);'
     'if(this.lightModelAmbientLocation)'
     'GLctx.uniform4fv(this.lightModelAmbientLocation,GLEmulation.lightModelAmbient);'),
]

# Marker of a fully applied patch, used to keep the script idempotent.
MARKER = 'u_colorFromArray'


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if MARKER in data:
        print('patch_light_colormaterial: already patched')
        return
    missing = [old for old, _ in PATCHES if old not in data]
    if missing:
        sys.exit('ERROR: colour-source anchors not found, the generated lighting '
                 'pass has changed shape:\n' + '\n'.join(repr(m) for m in missing))
    for old, new in PATCHES:
        data = data.replace(old, new, 1)
    open(path, 'w').write(data)
    print('patch_light_colormaterial: applied conditional colour source '
          '(vertex colour where a colour array feeds the draw, material uniforms '
          'where it does not)')


if __name__ == '__main__':
    main()
