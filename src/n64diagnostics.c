#include "n64diagnostics.h"

#if N64_LOAD_DIAGNOSTICS
#include <PR/rcp.h>
#include "n64rdpcheck.h"

#define DIAG_WIDTH 320
#define DIAG_HEIGHT 240
#define DIAG_TIMEOUT_TICKS 468750000U /* Ten seconds at standard N64 Count rate. */
#define DIAG_THREAD_ID 6
#define DIAG_THREAD_PRIORITY 60 /* Above game, audio, scheduler and TLB pager. */
#define DIAG_PAGE_TICKS 375000000U /* Eight seconds per photograph. */
#define DIAG_RDP_WORDS 32
#define DIAG_PAGES 6
#define DIAG_CHECK_SLOTS 4

extern u8 cfb_16[][DIAG_WIDTH * DIAG_HEIGHT * 2];
extern OSThread mainThread;
extern OSThread g_tlbThread;
extern OSThread *__osGetCurrFaultedThread(void);

/* All code, strings, font data and state stay in the resident segment.
 * The fatal screen must never need the game-code pager, ROM I/O or RSP. */
static OSThread g_DiagThread;
static u64 g_DiagStack[0x1000 / sizeof(u64)];
static OSTimer g_DiagTimer;
static OSMesgQueue g_DiagQueue;
static OSMesg g_DiagMessage;
static volatile struct {
    u32 enabled;
    u32 stage;
    u32 phase;
    u32 lastProgress;
    u32 steps;
    u32 dmaSource;
    u32 dmaTarget;
    u32 dmaSize;
    s32 dmaResult;
    u32 dmaWaiting;
    u32 bank;
    u32 bytes;
    u32 allocPos;
    u32 allocEnd;
    u32 taskType;
    u32 taskUcode;
    u32 taskList;
    u32 taskSize;
    u32 fifoStart;
    u32 fifoEnd;
} g_Diag;

/* Filled only at terminal takeover, before touching the framebuffer. */
static struct {
    u32 spStatus, dpStatus, piStatus, dpStart, dpCurrent, dpEnd;
    u32 spPost, spPc, halted, spMemoryReady;
    u32 imemAddress[8], imemWord[8];
    u32 fifoWrite, dmemStart, dmemEnd, modeHi, modeLo;
    u32 rdpAddress[DIAG_RDP_WORDS / 2];
    u32 rdpWord[DIAG_RDP_WORDS];
    u32 rdpValid;
    u32 registersValid, registers[32];
    u32 inputValid, inputSpAddress[3], inputRamAddress[3];
    u32 inputSpWord[6], inputRamWord[6];
} g_DiagStop;

static N64RdpCheck g_DiagCheck;
static u32 g_DiagCheckList;
static struct {
    u32 list, size, serial;
    N64RdpCheck result;
} g_DiagChecks[DIAG_CHECK_SLOTS];
static u32 g_DiagCheckNext, g_DiagCheckSerial;
static N64RdpCheck g_DiagRunningCheck;
static u32 g_DiagRunningList, g_DiagRunningSerial;

static const char *g_DiagPhases[] = {
    "BOOT", "STAGE BEGIN", "POOLS", "PLAYERS", "GFX MEMORY", "STAGE LOAD",
    "TEXTURES", "FONTS", "BG HEADER", "BG DATA", "STAN", "BG PROCESS",
    "STAGE STATE", "GUARDS", "SETUP", "FX", "PATHS", "BOND", "INTRO",
    "VIDEO", "WAIT FRAME", "TICK", "VIEW", "RENDER", "SUBMIT", "UNLOAD"
};
typedef char DiagPhaseNamesMatch[
    sizeof(g_DiagPhases) / sizeof(g_DiagPhases[0]) == N64DIAG_PHASE_COUNT ? 1 : -1];

