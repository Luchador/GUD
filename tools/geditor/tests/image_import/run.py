#!/usr/bin/env python3
"""Run image encoding/save/export regressions on Linux without a Windows UI.

Uses the production encoder, metadata reader, image edit store and ROM packer.
The BMP routines are extracted unchanged; WIC/dialogs require Windows testing.
The small Win32 file shim maps only the file APIs used by these routines.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source, name):
    match = re.search(r"^(?:static )?(?:BOOL|DWORD|int|s32|u32|u16) " + name + r"\(", source, re.M)
    if match is None:
        raise RuntimeError(f"Cannot find production function {name}")
    start = source.index("{", match.start())
    depth, end = 1, start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--photoshop-bmp', type=Path, help='Also verify the original jungle_tree_wall.bmp regression image.')
    args = parser.parse_args()
    tests = Path(__file__).resolve().parent
    src = tests.parent.parent / "src"
    source = (src / "texload.c").read_text()
    native = '#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
    native += '#include "texload.h"\n#include "imageedits.h"\n'
    native += source[source.index("#define GUTX_DESC_OFFSET"):source.index("/* WIC handles")]
    for name in ("TexWriteBmp", "TexReadBmpThumb", "TexThumbCompare", "TexLoadProjectThumbnails",
                 "TexLoadSavedProjectImage", "TexLoadProjectImage", "TexGetProjectImageSize"):
        native += "\n" + function(source, name) + "\n"
    native += '\nBOOL TestRestoreImportBmpAlpha(const char *path, TexPixel *pixels, DWORD w, DWORD h)\n{ return TexRestoreImportBmpAlpha(path, pixels, w, h); }\n'
    with tempfile.TemporaryDirectory(prefix="geditor-image-test-") as temp:
        work = Path(temp)
        (work / "texload_host.c").write_text(native)
        # Execute the actual runtime allocation formula with N64 struct sizes.
        engine = (src.parents[2] / 'src/game/image.c').read_text()
        allocation = '''#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32;
#define G_IM_SIZ_32b 3
#define G_IM_SIZ_16b 2
#define G_IM_SIZ_8b 1
#define TEXFORMAT_RGBA16_CI8 9
#define RAW_TEXTURE_DESCRIPTOR_OFFSET 16
#define RAW_TEXTURE_DESCRIPTOR_SIZE 12
#define ARRAYCOUNT(a) (sizeof(a)/sizeof(*(a)))
static const int g_TexFormatDepths[]={3,2,3,2,2,1,0,1,0,1,0,1,0};
struct texdataprefix { uint32_t magic,descriptor; };
struct tex { uint32_t native[4]; };
'''
        allocation += '\n'.join(function(engine, name) for name in
            ('texReadRawU16', 'texReadRawU32', 'texRawLevelBytes', 'texRawAllocationBytes'))
        allocation += '\nuint32_t TestTextureRam(const unsigned char *data) { return texRawAllocationBytes((u8 *)data); }\n'
        (work / 'allocation_host.c').write_text(allocation)
        command = [os.environ.get("CC", "cc"), "-O1", "-g", "-std=c99", "-Wall", "-Wextra",
                   "-Wno-format-overflow", "-ffunction-sections", "-fdata-sections",
                   "-fsanitize=address,undefined", "-Dfopen=TestFopen", f"-I{tests}", f"-I{src}",
                   str(tests / "check.c"), str(tests / "bmp_alpha.c"), str(tests / "reimport.c"), str(tests / "flips.c"), str(tests / "surfaces.c"),
                   str(tests / "platform.c"), str(work / "texload_host.c"), str(work / 'allocation_host.c')]
        command += [str(src / name) for name in ("texencode.c", "texinfo.c", "texrom.c", "imageedits.c", "gltf.c", "gltfjson.c", 'modelmaterials.c', 'bgmaterial.c', "bgrender.c")]
        command += ["-lm", "-Wl,--gc-sections", "-o", str(work / "check")]
        subprocess.run(command, check=True)
        env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1")
        subprocess.run([str(work / "check"), str(work / "project")]
                       + ([str(args.photoshop_bmp.resolve())] if args.photoshop_bmp else []), env=env, check=True)


if __name__ == "__main__":
    main()
