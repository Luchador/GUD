#include <ultra64.h>
#include "image.h"


void image_entries_load(void)
{   
    s32 offset;
    s32 temp_t3;
    struct image_entry *entryp;

    entryp = g_Textures;
    offset = 0;
    /* Convert appended images along with the built-ins, stopping before the
     * unused reserved slots. The following row stores the final end offset. */
    if (g_TextureRomConfig.textureCount > MAX_TEXTURES)
    { g_TextureRomConfig.textureCount = MAX_TEXTURES; }
    while (entryp < g_Textures + g_TextureRomConfig.textureCount)
    {
        temp_t3 = entryp->dataoffset;
        entryp->dataoffset = offset;
        offset += temp_t3;
        entryp++;
    }

    entryp->dataoffset = offset;
}
