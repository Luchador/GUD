# Temporary N64 load diagnostics

This patch gathers evidence for the real-console level-load crash that persists
after the BG header alignment fix. It does not claim to fix the remaining crash.
This consolidated Diagnostic 04 patch targets GUD commit `58435db0` (RDP check).
That commit contains the checker source files but not the earlier diagnostic
integration. This patch includes the missing hooks, linker entries, build
dependencies, displays and tests as well as Diagnostic 04's register capture.
Apply this patch directly to that HEAD; older diagnostic patches are not needed.
The hardware investigation targets the normal US build; `DEBUG` is not needed.

## Run the hardware test

Apply the patch and build normally. Diagnostics are enabled by default in
`src/n64diagnostics.h`; no USB cartridge or emulator debugger is needed.

1. Try the newly built ROM in GUD first. A preflight hazard can be caught there
   even if the emulator previously tolerated the bad graphics input. If it
   passes, copy the ROM to the EverDrive and repeat the failing level load.
2. If it freezes, wait at least 15 seconds for the ten-second watchdog.
3. Photograph **all six pages**. They cycle automatically every eight seconds
   and repeat until reset; no controller input is needed. Keep the matching
   `build/u/GUD.map` and `build/u/GUD.elf` from this exact build.
4. Send the photographs, ELF and map, along with the level and whether failure occurs
   before the intro, during the intro, or at player control. If no diagnostic
   appears, report that and whether sound continues.

The screen takes over permanently and requires a console reset. Its small font,
framebuffer drawing, and video setup all live in resident code; it requires
neither the game's fonts/textures nor a working RSP/RDP rendering task.
The optional scalar-register capture briefly runs a terminal RSP store program;
if it cannot run, the remaining diagnostic pages are still drawn by the CPU.

## Findings from the Diagnostic 03 capture

The September 12 photos `20260912_011228.jpg` through `20260912_011251.jpg`
and matching `GUD(2).elf` / `GUD(2).map` show the same graphics-progress stall:

| Observation | Interpretation |
|---|---|
| Stage `0x29`, phase WAIT FRAME | Cradle has reached the frame-completion wait. |
| MAIN PC `0x7000C808` | `osRecvMesg + 0x68`, after yielding the CPU; no CPU exception is reported. |
| Halted SP PC `0x1B8`, instruction `0x40175000` | `mfc0 $23, DPC_CURRENT` in the graphics FIFO free-space loop. |
| DP STATUS `0x760` | Command, pipeline and DMA busy, with START/END valid; the diagnostic had not yet set FREEZE. |
| FIFO `0x802BDCA0..0x802C7CA0`, WRITE/END `0x002BE7A0`, CURRENT `0x002BE800` | A 40 KiB circular buffer; CURRENT above END is possible during a wrap, not proof of bad bounds. |
| Preflight status 1, `0x1150` (4,432) commands | The last CPU check reached END without finding an implemented hazard. |
| CHECK GDL `0x800EDAE0`, RSP GDL `0x800D4AE0` | The displayed check describes a different, queued frame. It cannot establish the running frame's complete check result. |

The microcode text and initial data are byte-identical across all three supplied
ELFs. IMEM is loaded at `0x1080`, so SP PC `0x1B8` corresponds to text offset
`0x138`; ignoring this offset would identify the wrong instruction.
No specific malformed RDP command is proved by the raw FIFO window. It contains
triangle coefficients as well as possible command headers, and is not a trace.
The evidence points to an RDP progress failure with the RSP waiting behind it;
the triggering command or hardware condition remains unidentified.

## Diagnostic 04: match the task and capture RSP input

Page 4 now retains the preflight result for the graphics task selected by the
scheduler, matched by list pointer and size. Four recent checks cover the two
graphics task slots and queued frames; the newest matching record wins when a
buffer is reused. The result survives an audio interruption and graphics
resume. A missing/evicted match reports status 0, never a different task's
successful check. `LATEST` shows the most recently checked list for comparison;
`SERIAL` identifies the matched check. If the RSP is running audio, the retained
result describes the last graphics task. A preflight rejection still displays
the rejected list immediately.

