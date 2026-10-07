#ifndef GUD_PORTALFLAGS_H
#define GUD_PORTALFLAGS_H

/* First control byte of a native BG portal connection. Shared with GEditor. */
typedef enum PORTALFLAGS
{
    PORTALFLAG_DISABLED        = 0x01, /* Runtime door/glass state. */
    PORTALFLAG_SPECIAL         = 0x02,
    /* Opt in to plane rejection in both directions even when room bounds
     * straddle the plane. Room sides use bgOrderPortal's existing ordering. */
    PORTALFLAG_FORCE_SIDE_CULL = 0x04
} PORTALFLAGS;

#endif
