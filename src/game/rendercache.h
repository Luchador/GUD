#ifndef _RENDERCACHE_H_
#define _RENDERCACHE_H_

#include <ultra64.h>

/* Optional display-list copies share mema with required room geometry.
 * Reset only at stage initialization, after memaReset and the graphics drain. */
void renderCacheReset(void);
bool renderCacheIsEnabled(void);
void *renderCacheAlloc(s32 size);
/* The caller must already know that graphics no longer reference this copy. */
void renderCacheFree(void *data);

/* A failed required allocation requests recovery; it never frees live lists. */
void renderCacheRequestReclaim(void);
bool renderCacheReclaimPending(void);
/* Call only between frames, with no queued graphics tasks remaining. */
void renderCacheReclaim(void);

#endif