Page 5 records all 32 scalar RSP registers and the original halted PC. After
the original hardware/IMEM/FIFO snapshot, a 33-instruction program stores the
GPRs to DMEM `0xF00..0xF7F` and executes BREAK. It uses no scratch register,
DMA, or RDP command; the RDP stays frozen. The program is read back before
execution, and its completion wait is bounded. If the RSP cannot halt, has
outstanding DMA, rejects the injected code, or fails to complete, `VALID` is 0.
The CPU still draws the pages. This technique is used only at terminal takeover
and destroys the stopped task's IMEM; resuming the game is not supported.
Normal gameplay still uses the original unmodified graphics microcode.

Page 6 captures the two input entries preceding R26/R27 and the entry at those
input cursors. Its RDRAM reads use the uncached alias and installed-RAM bounds;
DMEM reads stay in GoldenEye's `0x6A0..0x7DF` input buffer. `VALID` bits 0..2
mark the three RDRAM rows, bits 8..10 the DMEM rows (`0x707` means all six rows).
Rows without a valid bit must be ignored. R18/R19 show the pending FIFO transfer
size and destination when the RSP is stopped in the observed wait loop. R28 is
the remaining input byte count. For other stop locations these registers may
have other temporary uses; their labels are not universal task-state fields.

DMEM and RDRAM input words can differ legitimately: TRI4 rewrites its buffered
command, and input DMA can be ahead of the current instruction. A mismatch
alone does not prove a cache bug. RSP input progress is also ahead of RDP
execution. These captures are intended to locate the producer's code/list and
check its FIFO request, not to declare that its latest command caused the stall.

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
| `SPSTAT`, `DPSTAT`, `PISTAT`, `DP CUR/END` | Hardware status and command addresses captured **before** the diagnostic halts/freezes graphics. |
| `SP PC` | Program counter read **after confirming RSP halt**. `FFFFFFFF` means halt was not confirmed. Diagnostic 01 read this register while the RSP could still be running; that value must not be treated as a reliable stopped instruction. |
| `ROM`, `RAM`, `SIZE`, `DMA WAIT` | Most recent synchronous ROM-copy request. WAIT is 1 after submission begins and 0 after its completion is received. This may be a pager read; audio's separate sample-DMA path is not included. |
| `BANK`, `ALLOC`, `POOL POS/END` | Most recent memory-pool allocation/resize request. ALLOC is requested bytes (new total size for resize), not necessarily a failed allocation. |

Thread IDs used by GUD include 2 scheduler, 3 main/game, 4 audio, 5 TLB pager,
and 6 this diagnostic watchdog. Exception code is `(CAUSE >> 2) & 31`. PC and
RA must be resolved against the map/ELF from this build, since instrumentation
changes code addresses. A branch-delay exception is indicated by CAUSE bit 31.

Page 2 captures the halted RSP. `HALTED` confirms the halt bit; `SP MEM OK`
also requires no outstanding SP DMA. A bounded poll permits the report to
continue if either condition never arrives. IMEM words and graphics DMEM
fields are unavailable unless `SP MEM OK` is 1. The eight instruction words
span PC minus 16 bytes through PC plus 12 bytes, wrapping within IMEM; they
identify the actual loaded instructions, including overlays or corruption.

`TYPE`, `UCODE`, `GDL` and `GDL SIZE` describe the most recent task prepared
for RSP start: type 1 is graphics, type 2 audio. `FIFO BASE/END` retain the
most recent graphics output buffer even if audio has since started. For a
graphics task, `WRITE`, `DMEM S/E` and `MODE HI/LO` read GoldenEye microcode's
output cursor, FIFO bounds and saved other-mode words at DMEM offsets 0x128,
0x150/0x154 and 0x118/0x11C. These fields are not decoded as graphics state
for an audio task. `MAIN A0/TLB A0` are saved registers; at the `osRecvMesg`
wait point they do not reliably identify the queue: `__osEnqueueAndYield`
saves callee-saved registers, but does not refresh A0. These can be stale.

