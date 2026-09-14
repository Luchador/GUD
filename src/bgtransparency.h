#ifndef GUD_BGTRANSPARENCY_H
#define GUD_BGTRANSPARENCY_H

/* Editor/runtime contract in a standard Fast3D gDPNoOpTag packet. Native
 * render-mode commands alongside it implement the choice with either AA mode.
 * The tag stores the policy and the pre-override surface bits. Auto resets it.
 * Type-0 texture markers do not use bits 12..23 of w1; the signature occupies
 * those reserved bits as well as the high byte. texLoadFromGdl passes our tags
 * through instead of interpreting them as texture requests.
 * Keep these definitions free of platform types for the Windows editor. */
#define BG_SURFACE_MARKER 0xc0000000u
#define BG_SURFACE_TAG 0x47400000u
#define BG_SURFACE_TAG_MASK 0xffc00000u
#define BG_SURFACE_MODE_MASK 0x000ffff8u
#define BG_SURFACE_AUTO 0u
#define BG_SURFACE_OPAQUE 1u
#define BG_SURFACE_CUTOUT 2u
#define BG_SURFACE_BLEND 3u
#define BG_SURFACE_UNKNOWN 255u
#define BG_SURFACE_IS_MARKER(w0, w1) \
    ((w0) == BG_SURFACE_MARKER && ((w1) & BG_SURFACE_TAG_MASK) == BG_SURFACE_TAG)
#define BG_SURFACE_TAG_VALUE(policy, mode) \
    (BG_SURFACE_TAG | ((policy) << 20) | ((mode) & BG_SURFACE_MODE_MASK))
#define BG_SURFACE_TAG_POLICY(w1) (((w1) >> 20) & 3u)

#endif
