#include <float.h>
#include <math.h>
#include "levelscale.h"
#include "rom.h"

static BOOL CoordinateFits(double value, float inverse)
{ return isfinite(value) && fabs(value * inverse) <= FLT_MAX; }

static BOOL Background(BgDocument *bg, float scale, BOOL apply)
{
    float inverse = 1.0f / scale;
    if (!apply)
        for (DWORD r = 1; bg->rooms && r <= bg->roomcount; r++)
        {
            const BgDocumentRoom *room = &bg->rooms[r];
            for (DWORD v = 0; v < room->vertexcount; v++)
            {
                const BgDocumentVertex *p = &room->vertices[v];
                if (!CoordinateFits((double)room->origin[0] + p->x, inverse)
                    || !CoordinateFits((double)room->origin[1] + p->y, inverse)
                    || !CoordinateFits((double)room->origin[2] + p->z, inverse)) { return FALSE; }
            }
        }
    for (DWORD i = 0; i < bg->portals.portalcount; i++)
    {
        BgPortal *portal = &bg->portals.portals[i];
        for (DWORD p = 0; p < portal->pointcount; p++)
        {
            const BgPortalPoint *native = &portal->nativepoints[p];
            if (!CoordinateFits(native->x, inverse) || !CoordinateFits(native->y, inverse)
                || !CoordinateFits(native->z, inverse)) { return FALSE; }
            if (apply) { portal->points[p] = (BgPortalPoint){native->x * inverse, native->y * inverse, native->z * inverse}; }
        }
    }
    if (apply) { bg->levelscale = scale; }
    return TRUE;
}

static BOOL Stan(StanFile *stan, float scale, BOOL apply)
{
    float inverse = 1.0f / scale;
    for (DWORD t = 0; t < stan->tilecount; t++)
    {
        StanTile *tile = &stan->tiles[t];
        if (!stan->data || tile->sourceoffset > stan->size
            || 8u + 8u * tile->pointcount > stan->size - tile->sourceoffset) { return FALSE; }
        for (DWORD p = 0; p < tile->pointcount; p++)
        {
            const unsigned char *native = stan->data + tile->sourceoffset + 8 + p * 8;
            float position[3];
            for (int axis = 0; axis < 3; axis++)
            {
                short value = (short)((native[axis * 2] << 8) | native[axis * 2 + 1]);
                if (!CoordinateFits(value, inverse)) { return FALSE; }
                position[axis] = value * inverse;
            }
            if (apply)
            { tile->points[p].x = position[0]; tile->points[p].y = position[1]; tile->points[p].z = position[2]; }
        }
    }
    if (apply) { stan->levelscale = scale; }
    return TRUE;
}

BOOL LevelScaleApply(BgDocument *bg, StanFile *stan, EditHistory *history,
                    BgDocument *faceclipboard, BgPortalFile *portalclipboard,
                    float scale, const char **why)
{
    *why = "The level scale cannot represent this level's coordinates.";
    if (!RomScaleIsValid(scale)) { return FALSE; }
    /* Two passes keep even the inactive history documents consistent, without
     * allocating or introducing repeated world/native round-trip rounding. */
    for (int apply = 0; apply < 2; apply++)
    {
        BgDocument portals = {.portals = *portalclipboard};
        if (!Background(bg, scale, apply) || !Stan(stan, scale, apply)) { return FALSE; }
        if (!Background(faceclipboard, scale, apply) || !Background(&portals, scale, apply)) { return FALSE; }
        for (int stack = 0; stack < 2; stack++)
        {
            DWORD count = stack ? history->redocount : history->undocount;
            EditHistoryEntry *entries = stack ? history->redoentries : history->undoentries;
            for (DWORD i = 0; i < count; i++)
                if (((entries[i].asset == EDIT_HISTORY_ASSET_BG || entries[i].asset == EDIT_HISTORY_ASSET_BG_SETUP) && !Background(&entries[i].bgdocument, scale, apply))
                    || (entries[i].asset == EDIT_HISTORY_ASSET_STAN && !Stan(&entries[i].stan, scale, apply))) { return FALSE; }
        }
    }
    *why = ""; return TRUE;
}