Page 3 contains sixteen raw 64-bit entries from DP CURRENT minus 112 bytes
through DP CURRENT plus 8 bytes. The row at CURRENT is row 14, counting from
zero. `VALID` bit N says whether row N was read; unavailable rows show
`FFFFFFFF FFFFFFFF`. Reads use uncached RDRAM and stay inside the recorded
graphics FIFO and installed RAM. An XBUS command source is not read as RDRAM.
The window does not wrap across the FIFO boundary. CURRENT can be ahead of
the command currently executing, entries can be coefficients rather than
command headers, and a pending ring-buffer wrap can put END below CURRENT.
This is evidence to decode, not identification of an offending command.

## Findings from the Diagnostic 02 capture

The three September 12 photos and matching `GUD(1).elf` / `GUD(1).map`
confirm a graphics-progress stall on Cradle, rather than a reported CPU exception.
The saved CPU PC `0x7000BA98` is again `osRecvMesg + 0x68`.

The halted RSP PC `0x1B8` contains `mfc0 $23, DPC_CURRENT`. The eight captured
instructions match the supplied ELF's graphics microcode. This is the FIFO
space-check loop, which prevents the RSP overwriting commands still needed
by the RDP. FIFO base/end are `0x802BDCA0` / `0x802C7CA0` (40 KiB), its write
cursor is `0x002BE658`, and DP CURRENT is `0x002BE690`. The RSP needs more
space before it can submit its next block. The apparent reversal of CURRENT
and END can occur during circular-buffer wrap and is not by itself a bad bound.

This identifies where graphics progress stops, not the command that triggered
it. The raw window is not a command trace: commands have varying lengths,
the RDP prefetches, and FIFO memory is reused. Triangle coefficients can look
like opcodes, and older commands may already have been overwritten. Neither
these photos nor a static ELF prove which RDP command caused the stall.

## Diagnostic 03: check commands before submission

Before each graphics task is queued, `n64DiagCheckGfx` walks the CPU-built GBI
stream, including pushed/branched display lists and segment-address changes.
It checks selected RDP hardware hazards: unsafe fill/copy state, 4-bit tile or
palette loads, reversed palette indices, and the misaligned texture-load
condition. It also checks list/RSP DMA addresses, other-mode masks, and the
length/termination of raw sky triangles and texture rectangles. It reads
through the CPU cache because the display lists have not yet been written back.
It does not insert rendering commands or change the scheduler's completion rules.

If a hazard is found, graphics submission stops and page 4 appears immediately.
It gives the reason, physical command address, two GBI words, enclosing call-site
address, mode words, and texture/framebuffer information. `CHECK GDL` identifies
the inspected master list. In Diagnostic 04 the saved result is matched to the
running graphics task, as described above. The page remains CPU preflight state,
not a new RDP-register snapshot.
Texture and framebuffer sizes use RDP encoding: 0=4b, 1=8b, 2=16b, 3=32b.
`FFFFFFFF` means the checker has not established that field.

| Status | Meaning |
|---|---|
| 0 | No matching completed check for the running graphics task. |
| 1 | Reached the list end without finding the hazards implemented here. |
| 2 | Partial inspection: unknown/conditional control flow or the bounded walk limit. The task is still submitted. |
| 3 | Potential hardware hazard or malformed input; stopped before submission. |

