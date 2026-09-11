#ifndef GUD_PROPRUNTIMEFLAGS_H
#define GUD_PROPRUNTIMEFLAGS_H

/* Runtime-only bits in the u8 PropRecord.flags field. These are maintained by
 * the game, not authored in setup files. They are unrelated to the same-valued
 * PROPFLAG bits in ObjectRecord.flags or to ObjectRecord.runtime_bitflags.
 * Keep the storage field as u8; the enum supplies names, not a new layout. */
typedef enum PROPRUNTIMEFLAG
{
    PROPRUNTIMEFLAG_RENDERPOSTBG   = 0x01, // Draw opaque geometry after the room background.
    PROPRUNTIMEFLAG_ONSCREEN       = 0x02, // Included in rendering for the current player view.
    PROPRUNTIMEFLAG_ENABLED        = 0x04, // Eligible for the on-screen prop list.
    PROPRUNTIMEFLAG_USESTOREDROOMS  = 0x08, // Use stored rooms instead of the stan locus; skip stan-line placement for effects.
    PROPRUNTIMEFLAG_PRESERVEROOMS  = 0x10, // Retain the room list when deregistering (e.g. settled projectiles outside stan).
    PROPRUNTIMEFLAG_RENDERLATE     = 0x20  // Defer opaque geometry to the translucent prop pass.
} PROPRUNTIMEFLAG;

#endif /* GUD_PROPRUNTIMEFLAGS_H */
