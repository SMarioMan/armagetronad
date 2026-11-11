#!/usr/bin/env python3
"""Post-build patch for the emscripten SDL1 JS port.

The port's IMG_Load_RW (STB_IMAGE path) nulls the 2D canvas/ctx of loaded
surfaces when SDL.GL is set (the assumption being that a GL app only reads
the heap pixel buffer). SDL_ConvertSurface then unconditionally calls
drawImage(oldData.canvas) -> drawImage(null) -> uncaught TypeError inside
the rAF main loop -> the game freezes on first game start (256x256 map/
player texture with an unexpected BytesPerPixel hits the default case of
rSurface::Create).

Fix: when the source canvas is gone, copy the pixels directly from the
heap buffer (RGBA, w*h*4). Patch the generated JS in place; re-run after
every relink.
"""
import pathlib
import sys
import re

p = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else 'src/armagetronad_main.js')
src = p.read_text(encoding='utf-8', errors='surrogateescape')

m = re.compile(r'var _SDL_ConvertSurface\s*=\s*\(').search(src)
assert m, '_SDL_ConvertSurface definition not found'
start = m.start()
end = src.find('};', start)
assert end != -1, 'end of _SDL_ConvertSurface definition not found'
end += 2

new = '''var _SDL_ConvertSurface=(surf,format,flags)=>{
  if(format){SDL.checkPixelFormat(format)}
  var oldData=SDL.surfaces[surf];
  var ret=SDL.makeSurface(oldData.width,oldData.height,oldData.flags,false,"copy:"+oldData.source);
  var newData=SDL.surfaces[ret];
  if(oldData.canvas&&oldData.ctx&&newData.ctx){
    newData.ctx.globalCompositeOperation="copy";
    newData.ctx.drawImage(oldData.canvas,0,0);
    newData.ctx.globalCompositeOperation=oldData.ctx.globalCompositeOperation;
  } else if(oldData.buffer){
    // (wasm port fix) IMG_Load_RW discards the 2D canvas of loaded
    // surfaces when SDL.GL is set; the pixels live in the heap buffer
    // (RGBA, w*h*4 bytes). Copy directly instead of drawImage(null)
    // (which throws TypeError and kills the rAF main loop).
    var n=oldData.width*oldData.height*4;
    HEAPU8.set(HEAPU8.subarray(oldData.buffer,oldData.buffer+n),newData.buffer);
  } else {
  }
  return ret};'''

if 'wasm port fix' in src[start:end]:
    print('already patched')
    sys.exit(0)

src = src[:start] + new + src[end:]
p.write_text(src, encoding='utf-8', errors='surrogateescape')
print(f'patched _SDL_ConvertSurface in {p}')