A status of 1 is not proof of valid rendering. This is not a full RSP emulator
or RDP validator. It does not calculate visibility, triangle coefficients,
texture memory contents, synchronization correctness, or cache coherency. A
hazard detected at an input triangle might have been avoided by RSP culling;
confirm the reported command before calling it the original crash cause.
The walk has a 200,000-command budget and a 16-entry call stack. CULLDL and
unsupported commands stop inspection instead of inventing a branch outcome.
This adds CPU overhead and changes memory layout, so it is only for diagnosis.

The fill/copy restrictions are described in Nintendo's
[gDPSetCycleType manual](https://ultra64.ca/files/documentation/online-manuals/man/n64man/gdp/gDPSetCycleType.html).
The additional load and copy-scissor checks follow the hardware conditions
recorded by [libdragon's RDP validator](https://github.com/DragonMinded/libdragon/blob/c4a7e119eff1cfad07adcfa892a2910c40d8bdb8/src/rdpq/rdpq_debug.c).
Only a subset is implemented here; libdragon itself is not linked into GUD.

## Findings from the first US hardware capture

The supplied ELF/map resolve both saved CPU PCs, `0x7000B258`, to
`osRecvMesg + 0x68`, immediately after its `__osEnqueueAndYield` call. This
is consistent with the game waiting for graphics messages and the pager
waiting for faults. It is not a CPU exception location.

The reported stage was Cradle (0x29), phase WAIT FRAME, SP status 0xC0 and
DP status 0x7C0. These support investigating graphics completion, but are
only one hardware snapshot. The initial graphics microcode image maps the
reported running SP PC of 0x300 to an AND in the other-mode update handler,
not a wait loop. Because Diagnostic 01 did not halt before reading PC, this
does not establish that the RSP was stuck there. Diagnostic 02 fixes that
capture ordering and collects live instructions and FIFO data that an ELF
cannot supply. The remaining game defect has not yet been identified.

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
- The terminal renderer disables interrupts, captures hardware status, halts
  the RSP, freezes RDP command processing, then captures halted PC/instructions
  and a bounded raw FIFO window before drawing through an uncached alias
  of the existing framebuffer. It programs a base 320-pixel VI mode directly,
  so a stopped scheduler or black loading screen does not prevent display.
- It cannot guarantee recovery from disabled-interrupt hangs, corrupted
  resident code/data, or hardware/bus failures that prevent its own execution.
  A long legitimate operation with no checkpoints could also time out.
- The two diagnostic modules total about 19 KiB of resident code/data including
  a 4 KiB watchdog stack, plus hooks (about 3.2 KiB more than Diagnostic 03).
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
metadata, framebuffer bounds on all six pages, and the terminal graphics-stop
sequence. They also check halted-PC ordering, unavailable SP memory, captured
IMEM/FIFO contents, preservation of graphics FIFO metadata across audio tasks,
and rejection of FIFO reads outside the buffer/installed RAM or in XBUS mode.
Diagnostic 04 also tests task matching with queued frames, audio/resume,
pointer reuse, differing sizes and record eviction. A host model executes the
injected SW/BREAK words, verifies GPR preservation and bounded DMEM writes, and
exercises code-readback failure, timeout, refused halt, busy DMA and input bounds.
This is not a hardware verification of the register dumper.
The preflight tests cover unsafe and legal fill/copy modes, texture-load
conditions, segmented pushed/branched lists, DMA bounds, sky-triangle and
texture-rectangle framing, missing terminators, partial inspection and a
self-branching list hitting the command budget. An optional PPM output path
produces previews using simulated values.
The submit-hook test verifies that an invalid master list goes directly to
the preflight page. The three static pipeline/geometry lists extracted from
the supplied ELF also pass the checker.

Diagnostic 04's changed production C unit compiles with IDO 5.3 for US. With
`N64_LOAD_DIAGNOSTICS=0` it also compiles without diagnostic function symbols.
Linker lists explicitly place the diagnostic text/data/rodata/BSS in resident
sections. A complete ROM build and real-console validation still need the
extracted assets and hardware that are not available in this workspace.
