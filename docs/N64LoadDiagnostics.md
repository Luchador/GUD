# Temporary N64 load diagnostics

This patch gathers evidence for the real-console level-load crash that persists
after the BG header alignment fix. It does not claim to fix the remaining crash.
The baseline is GUD commit `2bede995` (Memory alignment fix).

## Run the hardware test

Apply the patch and build normally. Diagnostics are enabled by default in
`src/n64diagnostics.h`; no USB cartridge or emulator debugger is needed.

1. Copy the newly built ROM to the EverDrive and repeat the failing level load.
2. If it freezes, wait at least 15 seconds for the ten-second watchdog.
3. Photograph the complete diagnostic screen and keep the matching
   `build/u/GUD.map` and `build/u/GUD.elf` from this exact build. For a DEBUG
   build, use the files in `build/d/` instead.
4. Send the photograph and map, along with the level and whether failure occurs
   before the intro, during the intro, or at player control. If no diagnostic
   appears, report that and whether sound continues.

The screen takes over permanently and requires a console reset. Its small font,
framebuffer drawing, and video setup all live in resident code; it requires
neither the game's fonts/textures nor a working RSP/RDP rendering task.

## What the screen means

All numbers on this screen are **hexadecimal**.

| Readout | Meaning |
|---|---|
| `CPU FAULT` | A fatal CPU exception, including a fault inside the TLB pager. Normal recoverable game-code TLB misses still use the existing pager. |
| `NO MAIN PROGRESS FOR 10 SECONDS` | The main thread has not reached another checkpoint for ten seconds. This is a stalled-progress report, not proof of a CPU exception or a particular RCP fault. |
| `INVALID ROM DMA` | A `romCopy` request violates the PI requirements checked here: RAM destination multiple of 8, ROM source multiple of 2, positive even size no greater than 16 MiB. The screen appears before submitting that request. |
| `ROM DMA QUEUE FAILED` | `osPiStartDma` rejected a ROM-copy submission. The old code would ignore the return value and wait for a completion message that might never arrive. |
| `PHASE` | Last coarse operation entered: pools, players, textures, fonts, BG header/data, STAN, setup, guards, paths, Bond initialization, intro, video, or frame processing. It describes where to start looking, not a proven cause. |
| `STAGE`, `STEPS` | Level ID and total number of checkpoint updates. |
| `THREAD`, `CAUSE`, `PC`, `RA`, `BADVADDR`, `FPCSR`, `SP`, `SR` | Faulted thread and saved CPU exception context. `THREAD` is `FFFFFFFF` and exception fields are zero for a watchdog/DMA report without a fault context. |
| `MAIN PC/RA`, `TLB PC/RA` | Last saved main-thread and pager program counter/return address. Useful for a watchdog stall; these are saved contexts, not a stack trace. |
| `SPSTAT`, `DPSTAT`, `PISTAT`, `SP PC`, `DP CUR/END` | Hardware status and command addresses captured **before** the diagnostic halts/freezes graphics. |
| `ROM`, `RAM`, `SIZE`, `DMA WAIT` | Most recent synchronous ROM-copy request. WAIT is 1 after submission begins and 0 after its completion is received. This may be a pager read; audio's separate sample-DMA path is not included. |
| `BANK`, `ALLOC`, `POOL POS/END` | Most recent memory-pool allocation/resize request. ALLOC is requested bytes (new total size for resize), not necessarily a failed allocation. |

Thread IDs used by GUD include 2 scheduler, 3 main/game, 4 audio, 5 TLB pager,
and 6 this diagnostic watchdog. Exception code is `(CAUSE >> 2) & 31`. PC and
RA must be resolved against the map/ELF from this build, since instrumentation
changes code addresses. A branch-delay exception is indicated by CAUSE bit 31.

## Scope and limits

- The watchdog runs every 250 ms above the game, scheduler, audio and pager
  threads, but below the PI/SI/VI managers. It starts at `bossEntry`, after OS
  timers and the scheduler are initialized.
- Checkpoints refresh on actual loading/frame progress. Receiving another VI
  message alone does not refresh them, so an RSP/RDP stall that prevents new
  frame submission can time out.
- DMA/allocation metadata is captured under a short interrupt mask. ROM reads
  do not refresh the main-thread watchdog, so repeated paging cannot hide a
  complete lack of application progress.
- The terminal renderer disables interrupts, captures hardware state, halts
  the RSP, freezes RDP command processing, and draws through an uncached alias
  of the existing framebuffer. It programs a base 320-pixel VI mode directly,
  so a stopped scheduler or black loading screen does not prevent display.
- It cannot guarantee recovery from disabled-interrupt hangs, corrupted
  resident code/data, or hardware/bus failures that prevent its own execution.
  A long legitimate operation with no checkpoints could also time out.
- The core module adds about 9 KiB of resident code/data/stack, plus the hooks.
  Its timing and memory-layout effects make this a diagnostic build, not a
  performance baseline. If the crash disappears, that is useful evidence of
  a timing/layout-sensitive failure, not confirmation of a fix.
- The existing crash handler now restores its saved interrupt mask on the
  no-fault-thread path before waiting again. The ordinary TLB recovery path
  and game scheduling rules are otherwise preserved.

Set `N64_LOAD_DIAGNOSTICS` to `0` in `src/n64diagnostics.h` and rebuild to remove
the diagnostic code and calls. The Makefile explicitly tracks that header for
all consumers. Do this before returning to performance measurements.

## Validation

```sh
python3 tools/tests/n64diagnostics/run.py
```

The host tests execute the production code with OS/register shims. They cover
watchdog timing and Count wraparound, progress reset, recoverable TLB misses,
fatal/pager faults, invalid DMA requests, failed DMA queue submission, captured
metadata, framebuffer bounds, and the terminal graphics-stop sequence. An
optional PPM output path produces a preview using simulated values.

All seven changed/new C units compile with IDO 5.3 for US and DEBUG. The disabled
configuration is also checked for compilation and absence of diagnostic calls.
Linker lists explicitly place the diagnostic text/data/rodata/BSS in resident
sections. A complete ROM build and real-console validation still need the
extracted assets and hardware that are not available in this workspace.