/* Five columns, seven rows per character. Digits, uppercase letters, colon. */
static const char g_DiagAlphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:";
static const u8 g_DiagFont[][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
    {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
    {14,17,17,15,1,1,14},
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
    {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
    {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
    {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
    {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}, {0,4,4,0,4,4,0}
};

static void n64DiagText(volatile u16 *fb, s32 x, s32 y, const char *text)
{
    s32 glyph, row, col;
    while (*text) {
        for (glyph = 0; g_DiagAlphabet[glyph]; glyph++) {
            if (*text == g_DiagAlphabet[glyph]) {
                for (row = 0; row < 7; row++) {
                    for (col = 0; col < 5; col++) {
                        if ((g_DiagFont[glyph][row] & (16 >> col))
                                && x + col >= 0 && x + col < DIAG_WIDTH
                                && y + row >= 0 && y + row < DIAG_HEIGHT) {
                            fb[(y + row) * DIAG_WIDTH + x + col] = 0xffff;
                        }
                    }
                }
                break;
            }
        }
        x += 6;
        text++;
    }
}

static void n64DiagHex(volatile u16 *fb, s32 x, s32 y, u32 value)
{
    char text[9];
    s32 i;
    for (i = 7; i >= 0; i--) {
        text[i] = "0123456789ABCDEF"[value & 15];
        value >>= 4;
    }
    text[8] = 0;
    n64DiagText(fb, x, y, text);
}

static void n64DiagPair(volatile u16 *fb, s32 y, const char *a, u32 av,
        const char *b, u32 bv)
{
    n64DiagText(fb, 24, y, a);
    n64DiagHex(fb, 80, y, av);
    n64DiagText(fb, 164, y, b);
    n64DiagHex(fb, 224, y, bv);
}

static s32 n64DiagRamRange(u32 address, u32 size)
{
    u32 physical = address & 0x1fffffff;
    u32 limit = osMemSize < 0x800000 ? osMemSize : 0x800000;
    return (address >> 29 == 4 || address >> 29 == 5)
        && physical >= 0x400 && size <= limit && physical <= limit - size;
}

static void n64DiagCaptureRegisters(void)
{
    u32 i, instruction, status, spAddress, ramAddress;

    if (!g_DiagStop.spMemoryReady) return;

    /* Terminal takeover only. The original PC, IMEM window, modes and FIFO
     * have already been saved. This tiny program preserves every scalar
     * register, writes only DMEM F00..F7F, and issues no DMA or RDP commands.
     * Store instructions use $zero as their base, so no scratch GPR is needed.
     * The game cannot resume after this; normal graphics microcode is unchanged.
     */
    for (i = 0; i < 32; i++) {
        instruction = 0xac000000U | (i << 16) | (0xf00 + i * 4);
        IO_WRITE(SP_IMEM_START + i * 4, instruction);
        if (IO_READ(SP_IMEM_START + i * 4) != instruction) return;
    }
    IO_WRITE(SP_IMEM_START + 128, 0x0000000d); /* break */
    if (IO_READ(SP_IMEM_START + 128) != 0x0000000d) return;
    IO_WRITE(SP_PC_REG, 0);
    IO_WRITE(SP_STATUS_REG, SP_CLR_HALT | SP_CLR_BROKE
            | SP_CLR_SSTEP | SP_CLR_INTR_BREAK);
    for (i = 0; i < 65536; i++) {
        status = IO_READ(SP_STATUS_REG);
        if ((status & (SP_STATUS_HALT | SP_STATUS_BROKE))
                == (SP_STATUS_HALT | SP_STATUS_BROKE)) break;
    }
    IO_WRITE(SP_STATUS_REG, SP_SET_HALT);
    if (i == 65536 || (status & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL))) return;
    for (i = 0; i < 32; i++) {
        g_DiagStop.registers[i] = IO_READ(SP_DMEM_START + 0xf00 + i * 4);
    }
    g_DiagStop.registersValid = 1;
    if (g_Diag.taskType != 1) return;

    /* GoldenEye uses R26/R27 as its DRAM/DMEM input cursors. Capture two
     * preceding entries and the cursor entry, without assuming which command
     * is responsible. TRI4 legitimately rewrites its buffered DMEM command.
     * RSP progress can be many primitives ahead of the stalled RDP.
     */
    for (i = 0; i < 3; i++) {
        spAddress = g_DiagStop.registers[27] - 16 + i * 8;
        ramAddress = (g_DiagStop.registers[26] & 0x1fffffff) - 16 + i * 8;
        g_DiagStop.inputSpAddress[i] = spAddress;
        g_DiagStop.inputRamAddress[i] = ramAddress;
        if (!(spAddress & 7) && spAddress >= 0x6a0 && spAddress <= 0x7d8) {
            g_DiagStop.inputSpWord[i * 2] = IO_READ(SP_DMEM_START + spAddress);
            g_DiagStop.inputSpWord[i * 2 + 1] = IO_READ(SP_DMEM_START + spAddress + 4);
            g_DiagStop.inputValid |= 1U << (i + 8);
        }
        if (!(ramAddress & 7) && n64DiagRamRange(ramAddress | 0x80000000, 8)) {
            g_DiagStop.inputRamWord[i * 2] = IO_READ(ramAddress);
            g_DiagStop.inputRamWord[i * 2 + 1] = IO_READ(ramAddress + 4);
            g_DiagStop.inputValid |= 1U << i;
        }
    }
}

static void n64DiagCapture(void)
{
    u32 i, address, base, end, size;

    g_DiagStop.spStatus = IO_READ(SP_STATUS_REG);
    g_DiagStop.dpStatus = IO_READ(DPC_STATUS_REG);
    g_DiagStop.piStatus = IO_READ(PI_STATUS_REG);
    g_DiagStop.dpStart = IO_READ(DPC_START_REG);
    g_DiagStop.dpCurrent = IO_READ(DPC_CURRENT_REG);
    g_DiagStop.dpEnd = IO_READ(DPC_END_REG);
    IO_WRITE(SP_STATUS_REG, SP_SET_HALT);
    IO_WRITE(DPC_STATUS_REG, DPC_SET_FREEZE);

    /* PC and SP memory need a halted RSP. Also wait for outstanding SP DMA
     * before reading IMEM/DMEM. Bound the wait so this screen cannot hang. */
    g_DiagStop.spPc = 0xffffffff;
    for (i = 0; i < 65536; i++) {
        g_DiagStop.spPost = IO_READ(SP_STATUS_REG);
        if ((g_DiagStop.spPost & (SP_STATUS_HALT | SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL))
                == SP_STATUS_HALT) {
            break;
        }
    }
    g_DiagStop.halted = !!(g_DiagStop.spPost & SP_STATUS_HALT);
    g_DiagStop.spMemoryReady = g_DiagStop.halted
        && !(g_DiagStop.spPost & (SP_STATUS_DMA_BUSY | SP_STATUS_DMA_FULL));
    if (g_DiagStop.halted) {
        g_DiagStop.spPc = IO_READ(SP_PC_REG) & 0xffc;
    }
    if (g_DiagStop.spMemoryReady) {
        for (i = 0; i < 8; i++) {
            address = (g_DiagStop.spPc - 16 + i * 4) & 0xffc;
            g_DiagStop.imemAddress[i] = address;
            g_DiagStop.imemWord[i] = IO_READ(SP_IMEM_START + address);
        }
        /* GoldenEye graphics microcode's state at DMEM 0x110. These offsets
         * were checked against the microcode in the supplied US GUD.elf. */
        if (g_Diag.taskType == 1) {
            g_DiagStop.modeHi = IO_READ(SP_DMEM_START + 0x118);
            g_DiagStop.modeLo = IO_READ(SP_DMEM_START + 0x11c);
            g_DiagStop.fifoWrite = IO_READ(SP_DMEM_START + 0x128);
            g_DiagStop.dmemStart = IO_READ(SP_DMEM_START + 0x150);
            g_DiagStop.dmemEnd = IO_READ(SP_DMEM_START + 0x154);
        }
    }

    /* Read through KSEG1: CPU cache lines may predate RSP output. This is a
     * raw window, not a claimed command boundary or execution trace. Never
     * follow a corrupt FIFO pointer outside installed RDRAM. */
    base = g_Diag.fifoStart & 0x1fffffff;
    end = g_Diag.fifoEnd & 0x1fffffff;
    size = end - base;
    for (i = 0; i < DIAG_RDP_WORDS / 2; i++) {
        address = (g_DiagStop.dpCurrent & 0xfffff8) - 112 + i * 8;
        g_DiagStop.rdpAddress[i] = address;
        g_DiagStop.rdpWord[i * 2] = 0xffffffff;
        g_DiagStop.rdpWord[i * 2 + 1] = 0xffffffff;
        if (!(g_DiagStop.dpStatus & DPC_STATUS_XBUS_DMEM_DMA)
                && end > base && size >= 8 && !(base & 7) && !(end & 7)
                && n64DiagRamRange(g_Diag.fifoStart, size)
                && address >= base && address <= end - 8) {
            g_DiagStop.rdpWord[i * 2] = IO_READ(address);
            g_DiagStop.rdpWord[i * 2 + 1] = IO_READ(address + 4);
            g_DiagStop.rdpValid |= 1U << i;
        }
    }
    n64DiagCaptureRegisters();
}

static void n64DiagPage(volatile u16 *fb, u32 page, const char *reason, OSThread *fault)
{
    u32 i;
    const N64RdpCheck *check;
    u32 checkList;
    char left[4], right[4];
    for (i = 0; i < DIAG_WIDTH * DIAG_HEIGHT; i++) {
        fb[i] = 0x0001;
    }

    n64DiagText(fb, 24, 16, page == 0 ? "N64 DIAGNOSTIC 04 PAGE 1 OF 6"
            : page == 1 ? "N64 DIAGNOSTIC 04 PAGE 2 OF 6"
            : page == 2 ? "N64 DIAGNOSTIC 04 PAGE 3 OF 6"
            : page == 3 ? "N64 DIAGNOSTIC 04 PAGE 4 OF 6"
            : page == 4 ? "N64 DIAGNOSTIC 04 PAGE 5 OF 6" : "N64 DIAGNOSTIC 04 PAGE 6 OF 6");
    if (page == 4) {
        n64DiagText(fb, 24, 28, "RSP SCALAR REGISTERS AT STALL");
        n64DiagPair(fb, 40, "VALID", g_DiagStop.registersValid, "SP PC", g_DiagStop.spPc);
        for (i = 0; i < 16; i++) {
            left[0] = right[0] = 'R';
            left[1] = '0' + i / 10;
            left[2] = '0' + i % 10;
            right[1] = '0' + (i + 16) / 10;
            right[2] = '0' + (i + 16) % 10;
            left[3] = right[3] = 0;
            n64DiagPair(fb, 54 + i * 10, left, g_DiagStop.registers[i],
                    right, g_DiagStop.registers[i + 16]);
        }
        n64DiagText(fb, 24, 228, "PHOTO ALL SIX PAGES THEN RESET");
        return;
    }
    if (page == 5) {
        n64DiagText(fb, 24, 28, "GOLDENEYE RSP INPUT AT STALL");
        n64DiagPair(fb, 40, "REG OK", g_DiagStop.registersValid, "TYPE", g_Diag.taskType);
        n64DiagPair(fb, 52, "FIFO OUT", g_DiagStop.registers[19], "BYTES", g_DiagStop.registers[18]);
        n64DiagPair(fb, 64, "RAM CUR", g_DiagStop.registers[26], "SP CUR", g_DiagStop.registers[27]);
        n64DiagPair(fb, 76, "REMAIN", g_DiagStop.registers[28], "VALID", g_DiagStop.inputValid);
        n64DiagText(fb, 24, 88, "DMEM INPUT ADDRESS AND WORDS");
        for (i = 0; i < 3; i++) {
            n64DiagHex(fb, 24, 100 + i * 12, g_DiagStop.inputSpAddress[i]);
            n64DiagHex(fb, 96, 100 + i * 12, g_DiagStop.inputSpWord[i * 2]);
            n64DiagHex(fb, 168, 100 + i * 12, g_DiagStop.inputSpWord[i * 2 + 1]);
        }
        n64DiagText(fb, 24, 142, "RDRAM INPUT ADDRESS AND WORDS");
        for (i = 0; i < 3; i++) {
            n64DiagHex(fb, 24, 154 + i * 12, g_DiagStop.inputRamAddress[i]);
            n64DiagHex(fb, 96, 154 + i * 12, g_DiagStop.inputRamWord[i * 2]);
            n64DiagHex(fb, 168, 154 + i * 12, g_DiagStop.inputRamWord[i * 2 + 1]);
        }
        n64DiagText(fb, 24, 202, "TRI4 MAY REWRITE DMEM WORDS");
        n64DiagText(fb, 24, 214, "RSP POSITION IS AHEAD OF RDP");
        n64DiagText(fb, 24, 228, "PHOTO ALL SIX PAGES THEN RESET");
        return;
    }
    if (page == 3) {
        check = g_DiagCheck.status == N64RDP_HAZARD ? &g_DiagCheck : &g_DiagRunningCheck;
        checkList = check == &g_DiagCheck ? g_DiagCheckList : g_DiagRunningList;
        n64DiagText(fb, 24, 28, check == &g_DiagCheck ? "REJECTED GBI PREFLIGHT" : "RUNNING GFX TASK PREFLIGHT");
        n64DiagText(fb, 24, 40, check->reason ? check->reason : "NO MATCHING TASK CHECK");
        n64DiagPair(fb, 64, "STATUS", check->status, "COMMANDS", check->commands);
        n64DiagPair(fb, 76, "CHECK GDL", checkList, "COMMAND", check->address);
        n64DiagPair(fb, 88, "WORD 0", check->word0, "WORD 1", check->word1);
        n64DiagPair(fb, 100, "CALLER", check->caller, "TEX ADDR", check->texture);
        n64DiagPair(fb, 112, "MODE HI", check->modeHi, "MODE LO", check->modeLo);
        n64DiagPair(fb, 124, "TEX SIZE", check->textureSize, "FB SIZE", check->colorSize);
        n64DiagPair(fb, 136, "CLIP X0", check->scissorLeft, "RSP GDL", g_Diag.taskList);
        n64DiagPair(fb, 148, "LATEST", g_DiagCheckList, "SERIAL", g_DiagRunningSerial);
        n64DiagText(fb, 24, 160, "1 CHECKED  2 PARTIAL  3 HAZARD");
        n64DiagText(fb, 24, 172, "CHECKED DOES NOT PROVE RDP SAFE");
        n64DiagText(fb, 24, 184, "LATEST MAY BE A LATER QUEUED LIST");
        n64DiagText(fb, 24, 222, "PHOTO ALL SIX PAGES THEN RESET");
        return;
    }
    if (page == 1) {
        n64DiagText(fb, 24, 28, "RSP SNAPSHOT AFTER HALT");
        n64DiagPair(fb, 40, "HALTED", g_DiagStop.halted, "SP MEM OK", g_DiagStop.spMemoryReady);
        n64DiagPair(fb, 52, "SP PC", g_DiagStop.spPc, "TYPE", g_Diag.taskType);
        n64DiagPair(fb, 64, "UCODE", g_Diag.taskUcode, "GDL", g_Diag.taskList);
        n64DiagPair(fb, 76, "GDL SIZE", g_Diag.taskSize, "FIFO BASE", g_Diag.fifoStart);
        n64DiagPair(fb, 88, "FIFO END", g_Diag.fifoEnd, "WRITE", g_DiagStop.fifoWrite);
        n64DiagPair(fb, 100, "DMEM S", g_DiagStop.dmemStart, "DMEM E", g_DiagStop.dmemEnd);
        n64DiagPair(fb, 112, "MODE HI", g_DiagStop.modeHi, "MODE LO", g_DiagStop.modeLo);
        n64DiagPair(fb, 124, "MAIN A0", (u32)mainThread.context.a0, "TLB A0", (u32)g_tlbThread.context.a0);
        n64DiagText(fb, 24, 136, "IMEM ADDRESS AND INSTRUCTION WORD");
        for (i = 0; i < 4; i++) {
            n64DiagHex(fb, 24, 148 + i * 12, g_DiagStop.imemAddress[i * 2]);
            n64DiagHex(fb, 80, 148 + i * 12, g_DiagStop.imemWord[i * 2]);
            n64DiagHex(fb, 164, 148 + i * 12, g_DiagStop.imemAddress[i * 2 + 1]);
            n64DiagHex(fb, 224, 148 + i * 12, g_DiagStop.imemWord[i * 2 + 1]);
        }
        n64DiagPair(fb, 196, "SP POST", g_DiagStop.spPost, "DP START", g_DiagStop.dpStart);
        n64DiagText(fb, 24, 222, "PHOTO ALL SIX PAGES THEN RESET");
        return;
    }
    if (page == 2) {
        n64DiagText(fb, 24, 28, "RDP RAW WINDOW AROUND DP CURRENT");
        n64DiagPair(fb, 40, "DP CUR", g_DiagStop.dpCurrent, "VALID", g_DiagStop.rdpValid);
        n64DiagText(fb, 24, 52, "ADDRESS     WORD 0      WORD 1");
        for (i = 0; i < DIAG_RDP_WORDS / 2; i++) {
            n64DiagHex(fb, 24, 64 + i * 10, g_DiagStop.rdpAddress[i]);
            n64DiagHex(fb, 96, 64 + i * 10, g_DiagStop.rdpWord[i * 2]);
            n64DiagHex(fb, 168, 64 + i * 10, g_DiagStop.rdpWord[i * 2 + 1]);
        }
        n64DiagText(fb, 24, 228, "PHOTO ALL SIX PAGES THEN RESET");
        return;
    }
    n64DiagText(fb, 24, 28, reason);
    n64DiagText(fb, 24, 40, "PHASE:");
    n64DiagText(fb, 72, 40, g_Diag.phase < N64DIAG_PHASE_COUNT
            ? g_DiagPhases[g_Diag.phase] : "UNKNOWN");
    n64DiagPair(fb, 52, "STAGE", g_Diag.stage, "STEPS", g_Diag.steps);
    n64DiagPair(fb, 64, "THREAD", fault ? (u32)fault->id : 0xffffffff,
            "CAUSE", fault ? fault->context.cause : 0);
    n64DiagPair(fb, 76, "PC", fault ? fault->context.pc : 0,
            "RA", fault ? (u32)fault->context.ra : 0);
    n64DiagPair(fb, 88, "BADVADDR", fault ? fault->context.badvaddr : 0,
            "FPCSR", fault ? fault->context.fpcsr : 0);
    n64DiagPair(fb, 100, "SP", fault ? (u32)fault->context.sp : 0,
            "SR", fault ? fault->context.sr : 0);
    n64DiagPair(fb, 112, "MAIN PC", mainThread.context.pc,
            "MAIN RA", (u32)mainThread.context.ra);
    n64DiagPair(fb, 124, "TLB PC", g_tlbThread.context.pc,
            "TLB RA", (u32)g_tlbThread.context.ra);
    n64DiagPair(fb, 136, "SPSTAT", g_DiagStop.spStatus, "DPSTAT", g_DiagStop.dpStatus);
    n64DiagPair(fb, 148, "PISTAT", g_DiagStop.piStatus, "SP PC", g_DiagStop.spPc);
    n64DiagPair(fb, 160, "DP CUR", g_DiagStop.dpCurrent, "DP END", g_DiagStop.dpEnd);
    n64DiagPair(fb, 172, "ROM", g_Diag.dmaSource, "SIZE", g_Diag.dmaSize);
    n64DiagPair(fb, 184, "RAM", g_Diag.dmaTarget, "DMA WAIT", g_Diag.dmaWaiting);
    n64DiagPair(fb, 196, "BANK", g_Diag.bank, "ALLOC", g_Diag.bytes);
    n64DiagPair(fb, 208, "POOL POS", g_Diag.allocPos, "POOL END", g_Diag.allocEnd);
    n64DiagText(fb, 24, 222, "PHOTO ALL SIX PAGES THEN RESET");
}

static void n64DiagStop(const char *reason, OSThread *fault)
{
    volatile u16 *fb;
    OSViMode *mode;
    u32 page = g_DiagCheck.status == N64RDP_HAZARD ? 3 : 0, pageStart;

    /* Terminal takeover only. Interrupts stay disabled. No game code, OS
     * messages, controller I/O, or RCP task is needed to display the pages. */
    osSetIntMask(OS_IM_NONE);
    n64DiagCapture();
    IO_WRITE(VI_CONTROL_REG, 0);
    osInvalDCache(cfb_16, DIAG_WIDTH * DIAG_HEIGHT * 2);
    fb = (volatile u16 *)PHYS_TO_K1(K0_TO_PHYS(cfb_16));
    n64DiagPage(fb, page, reason, fault);

    /* osTvType uses 0=PAL, 1=NTSC, 2=MPAL in this SDK. */
    mode = &osViModeTable[osTvType == 2 ? OS_VI_MPAL_LAN1
            : osTvType == 0 ? OS_VI_PAL_LAN1 : OS_VI_NTSC_LAN1];
    /* Equivalent base mode programming to __osViSwapContext, without waiting
     * for a VI interrupt or touching any game-owned video state. */
    IO_WRITE(VI_ORIGIN_REG, K0_TO_PHYS(cfb_16) + mode->fldRegs[0].origin);
    IO_WRITE(VI_WIDTH_REG, mode->comRegs.width);
    IO_WRITE(VI_BURST_REG, mode->comRegs.burst);
    IO_WRITE(VI_V_SYNC_REG, mode->comRegs.vSync);
    IO_WRITE(VI_H_SYNC_REG, mode->comRegs.hSync);
    IO_WRITE(VI_LEAP_REG, mode->comRegs.leap);
    IO_WRITE(VI_H_START_REG, mode->comRegs.hStart);
    IO_WRITE(VI_V_START_REG, mode->fldRegs[0].vStart);
    IO_WRITE(VI_V_BURST_REG, mode->fldRegs[0].vBurst);
    IO_WRITE(VI_INTR_REG, mode->fldRegs[0].vIntr);
    IO_WRITE(VI_X_SCALE_REG, mode->comRegs.xScale);
    IO_WRITE(VI_Y_SCALE_REG, mode->fldRegs[0].yScale);
    IO_WRITE(VI_CONTROL_REG, mode->comRegs.ctrl);
    pageStart = osGetCount();
    for (;;) {
        if ((u32)(osGetCount() - pageStart) >= DIAG_PAGE_TICKS) {
            page = (page + 1) % DIAG_PAGES;
            n64DiagPage(fb, page, reason, fault);
            pageStart = osGetCount();
        }
    }
}

void n64DiagTask(u32 type, void *ucode, void *list, u32 size, void *out, void *end)
{
    N64RdpCheck empty = {0};
    u32 i, slot;
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.taskType = type;
    g_Diag.taskUcode = (u32)ucode;
    g_Diag.taskList = (u32)list;
    g_Diag.taskSize = size;
    if (type == 1) { /* Retain the graphics FIFO when audio takes the RSP. */
        g_Diag.fifoStart = (u32)out;
        g_Diag.fifoEnd = (u32)end;
        g_DiagRunningList = (u32)list;
        g_DiagRunningSerial = 0;
        g_DiagRunningCheck = empty;
        g_DiagRunningCheck.reason = "NO MATCHING TASK CHECK";
        /* The main thread can preflight another frame before the current
         * graphics task completes. Match the launched/resumed task itself.
         */
        for (i = 0; i < DIAG_CHECK_SLOTS; i++) {
            slot = (g_DiagCheckNext - 1 - i) & (DIAG_CHECK_SLOTS - 1);
            if (g_DiagChecks[slot].serial && g_DiagChecks[slot].list == (u32)list
                    && g_DiagChecks[slot].size == size) {
                g_DiagRunningCheck = g_DiagChecks[slot].result;
                g_DiagRunningSerial = g_DiagChecks[slot].serial;
                break;
            }
        }
    }
    osSetIntMask(mask);
}

void n64DiagFault(OSThread *thread)
{
    n64DiagStop("CPU FAULT", thread);
}

static s32 n64DiagReadGbi(u32 address, u32 *word0, u32 *word1)
{
    u32 *words;
    if (!n64DiagRamRange(address | 0x80000000, 8)) return FALSE;
    /* These commands were just built by the CPU. Reading uncached here
     * would inspect stale memory before rspGfxTaskStart writes back cache. */
    words = (u32 *)(address | 0x80000000);
    *word0 = words[0];
    *word1 = words[1];
    return TRUE;
}

static void n64DiagSaveCheck(void *list, u32 size, const N64RdpCheck *result)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_DiagCheck = *result;
    g_DiagCheckList = (u32)list;
    if (++g_DiagCheckSerial == 0) g_DiagCheckSerial = 1;
    g_DiagChecks[g_DiagCheckNext].list = (u32)list;
    g_DiagChecks[g_DiagCheckNext].size = size;
    g_DiagChecks[g_DiagCheckNext].serial = g_DiagCheckSerial;
    g_DiagChecks[g_DiagCheckNext].result = *result;
    g_DiagCheckNext = (g_DiagCheckNext + 1) & (DIAG_CHECK_SLOTS - 1);
    osSetIntMask(mask);
}

void n64DiagCheckGfx(void *list, u32 size)
{
    N64RdpCheck result;
    n64RdpCheckList((u32)list & 0x1fffffff, size,
            osMemSize < 0x800000 ? osMemSize : 0x800000,
            n64DiagReadGbi, &result);
    n64DiagSaveCheck(list, size, &result);
    if (result.status == N64RDP_HAZARD) {
        n64DiagStop("GBI PREFLIGHT HAZARD", NULL);
    }
}

void n64DiagStep(enum N64DiagPhase phase)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.phase = phase;
    g_Diag.lastProgress = osGetCount();
    g_Diag.steps++;
    osSetIntMask(mask);
}

void n64DiagStage(s32 stage)
{
    g_Diag.stage = stage;
    n64DiagStep(N64DIAG_STAGE_BEGIN);
}

void n64DiagDmaStart(void *target, void *source, u32 size)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.dmaSource = (u32)source;
    g_Diag.dmaTarget = (u32)target;
    g_Diag.dmaSize = size;
    g_Diag.dmaWaiting = 1;
    g_Diag.dmaResult = 0;
    osSetIntMask(mask);
    if (((u32)target & 7) || ((u32)source & 1) || !size || (size & 1)
            || size > 0x1000000) {
        n64DiagStop("INVALID ROM DMA", NULL);
    }
}

