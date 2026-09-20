#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "setupstan.h"

static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }
static void Write32(unsigned char *p, DWORD v)
{ p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }

BOOL SetupRefreshPadStanNativeReport(const unsigned char *data, DWORD size,
    const StanFile *stan, unsigned char **out, DWORD *sizeout,
    SetupStanRefresh *stats, SetupStanIssueFn report, void *context, const char **reasonout)
{
    unsigned char *copy = NULL;
    DWORD length;
    float inverse;
    *out = NULL; *sizeout = 0; ZeroMemory(stats, sizeof(*stats));
    *reasonout = "There is no valid stan for refreshing placement pads.";
    if (!stan || !stan->tiles || !stan->tilecount
        || !isfinite(stan->levelscale) || stan->levelscale <= 0) { return FALSE; }
    if (!SetupCompactNative(data, size, &copy, &length, reasonout)) { return FALSE; }
    inverse = 1.0f / stan->levelscale;
    for (DWORD bound = 0; bound < 2; bound++)
    {
        DWORD at = Read32(copy + 24 + bound * 4), stride = bound ? 68 : 44;
        for (DWORD index = 0; at && Read32(copy + at + 36); index++, at += stride)
        {
            float pos[3]; char name[16];
            DWORD link = Read32(copy + at + 36);
            /* Deleted pads retain their native stable-index placeholder. */
            if (Read32(copy + at + 40) == 0x47455044u) { continue; }
            for (int axis = 0; axis < 3; axis++)
            {
                union { DWORD bits; float value; } f;
                f.bits = Read32(copy + at + axis * 4); pos[axis] = f.value * inverse;
            }
            if (!isfinite(pos[0]) || !isfinite(pos[1]) || !isfinite(pos[2])
                || !StanResolveSavedPadName(stan, (const char *)copy + link, pos, name))
            {
                if (!stats->unresolved) { stats->firstunresolved = (SetupPadRef){index, bound != 0}; }
                if (report && !report(context, (SetupPadRef){index, bound != 0}, pos, (const char *)copy + link))
                { *reasonout = "Out of memory recording unresolved pads."; goto fail; }
                stats->unresolved++; continue;
            }
            if (!strcmp((const char *)copy + link, name)) { continue; }
            DWORD start = (length + 3u) & ~3u;
            DWORD end = (start + (DWORD)strlen(name) + 4u) & ~3u;
            unsigned char *grown;
            if (end > 16u * 1024u * 1024u)
            { *reasonout = "The refreshed setup exceeds the native size limit."; goto fail; }
            grown = realloc(copy, end);
            if (!grown) { *reasonout = "Out of memory refreshing pad references."; goto fail; }
            copy = grown; memset(copy + length, 0, end - length);
            memcpy(copy + start, name, strlen(name) + 1);
            Write32(copy + at + 36, start); length = end; stats->updated++;
        }
    }
    if (!SetupCompactNative(copy, length, out, sizeout, reasonout)) { goto fail; }
    free(copy); return TRUE;
fail:
    free(copy); return FALSE;
}

BOOL SetupRefreshPadStanNative(const unsigned char *data, DWORD size,
    const StanFile *stan, unsigned char **out, DWORD *sizeout,
    SetupStanRefresh *stats, const char **reasonout)
{
    return SetupRefreshPadStanNativeReport(data, size, stan, out, sizeout, stats, NULL, NULL, reasonout);
}

BOOL SetupSaveProjectFileWithStan(const char *projectdir, const SetupFile *setup,
    const StanFile *stan, SetupStanRefresh *stats, const char **reasonout)
{
    SetupFile copy;
    StanFile savedstan = {0};
    unsigned char *standata;
    DWORD stansize;
    BOOL ok;
    ZeroMemory(stats, sizeof(*stats));
    if (!stan || !stan->data) { return SetupSaveProjectFile(projectdir, setup, reasonout); }
    if (!setup) { *reasonout = "There is no setup to save."; return FALSE; }
    /* Saving can regroup rooms. Resolve against that exact tile order so
     * duplicate native IDs have the same first match after reloading. */
    if (!StanPrepareSave(stan, &standata, &stansize, reasonout)) { return FALSE; }
    ok = StanLoadNative(standata, stansize, stan->levelscale, &savedstan, reasonout);
    free(standata);
    if (!ok) { return FALSE; }
    copy = *setup;
    ok = SetupRefreshPadStanNative(setup->data, setup->size, &savedstan,
        &copy.data, &copy.size, stats, reasonout);
    StanFileFree(&savedstan);
    if (!ok) { return FALSE; }
    ok = SetupSaveProjectFile(projectdir, &copy, reasonout);
    free(copy.data); return ok;
}
