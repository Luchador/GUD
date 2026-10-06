#include <ultra64.h>
#include <memp.h>
#include "image.h"
#include "initmttex.h"
#include "lv.h"


void set_mt_tex_alloc(void)
{  
    g_TexCacheCount = 0;

    if (tokenFind(1, "-mt"))
    {
        bytes = strtol(tokenFind(1, "-mt"), 0x0, 0) * 1024; //get KB
    }

    /* The restored Bond portraits add 45 KB to the front end's resident
     * textures. Keep room for multiplayer portraits and menu icons, including
     * projects rebased with the old Title -mt646 allocation still saved. */
    if (lvlGetCurrentStageToLoad() == LEVELID_TITLE && bytes < 710 * 1024)
    {
        bytes = 710 * 1024;
    }

    texInitPool(&ptr_texture_alloc_start, mempAllocBytesInBank(bytes, MEMPOOL_STAGE), bytes);
}
