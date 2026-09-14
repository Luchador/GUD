#!/usr/bin/env python3
"""Exercise the real profiler, scheduler hooks and compact HUD without a ROM."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def strip_includes(source):
    return re.sub(r'^#include[^\n]*\n', '', source, flags=re.M)


def function(source, name):
    match = re.search(r'^(?:static )?[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end] + '\n'


def main():
    source = (HERE / 'harness.h').read_text()
    source += strip_includes((ROOT / 'src/frameprofile.h').read_text())
    source += strip_includes((ROOT / 'src/frameprofile.c').read_text())
    source += strip_includes((ROOT / 'src/sched.h').read_text())
    sched = (ROOT / 'src/sched.c').read_text()
    source += '\n'.join(line for line in sched.splitlines() if line.startswith('#define OS_SC_')) + '\n'
    source += '''
static OSScTask *nextTask;
static FrameBottleneck releasedResult;
static s32 __scTaskComplete(OSSched *sc, OSScTask *task)
{
    if (!(task->state & OS_SC_RCP_MASK) && task->list.t.type == M_GFXTASK)
        releasedResult = frameProfileGetBottleneck();
    return 0;
}
static s32 __scSchedule(OSSched *sc, OSScTask **sp, OSScTask **dp, s32 avail)
{
    if (nextTask && avail == 3) { *sp = *dp = nextTask; nextTask = NULL; return 0; }
    return avail;
}
'''
    for name in ('__scExec', '__scYield', '__scHandleRSP', '__scHandleRDP'):
        source += function(sched, name)
    source += (HERE / 'check.c').read_text()
    with tempfile.TemporaryDirectory(prefix='gud-frameprofile-') as directory:
        work = Path(directory)
        (work / 'check.c').write_text(source)
        command = shlex.split(os.environ.get('CC', 'cc')) + [
            '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-parameter', '-Wno-unused-but-set-variable',
            '-fsanitize=address,undefined']
        subprocess.run(command + [str(work / 'check.c'), '-o', str(work / 'check')], check=True)
        subprocess.run([str(work / 'check')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

        (work / 'profile.inc').write_text(strip_includes((ROOT / 'src/frameprofile.h').read_text()))
        lv = (ROOT / 'src/game/lv.c').read_text()
        (work / 'hud.inc').write_text(function(lv, 'lvDrawProfilerText') + function(lv, 'lvDrawFrameRateDisplay'))
        subprocess.run(command + ['-I', str(work), str(HERE / 'hud.c'), '-o', str(work / 'hud')], check=True)
        subprocess.run([str(work / 'hud')], check=True,
                       env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))

    # Integration gates that the standalone accounting harness cannot exercise.
    for name in ('text', 'data', 'rodata', 'bss'):
        entry = f'build/OUTCODE/src/frameprofile.o (.{name});'
        assert entry in (ROOT / f'ld/lib.{name}.ld.inc').read_text()
        assert entry not in (ROOT / f'ld/game.{name}.ld.inc').read_text()
    linker = (ROOT / 'ge007.ld').read_text()
    for name in ('frameProfileRspStart', 'frameProfileRspStop', 'frameProfileRdpDone'):
        assert f'{name} >= _codeSegmentStart && {name} < _codeSegmentEnd' in linker
    boss = (ROOT / 'src/boss.c').read_text()
    block = boss[boss.index('if (g_MainStageNum < 0 && pendingGfx < 2U'):]
    assert block.index('waitForNextFrame();') < block.index('frameProfileCpuBegin();') < block.index('lvTick();')
    assert block.index('rspGfxTaskStart(') < block.index('memaSingleDefragPass();') < block.index('frameProfileCpuEnd();')
    rsp = function((ROOT / 'src/game/rsp.c').read_text(), 'rspGfxTaskStart')
    assert rsp.index('frameProfileTaskSubmitted(sctask);') < rsp.index('osSendMesg(')
    assert 'osGetCount' not in rsp
    for path in (ROOT / 'src').rglob('*'):
        if path.suffix in ('.c', '.h'):
            assert 'g_Prof' not in path.read_text(), path
    print('PASS: resident placement, frame pacing/submission boundaries and removal of old operation timers.')


if __name__ == '__main__':
    main()
