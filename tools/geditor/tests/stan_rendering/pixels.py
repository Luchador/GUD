#!/usr/bin/env python3
"""Pixel regression on Linux Mesa/EGL (libEGL, libGL, and GL/gl.h required).

GL_HEADER may name an alternative gl.h, including MinGW's OpenGL 1.1 header.
VIEWPORT_SOURCE may name an older viewport.c to reproduce the original bug.
"""
import ctypes as c
import importlib.util
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

here=Path(__file__).resolve().parent
src=here.parents[1]/'src'
sys.dont_write_bytecode=True
spec=importlib.util.spec_from_file_location('extract',here.parent/'portal_editing/run.py')
extract=importlib.util.module_from_spec(spec);spec.loader.exec_module(extract)
egl=c.CDLL('libEGL.so.1')
def api(name, result, *args):
    fn=getattr(egl,name);fn.restype=result;fn.argtypes=args;return fn
ptr=c.c_void_p;integer=c.c_int;uint=c.c_uint;ints=c.POINTER(integer)
getproc=api('eglGetProcAddress',ptr,c.c_char_p)
platform=c.CFUNCTYPE(ptr,uint,ptr,ints)(getproc(b'eglGetPlatformDisplayEXT'))
display=platform(0x31DD,None,None)  # EGL_PLATFORM_SURFACELESS_MESA
assert api('eglInitialize',uint,ptr,ints,ints)(display,None,None)
assert api('eglBindAPI',uint,uint)(0x30A2)  # EGL_OPENGL_API
attrs=(integer*15)(0x3033,1,0x3040,8,0x3024,8,0x3023,8,0x3022,8,0x3025,24,0x3021,0,0x3038)
config=ptr();count=integer()
assert api('eglChooseConfig',uint,ptr,ints,c.POINTER(ptr),integer,ints)(display,attrs,c.byref(config),1,c.byref(count)) and count.value
surface=api('eglCreatePbufferSurface',ptr,ptr,ptr,ints)(display,config,(integer*5)(0x3057,256,0x3056,256,0x3038))
context=api('eglCreateContext',ptr,ptr,ptr,ptr,ints)(display,config,None,(integer*1)(0x3038))
assert surface and context and api('eglMakeCurrent',uint,ptr,ptr,ptr,ptr)(display,surface,surface,context)
try:
    with tempfile.TemporaryDirectory(prefix='geditor-stan-pixels-') as temp:
        work=Path(temp)
        if os.environ.get('GL_HEADER'):
            (work/'GL').mkdir();(work/'GL/gl.h').write_bytes(Path(os.environ['GL_HEADER']).read_bytes())
        viewport=Path(os.environ.get('VIEWPORT_SOURCE',src/'viewport.c')).read_text()
        (work/'types.inc').write_text(re.search(r'typedef struct Vertex \{.*?\} Vertex;',viewport,re.S)[0])
        (work/'state.inc').write_text(re.search(r'typedef struct ViewportState \{.*?\} ViewportState;',
                                               (here/'check.c').read_text(),re.S)[0])
        (work/'draw.inc').write_text(''.join(extract.function(viewport,n) for n in
            ('ViewportStanVisible','ViewportApplyStanOpacity','ViewportDrawStanExtrusion',
             'ViewportDrawStanTypeLabels','ViewportDrawStanDiscontinuities','ViewportDrawStanOverlay')))
        subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra',
            '-Werror','-Wno-unused-parameter','-shared','-fPIC',f'-I{work}',
            f'-I{here.parent/"image_import"}',f'-I{src}',str(here/'pixels.c'),
            '-l:libGL.so.1','-o',str(work/'render.so')],check=True)
        render=c.CDLL(str(work/'render.so')).Render
        render.argtypes=[c.c_float,c.c_float,integer,integer,integer,integer,integer,c.POINTER(c.c_ubyte)]
        render.restype=None
        pixels=(c.c_ubyte*(256*256*3))()
        def white(distance,angle,opacity,fill,wall=0,extrusion=0):
            render(distance,angle,opacity,fill,wall,extrusion,0,pixels)
            return {i for i in range(256*256) if min(pixels[i*3:i*3+3])>=250}
        def red(distance,angle,opacity=100,fill=1,wall=0,warning=1):
            render(distance,angle,opacity,fill,wall,0,warning,pixels)
            return {i for i in range(256*256) if pixels[i*3]>=250 and max(pixels[i*3+1:i*3+3])<5}
        cases=0
        for distance in (2.5,4,10,50):
            for angle in (10,30,60,85):
                reference=white(distance,angle,100,0)
                opaque=white(distance,angle,100,1)
                assert len(reference)>10
                assert reference==opaque, (distance,angle,'hidden perimeter',len(reference),len(opaque))
                # A quad's fan diagonal must never be added to its perimeter.
                assert not white(distance,angle,100,1,wall=1)
                assert not white(distance,angle,0,1)
                preview=white(distance,angle,100,1,extrusion=1)
                assert reference<=preview, (distance,angle,'hidden preview perimeter')
                assert not white(distance,angle,100,1,wall=1,extrusion=1)
                reference=red(distance,angle,warning=2)
                assert len(reference)>5
                for opacity in (44,100):
                    assert reference==red(distance,angle,opacity), (distance,angle,'hidden or extra warning edges')
                assert not red(distance,angle,wall=1)
                assert not red(distance,angle,opacity=0)
                assert not red(distance,angle,warning=3)
                cases+=1
        print(f'PASS: real GL pixels in {cases} close/distant/angled views: complete opaque perimeters, extrusion edges, red discontinuities without helper edges, hidden/0% suppression and foreground occlusion.')
finally:
    api('eglMakeCurrent',uint,ptr,ptr,ptr,ptr)(display,None,None,None)
    api('eglDestroyContext',uint,ptr,ptr)(display,context)
    api('eglDestroySurface',uint,ptr,ptr)(display,surface)
    api('eglTerminate',uint,ptr)(display)
