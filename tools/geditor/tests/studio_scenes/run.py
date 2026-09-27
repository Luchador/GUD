#!/usr/bin/env python3
"""Exercise production scene creation/listing with real files and failure injection.

Uses the existing POSIX Win32 file shim. Native dialogs, Windows filename case
folding, and resize painting require the Windows build/manual smoke test.
"""
import json
import os
from pathlib import Path
import subprocess
import tempfile


def main():
    here = Path(__file__).resolve().parent
    src = here.parent.parent / 'src'
    platform = here.parent / 'project_rebase'
    with tempfile.TemporaryDirectory(prefix='geditor-scenes-') as temp:
        work = Path(temp)
        command = [os.environ.get('CC', 'cc'), '-O1', '-g', '-std=c99', '-Wall', '-Wextra',
                   '-Wno-unused-parameter', '-Wno-format', '-ffunction-sections', '-fdata-sections',
                   '-fsanitize=address,undefined', '-Dfopen=TestFopen', f'-I{platform}', f'-I{src}',
                   f'-I{src.parents[2]}', str(here / 'check.c'), str(platform / 'platform.c'),
                   str(src / 'project.c'), str(src / 'studioscene.c'), str(src / 'studioassets.c'), '-Wl,--gc-sections',
                   '-Wl,--wrap=WriteFile,--wrap=FlushFileBuffers,--wrap=CloseHandle', '-o', str(work / 'check')]
        subprocess.run(command, check=True)
        subprocess.run([str(work / 'check'), str(work)], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        scenes = work / 'Project/studio/scenes'
        expected = {'format': 'GEditor Render Studio', 'version': 1, 'objects': []}
        for name in ('Main.rnd', 'Alpha.rnd', 'Scene 01.rnd', 'Scene 40.rnd'):
            assert json.loads((scenes / name).read_text()) == expected
        assert (scenes / 'zeta.RND').read_text() == 'existing future scene content'
        assert (work / 'Blocked/studio/scenes').read_text() == 'keep this file'
        print('PASS: versioned JSON scenes and unchanged existing scene/folder-collision bytes.')


if __name__ == '__main__':
    main()
