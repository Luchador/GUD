#include "n64diagnostics.h"
#if N64_LOAD_DIAGNOSTICS
#include "n64rdpcheck.h"

/* Preflight for GoldenEye's original GBI, not F3DEX/F3DEX2. This walks CPU
 * display lists; it cannot validate RSP-generated triangle coefficients,
 * cache coherency or the exact RDP state at an eventual hardware stall.
 * Unknown control flow stops the walk instead of guessing a branch outcome.
 */
#define CHECK_LIMIT 200000U
#define CHECK_DEPTH 16

static u32 checkAddress(u32 address, u32 *segments, u32 known)
{
    u32 segment = (address >> 24) & 15;
    if (!(known & (1U << segment))) return 0xffffffff;
    return (address & 0xffffff) + segments[segment];
}

static s32 checkRange(u32 address, u32 bytes, u32 limit)
{
    return address >= 0x400 && address < limit && bytes <= limit - address;
}

static const char *checkDraw(N64RdpCheck *r, u32 hiKnown, u32 loKnown)
{
    u32 cycle;
    if ((hiKnown & 0x300000) != 0x300000) return NULL;
    cycle = (r->modeHi >> 20) & 3;
    if (cycle == 3) {
        if (r->colorSize == 0) return "FILL ON 4 BIT FRAMEBUFFER";
        if (r->modeLo & loKnown & 0x40) return "FILL WITH IMAGE READ";
        if (r->modeLo & loKnown & 0x10) return "FILL WITH Z COMPARE";
        if ((loKnown & 0x24) == 0x24 && (r->modeLo & 0x24) == 0x20)
            return "FILL WITH PIXEL Z UPDATE";
    }
    if (cycle == 2) {
        if (r->colorSize == 3) return "COPY ON 32 BIT FRAMEBUFFER";
        if (r->scissorLeft != 0xffffffff && r->scissorLeft != 0)
            return "COPY WITH LEFT SCISSOR";
    }
    return NULL;
}

