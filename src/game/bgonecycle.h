#ifndef _BGONECYCLE_H_
#define _BGONECYCLE_H_

#include <ultra64.h>

/* Build an alternate, AA-off primary room list AFTER texture expansion and
 * the environment's combiner/render-mode LUT. Source commands are untouched.
 * With dst == NULL, return the required byte count. Return 0 when there are
 * no eligible draws, or -1 for an invalid/truncated list or small destination.
 * The alternate restores the source's AA-Off outgoing state before returning.
 * Like ordinary opaque BG rendering, inherited alpha compare is assumed None;
 * explicitly enabled alpha comparison makes a draw ineligible. */
s32 bgBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity);

#endif
