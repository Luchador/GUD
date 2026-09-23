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

/* Explicit Cutout enables texture alpha on the standard opaque textured
 * combiners. Keep the authored words in the asset so Auto restores them;
 * apply this before the environment LUT, in both the loader and preview.
 * Custom equations and untextured SHADE materials are left alone. */
#define BG_CUTOUT_COMBINE_W0(w0, w1) \
    (((w0) == 0xfc127e24u && (w1) == 0xfffff9fcu) ? 0xfc121824u : (w0))
#define BG_CUTOUT_COMBINE_W1(w0, w1) \
    (((w0) == 0xfc26a004u && (w1) == 0x1ffc93fcu) ? 0x1f1093ffu : \
     ((w0) == 0xfc127e24u && (w1) == 0xfffff9fcu) ? 0xff33ffffu : (w1))

/* Per-face alpha scopes. Keep the authored combiner in the asset so Auto can
 * restore it exactly. The loader replaces reserved packets with a pipe sync,
 * fog geometry state and four first-cycle blender fields in place.
 * Scope boundaries precede vertex loads: hardware fog overwrites vertex A.
 * A closing Auto scope restores the surrounding fog/blender before ENDDL. */
#define BG_ALPHA_TAG 0x47800000u
#define BG_ALPHA_IS_MARKER(w0, w1) \
    ((w0) == BG_SURFACE_MARKER && ((w1) & 0xffffff00u) == BG_ALPHA_TAG)
#define BG_ALPHA_AUTO 0u
#define BG_ALPHA_VERTEX 1u
#define BG_ALPHA_SYNC 2u
#define BG_ALPHA_FOG 3u
#define BG_ALPHA_BLENDER 4u
#define BG_ALPHA_LAST_SLOT 7u
/* 2..7 are reserved runtime packets, not selectable alpha presets. Keep the
 * original Auto/Vertex values compatible with existing projects. */
#define BG_ALPHA_OPAQUE 8u
#define BG_ALPHA_TEXTURE 9u
#define BG_ALPHA_TEXTURE_VERTEX 10u
#define BG_ALPHA_CONSTANT 11u
#define BG_ALPHA_TEXTURE_CONSTANT 12u
#define BG_ALPHA_IS_PRESET(source) \
    ((source) <= BG_ALPHA_VERTEX || ((source) >= BG_ALPHA_OPAQUE && (source) <= BG_ALPHA_TEXTURE_CONSTANT))
#define BG_ALPHA_USES_VERTEX(source) \
    ((source) == BG_ALPHA_VERTEX || (source) == BG_ALPHA_TEXTURE_VERTEX)
#define BG_ALPHA_USES_TEXTURE(source) \
    ((source) == BG_ALPHA_TEXTURE || (source) == BG_ALPHA_TEXTURE_VERTEX || (source) == BG_ALPHA_TEXTURE_CONSTANT)
#define BG_ALPHA_USES_CONSTANT(source) \
    ((source) == BG_ALPHA_CONSTANT || (source) == BG_ALPHA_TEXTURE_CONSTANT)
#define BG_ALPHA_WITHOUT_TEXTURE(source) \
    ((source) == BG_ALPHA_TEXTURE ? BG_ALPHA_OPAQUE : \
     (source) == BG_ALPHA_TEXTURE_VERTEX ? BG_ALPHA_VERTEX : \
     (source) == BG_ALPHA_TEXTURE_CONSTANT ? BG_ALPHA_CONSTANT : (source))
#define BG_ALPHA_TAG_KIND(w1) ((w1) & 0xffu)
#define BG_EDITOR_IS_MARKER(w0, w1) \
    (BG_SURFACE_IS_MARKER(w0, w1) || BG_ALPHA_IS_MARKER(w0, w1))

/* (0 - 0) * 0 + SHADE in both alpha cycles; RGB mux bits are untouched. */
#define BG_ALPHA_COMBINE_W0(w0) ((w0) | 0x00007e00u)
#define BG_ALPHA_COMBINE_W1(w1) (((w1) & ~0x00fc7e3fu) | 0x00fc783cu)

#endif