void n64DiagDmaResult(s32 result)
{
    g_Diag.dmaResult = result;
    if (result != 0) {
        n64DiagStop("ROM DMA QUEUE FAILED", NULL);
    }
}

void n64DiagDmaDone(void)
{
    g_Diag.dmaWaiting = 0;
}

void n64DiagAllocation(u32 bank, u32 bytes, void *pos, void *end)
{
    OSIntMask mask = osSetIntMask(OS_IM_NONE);
    g_Diag.bank = bank;
    g_Diag.bytes = bytes;
    g_Diag.allocPos = (u32)pos;
    g_Diag.allocEnd = (u32)end;
    osSetIntMask(mask);
}

static void n64DiagPoll(u32 now)
{
    OSThread *fault = __osGetCurrFaultedThread();
    /* Normal game-code TLB misses are handled by crashMain. A fault in the
     * pager itself cannot be handled by that same stopped thread. */
    if (fault && (fault->flags & OS_FLAG_FAULT)
            && (fault == &g_tlbThread || (fault->context.cause & 0x7c) != 8
                || (fault->context.badvaddr & 0xffc00000) != 0x7f000000)) {
        n64DiagFault(fault);
    }
    if (g_Diag.enabled && (u32)(now - g_Diag.lastProgress) >= DIAG_TIMEOUT_TICKS) {
        n64DiagStop("NO MAIN PROGRESS FOR 10 SECONDS", NULL);
    }
}

static void n64DiagMain(void *arg)
{
    for (;;) {
        osSetTimer(&g_DiagTimer, OS_USEC_TO_CYCLES(250000), 0,
                &g_DiagQueue, (OSMesg)1);
        osRecvMesg(&g_DiagQueue, NULL, OS_MESG_BLOCK);
        n64DiagPoll(osGetCount());
    }
}

void n64DiagInit(void)
{
    g_Diag.enabled = TRUE;
    n64DiagStep(N64DIAG_BOOT);
    osCreateMesgQueue(&g_DiagQueue, &g_DiagMessage, 1);
    osCreateThread(&g_DiagThread, DIAG_THREAD_ID, n64DiagMain, NULL,
            g_DiagStack + sizeof(g_DiagStack) / sizeof(g_DiagStack[0]),
            DIAG_THREAD_PRIORITY);
    osStartThread(&g_DiagThread);
}
#endif
