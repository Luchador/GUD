#include "occluders.h"
#include "setupmeta.h"
#include "../../../src/occluderformat.h"
#include "../../../src/game/occlusionmath.h"

BOOL OccludersValidateNative(const unsigned char *data, DWORD size, const RomFile *rom, const char **why)
{
    DWORD count = 0, at;
    BOOL supported = FALSE;
    OcclusionBox box;
    *why = "Invalid occluder bound-pad table.";
    if (!data || size < 40) { return FALSE; }
    at = SetupMetaRead32(data+28);
    while (at)
    {
        if (at > size || size-at < 68) { return FALSE; }
        if (!SetupMetaRead32(data+at+36)) { break; }
        if (SetupMetaRead32(data+at+40) == OCCLUDER_PAD_TAG)
        {
            float pos[3], up[3], look[3], bounds[6];
            count++;
            for (int i = 0; i < 15; i++)
            {
                union { DWORD u; float f; } value;
                value.u = SetupMetaRead32(data+at+(i < 9 ? i*4 : 44+(i-9)*4));
                if (i < 3) { pos[i] = value.f; }
                else if (i < 6) { up[i-3] = value.f; }
                else if (i < 9) { look[i-6] = value.f; }
                else { bounds[i-9] = value.f; }
            }
            if (!occlusionBoxInit(&box, pos, up, look, bounds))
            { *why = "An occluder has invalid or collapsed bounds. Resize or delete it before exporting."; return FALSE; }
        }
        at += 68;
    }
    if (count > OCCLUDER_MAX)
    { *why = "A setup contains more than 32 occluders. Delete the excess before exporting."; return FALSE; }
    for (DWORD i = 0; rom && i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *entry = &rom->info.entries[i];
        if (entry->kind == OCCLUDER_MANIFEST_KIND && entry->flags == OCCLUDER_VERSION) { supported = TRUE; }
    }
    if (count && !supported)
    { *why = "This setup contains occluders. Rebase onto a GUD ROM built with occluder support before exporting."; return FALSE; }
    *why = ""; return TRUE;
}
