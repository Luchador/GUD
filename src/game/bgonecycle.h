#ifndef _BGONECYCLE_H_
#define _BGONECYCLE_H_

#include <ultra64.h>

#define BG_CUTOUT_THRESHOLD 128

/* Secondary room variant: also threshold fully opaque binary-alpha materials.
 * Caller establishes G_AC_NONE and blend alpha BG_CUTOUT_THRESHOLD. */
s32 bgBuildCutoutGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity);

/* Build an alternate, AA-off primary room list AFTER texture expansion and
 * the environment's combiner/render-mode LUT. Source commands are untouched.
 * With dst == NULL, return the required byte count. Return 0 when there are
 * no eligible draws, or -1 for an invalid/truncated list or small destination.
 * The alternate restores the source's AA-Off outgoing state before returning.
 * Like ordinary opaque BG rendering, inherited alpha compare is assumed None;
 * explicitly enabled alpha comparison makes a draw ineligible. */
s32 bgBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity);

/* Shared converter with an explicit initial material setup. The setup is
 * interpreted, not emitted; instance-specific fog/primitive colours stay in
 * the caller's list. Also accepts the constant fog-alpha blender used by props. */
s32 gfxBuildOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity,
        const Gfx *initial, s32 initialSize);

/* Opaque character variant. Caller must prove every referenced vertex has
 * alpha 255, is unmodified, and is processed with RSP fog disabled. */
s32 gfxBuildCharacterOneCycleGdl(const Gfx *src, s32 size, Gfx *dst, s32 capacity,
        const Gfx *initial, s32 initialSize);

#endif
