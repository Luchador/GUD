#!/usr/bin/env python3
"""Runtime room-table/empty-stream regression; optional native BG file arguments."""
from pathlib import Path
import importlib.util
import os
import re
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
root = here.parents[2]
spec = importlib.util.spec_from_file_location('extract', root / 'tools/geditor/tests/vertex_eyedropper/run.py')
extract = importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)
bg = (root / 'src/game/bg.c').read_text()
header = (root / 'src/game/bg.h').read_text()
with tempfile.TemporaryDirectory(prefix='gud-bg-room-table-') as temp:
    work = Path(temp)
    (work / 'roomtypes.inc').write_text('\n'.join(re.search(
        r'typedef struct ' + name + r'\s*\{.*?\}\s*' + name + r';', header, re.S)[0]
        for name in ('RoomVtxBatchBounds', 'RoomInfo')))
    functions = ''.join(extract.function(bg, name) for name in (
        'bgCountRooms', 'bgInitRoomStreams', 'bgLoadRoomVtxData',
        'bgLoadRoomPrimaryGdl', 'bgLoadRoomSecondaryGdl', 'bgRoomSetEmptyBounds', 'bgRoomCalcBB'))
    # N64 pointers and room records are 32/24 bits/bytes. Use native host
    # addressing for bounds traversal without altering the algorithm.
    functions = functions.replace('(BgRoomData *) ((s32) ptr_bgdata_room_fileposition_list + room * 24)',
                                  '&ptr_bgdata_room_fileposition_list[room]')
    functions = functions.replace('(s32) g_BgRoomInfo[room].vertices', '(uintptr_t) g_BgRoomInfo[room].vertices')
    (work / 'roomlogic.inc').write_text(functions)
    ramrom = (root / 'src/ramrom.c').read_text()
    dma = (root / 'src/libultra/io/pirawdma.c').read_text()
    (work / 'dmalogic.inc').write_text(extract.function(dma,'osPiRawStartDma')
        + ''.join(extract.function(ramrom, n) for n in ('doRomCopy','romReceiveMesg','romCopy')))
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-Wno-pointer-to-int-cast','-Wno-int-conversion','-fsanitize=address,undefined',
        f'-I{work}',str(here/'check.c'),'-lm','-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check'),*sys.argv[1:]],check=True,
        env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