void n64RdpCheckList(u32 start, u32 bytes, u32 ramSize,
        N64RdpRead read, N64RdpCheck *r)
{
    u32 segments[16], returns[CHECK_DEPTH], callers[CHECK_DEPTH];
    u32 segmentKnown = 0, hiKnown = 0, loKnown = 0;
    u32 pc = start, end = start + bytes, depth = 0;
    u32 op, w0, w1, address, size, shift, mask, pixels;
    u32 halves = 0, halfWords = 0, rectWords = 0;
    const char *reason;

    r->status = N64RDP_PARTIAL;
    r->commands = r->address = r->word0 = r->word1 = r->caller = 0;
    r->modeHi = r->modeLo = 0;
    r->texture = r->textureSize = r->colorSize = r->scissorLeft = 0xffffffff;
    r->reason = "COMMAND LIMIT";
    if ((start & 7) || !bytes || (bytes & 7) || !checkRange(start, bytes, ramSize)) {
        r->address = start;
        r->reason = "INVALID MASTER LIST";
        r->status = N64RDP_HAZARD;
        return;
    }

    for (;;) {
        if (r->commands == CHECK_LIMIT) return;
        r->address = pc;
        r->caller = depth ? callers[depth - 1] : 0;
        if (!depth && (pc < start || pc >= end)) {
            r->word0 = r->word1 = 0;
            r->reason = "MASTER LIST WITHOUT END";
            break;
        }
        if (pc & 7 || !checkRange(pc, 8, ramSize) || !read(pc, &w0, &w1)) {
            r->word0 = r->word1 = 0;
            r->reason = "INVALID DISPLAY LIST ADDRESS";
            break;
        }
        r->word0 = w0;
        r->word1 = w1;
        r->commands++;
        pc += 8;
        op = w0 >> 24;

        /* Texture rectangle parameters are GBI half commands, not another
         * RDP command. Raw sky triangles end with HALF_2 (0xb3). */
        if (rectWords) {
            if (op != (rectWords == 2 ? 0xb4 : 0xb3)) {
                r->reason = "BROKEN TEXTURE RECTANGLE";
                break;
            }
            rectWords--;
            continue;
        }
        if (halves) {
            if (op != 0xb4 && op != 0xb2 && op != 0xb3) {
                r->reason = "UNTERMINATED RAW TRIANGLE";
                break;
            }
            halves++;
            if (halves > halfWords || (op == 0xb3 && halves != halfWords)
                    || (halves == halfWords && op != 0xb3)) {
                r->reason = "WRONG RAW TRIANGLE LENGTH";
                break;
            }
            if (op == 0xb3) halves = 0;
            continue;
        }
        if (op == 0xb4) {
            op = (w1 >> 24) & 0x3f;
            if (op < 8 || op > 15) {
                r->reason = "UNSUPPORTED HALF COMMAND";
                return;
            }
            halfWords = 8 + ((op & 4) ? 16 : 0)
                + ((op & 2) ? 16 : 0) + ((op & 1) ? 4 : 0);
            halves = 1;
            reason = checkDraw(r, hiKnown, loKnown);
            if (reason) { r->reason = reason; break; }
            continue;
        }
        if (op == 0x06) {
            address = checkAddress(w1, segments, segmentKnown);
            if (address == 0xffffffff) { r->reason = "UNKNOWN DL SEGMENT"; return; }
            if ((w0 >> 16 & 0xff) == 0) {
                if (depth == CHECK_DEPTH) { r->reason = "DL STACK LIMIT"; return; }
                callers[depth] = r->address;
                returns[depth++] = pc;
            } else if (!depth) {
                /* A root branch has no return to the original master list. */
                start = 0;
                end = ramSize;
            }
            pc = address;
            continue;
        }
        if (op == 0xb8) {
            if (depth) { pc = returns[--depth]; continue; }
            r->status = N64RDP_CHECKED;
            r->reason = "CHECKED KNOWN HAZARDS";
            return;
        }
        if (op == 0xbe) { r->reason = "CONDITIONAL CULL LIST"; return; }
        if (op == 0xbc && (w0 & 0xff) == 6) {
            address = (w0 >> 8) & 0xffff;
            if ((address & 3) || address >= 64) {
                r->reason = "INVALID SEGMENT OFFSET";
                break;
            }
            segments[address / 4] = w1 & 0xffffff;
            segmentKnown |= 1U << (address / 4);
        } else if (op == 0xba || op == 0xb9) {
            shift = w0 >> 8 & 0xff;
            size = w0 & 0xff;
            if (!size || shift >= 32 || size > 32 - shift) {
                r->reason = "INVALID OTHERMODE MASK";
                break;
            }
            mask = (0xffffffffU >> (32 - size)) << shift;
            if (op == 0xba) {
                r->modeHi = (r->modeHi & ~mask) | w1;
                hiKnown |= mask;
            } else {
                r->modeLo = (r->modeLo & ~mask) | w1;
                loKnown |= mask;
            }
        } else if (op == 0xef) {
            r->modeHi = w0 & 0xffffff;
            r->modeLo = w1;
            hiKnown = loKnown = 0xffffffff;
        } else if (op == 0xfd) {
            r->texture = checkAddress(w1, segments, segmentKnown);
            r->textureSize = (w0 >> 19) & 3;
        } else if (op == 0xff) {
            r->colorSize = (w0 >> 19) & 3;
        } else if (op == 0xed) {
            r->scissorLeft = (w0 >> 12) & 0xfff;
        } else if (op == 0xf4 || op == 0xf3 || op == 0xf0) {
            if (op != 0xf3 && r->textureSize == 0) {
                r->reason = "4 BIT TILE OR PALETTE LOAD";
                break;
            }
            if (op == 0xf0 && ((w0 >> 14) & 0x3ff) > ((w1 >> 14) & 0x3ff)) {
                r->reason = "REVERSED PALETTE LOAD";
                break;
            }
            pixels = op == 0xf3 ? ((w1 >> 12) & 0xfff) + 1
                : (((w1 >> 12) & 0xfff) >> 2) - (((w0 >> 12) & 0xfff) >> 2) + 1;
            /* Hardware load bug documented by libdragon's RDP validator:
             * nonzero misalignment in the first half of a 16-byte line,
             * non-4b texture, and a horizontal transfer of roughly 58+ bytes.
             * Report this as a hazard; the exact threshold is size-dependent. */
            if (op != 0xf0 && r->texture != 0xffffffff
                    && (r->texture & 15) > 0 && (r->texture & 15) < 8
                    && r->textureSize > 0 && r->textureSize <= 3
                    && pixels <= 4096 && pixels * (4U << r->textureSize) >= 58 * 8) {
                r->reason = "MISALIGNED TEXTURE LOAD";
                break;
            }
        } else if (op == 0x01 || op == 0x03 || op == 0x04) {
            address = checkAddress(w1, segments, segmentKnown);
            size = w0 & 0xffff;
            if (address != 0xffffffff && ((address & 7) || !size
                    || !checkRange(address, size, ramSize))) {
                r->reason = "INVALID RSP DMA ADDRESS";
                break;
            }
        } else if (op == 0xf6 || op == 0xe4 || op == 0xe5 || op == 0xbf || op == 0xb1) {
            reason = checkDraw(r, hiKnown, loKnown);
            if (reason) { r->reason = reason; break; }
            if (op == 0xe4 || op == 0xe5) rectWords = 2;
        } else if (op != 0 && op != 0xbc && op != 0xbb && op != 0xbd
                && op != 0xb6 && op != 0xb7 && op != 0xc0
                && !(op >= 0xe6 && op <= 0xee)
                && op != 0xf2 && op != 0xf5 && !(op >= 0xf7 && op <= 0xfc)
                && op != 0xfe) {
            r->reason = "UNSUPPORTED GBI COMMAND";
            return;
        }
    }
    r->status = N64RDP_HAZARD;
}
#endif
