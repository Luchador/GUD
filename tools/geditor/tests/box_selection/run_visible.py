#!/usr/bin/env python3
"""Pixel-test the production visible-face box selection on a headless Mesa context.

Requires libEGL.so.1, libGL.so.1 and GL/gl.h (or GL_INCLUDE pointing to headers).
Uses real rendering, including cutout alpha, draw order, culling and depth.
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
    extract = runpy.run_path(str(here.parent / 'studio_lights/run.py'))['extract']
    viewport = (src / 'viewport.c').read_text()
    logic = ''.join(extract(viewport, n) for n in
        ('ViewportCompareStanIds', 'ViewportStanTileHidden', 'ViewportPortalGeometryIsFirst',
         'ViewportTriangleHidden', 'ViewportBatchIsPickable', 'ViewportApplyRenderFlags',
         'ViewportBatchCullMode', 'ViewportApplyCullMode', 'ViewportTextureKey', 'ViewportApplyTextureWrap',
         'ViewportBoxFaceColor', 'ViewportDrawBoxFaceIds', 'ViewportFilterBoxFacesGL'))
    types = ''
    for name in ('ViewportRenderMode', 'ViewportBoxFaceKind'):
        source = (src / 'viewport.h').read_text() if name == 'ViewportRenderMode' else viewport
        types += re.search(r'typedef enum ' + name + r' \{.*?\} ' + name + ';', source, re.S)[0] + '\n'
    for name in ('Vertex', 'SceneBatch', 'ViewportTexture'):
        types += re.search(r'typedef struct ' + name + r' \{.*?\} ' + name + ';', viewport, re.S)[0] + '\n'
    for name in ('BLEND_ALPHA_THRESHOLD', 'CUTOUT_ALPHA_THRESHOLD', 'FOV_Y', 'NEAR_Z', 'FAR_Z', 'DEG_TO_RAD'):
        types += re.search(r'^#define VIEWPORT_' + name + r' .*', viewport, re.M)[0] + '\n'
    types += '#define GL_CLAMP_TO_EDGE 0x812F\n#define GL_MIRRORED_REPEAT 0x8370\n'
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
    surface = egl.eglCreatePbufferSurface(display, config, ints(0x3057, 200, 0x3056, 200, 0x3038))
    context = egl.eglCreateContext(display, config, None, ints(0x3038))
    assert surface and context and egl.eglMakeCurrent(display, surface, surface, context)
    try:
        with tempfile.TemporaryDirectory(prefix='geditor-visible-box-') as temp:
            work = Path(temp)
            (work / 'types.inc').write_text(types)
            (work / 'logic.inc').write_text(logic)
            cmd = [os.environ.get('CC', 'cc'), '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-shared', '-fPIC', '-Wno-unused-parameter', f'-I{work}', f'-I{src}', f'-I{here.parent / "image_import"}']
            if os.environ.get('GL_INCLUDE'):
                (work / 'GL').mkdir()
                (work / 'GL/gl.h').write_bytes((Path(os.environ['GL_INCLUDE']) / 'GL/gl.h').read_bytes())
            cmd += [str(here / 'visible.c'), str(src / 'orbitcamera.c'), '-l:libGL.so.1', '-lm', '-o', str(work / 'check.so')]
            subprocess.run(cmd, check=True)
            c.CDLL(str(work / 'check.so')).CheckVisibleFaces()
    finally:
        egl.eglMakeCurrent(display, None, None, None)
        egl.eglTerminate(display)


if __name__ == '__main__':
    main()
