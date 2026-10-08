#!/usr/bin/env python3
"""Pixel-test the production water preview on a headless Mesa context.

Requires libEGL.so.1, libGL.so.1 and GL/gl.h (or GL_INCLUDE pointing to headers).
Uses real rendering, texture reloads, animation, horizon clipping and depth state.
"""
import ctypes as c
import os
from pathlib import Path
import re
import runpy
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parents[1] / 'src'
    extract = runpy.run_path(str(here.parent / 'object_properties/run.py'))['function']
    viewport = (src / 'viewport.c').read_text()
    logic = ''.join(extract(viewport, n) for n in
                    ('ViewportGetBasis', 'ViewportDrawWater', 'ViewportUpdateSkyTimer',
                     'ViewportSetLevelWater', 'ViewportSetLevelClouds'))
    types = '\n'.join(re.search(r'^#define ' + name + r' .*', viewport, re.M)[0]
                      for name in ('VIEWPORT_FOV_Y', 'VIEWPORT_DEG_TO_RAD', 'VIEWPORT_CLOUD_TIMER')) + '\n'
    # EGL_MESA_platform_surfaceless; OpenGL compatibility, RGBA8/depth24 pbuffer.
    egl = c.CDLL('libEGL.so.1')
    signatures = {
        'eglGetPlatformDisplay': (c.c_void_p, [c.c_uint, c.c_void_p, c.POINTER(c.c_int)]),
        'eglInitialize': (c.c_uint, [c.c_void_p, c.POINTER(c.c_int), c.POINTER(c.c_int)]),
        'eglChooseConfig': (c.c_uint, [c.c_void_p, c.POINTER(c.c_int), c.POINTER(c.c_void_p), c.c_int, c.POINTER(c.c_int)]),
        'eglBindAPI': (c.c_uint, [c.c_uint]),
        'eglCreatePbufferSurface': (c.c_void_p, [c.c_void_p, c.c_void_p, c.POINTER(c.c_int)]),
        'eglCreateContext': (c.c_void_p, [c.c_void_p, c.c_void_p, c.c_void_p, c.POINTER(c.c_int)]),
        'eglMakeCurrent': (c.c_uint, [c.c_void_p, c.c_void_p, c.c_void_p, c.c_void_p]),
        'eglTerminate': (c.c_uint, [c.c_void_p]),
    }
    for name, (result, args) in signatures.items():
        fn = getattr(egl, name)
        fn.restype, fn.argtypes = result, args
    ints = lambda *args: (c.c_int * len(args))(*args)
    display = egl.eglGetPlatformDisplay(0x31DD, None, None)
    assert display and egl.eglInitialize(display, None, None)
    config, count = c.c_void_p(), c.c_int()
    attributes = ints(0x3033, 1, 0x3040, 8, 0x3024, 8, 0x3023, 8,
                      0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3038)
    assert egl.eglChooseConfig(display, attributes, c.byref(config), 1, c.byref(count)) and count.value
    assert egl.eglBindAPI(0x30A2)
    surface = egl.eglCreatePbufferSurface(display, config, ints(0x3057, 64, 0x3056, 64, 0x3038))
    context = egl.eglCreateContext(display, config, None, ints(0x3038))
    assert surface and context and egl.eglMakeCurrent(display, surface, surface, context)
    try:
        with tempfile.TemporaryDirectory(prefix='geditor-water-gl-') as temp:
            work = Path(temp)
            (work / 'types.inc').write_text(types)
            (work / 'logic.inc').write_text(logic)
            cmd = [os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-Wno-unused-parameter', '-shared', '-fPIC', f'-I{work}', f'-I{src}', f'-I{here.parent / "image_import"}']
            if os.environ.get('GL_INCLUDE'):
                (work / 'GL').mkdir()
                (work / 'GL/gl.h').write_bytes((Path(os.environ['GL_INCLUDE']) / 'GL/gl.h').read_bytes())
            cmd += [str(here / 'pixels.c'), str(src / 'water.c'), str(src / 'clouds.c'), '-l:libGL.so.1', '-lm', '-o', str(work / 'check.so')]
            subprocess.run(cmd, check=True)
            c.CDLL(str(work / 'check.so')).CheckWaterPixels()
    finally:
        egl.eglMakeCurrent(display, None, None, None)
        egl.eglTerminate(display)


if __name__ == '__main__':
    main()
