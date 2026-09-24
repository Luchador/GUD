#include <math.h>
#include <stdlib.h>
#include "roomedit.h"
#include "doorshadow.h"

DWORD RoomEditPadRoom(const StanFile *stan, const SetupPad *pad, float scale)
{
    if (!stan || !pad || pad->deleted || !(scale > 0)) { return 0; }
    float position[3];
    for (int a = 0; a < 3; a++) { position[a] = pad->pos[a] / scale; }
    DWORD tile = StanResolvePadTile(stan, pad->stanname, position);
    return tile < stan->tilecount ? stan->tiles[tile].room : 0;
}

DWORD RoomEditObjectRoom(const SetupFile *setup, const StanFile *stan, float scale, DWORD object)
{
    SetupPadRef ref;
    DoorShadowProperties shadow;
    if (!setup) { return 0; }
    if (!(object & SETUP_CHARACTER_SELECTION_BIT) && DoorShadowGet(setup, object, &shadow))
    { return shadow.room; }
    if (!SetupFileGetModelPad(setup, object, &ref)) { return 0; }
    return RoomEditPadRoom(stan, ref.bound ? &setup->boundpads[ref.index].pad : &setup->pads[ref.index], scale);
}

BOOL RoomEditPortalConnected(const BgPortalFile *portals, DWORD index, DWORD room)
{
    if (!room || index >= portals->portalcount) { return FALSE; }
    for (DWORD i = 0; i < portals->portalcount; i++)
    {
        const BgPortal *p = &portals->portals[i];
        if (p->geometryoffset == portals->portals[index].geometryoffset
            && (p->connectedroom1 == room || p->connectedroom2 == room)) { return TRUE; }
    }
    return FALSE;
}

BOOL RoomEditOffset(float scale, const double requested[3], double applied[3])
{
    if (!(scale > 0) || !isfinite(scale)) { return FALSE; }
    for (int a = 0; a < 3; a++)
    {
        double native = round(requested[a] * scale);
        if (!isfinite(native) || fabs(native) > 65535) { return FALSE; }
        applied[a] = native / scale;
    }
    return TRUE;
}

BOOL RoomEditTranslate(BgDocument *bg, SetupFile *setup, StanFile *stan, DWORD room,
    const double requested[3], BOOL *changed, const char **why)
{
    double offset[3];
    unsigned char *pads = NULL, *boundpads = NULL;
    BgPortalPointRef points[BG_MAX_PORTALS * BG_PORTAL_MAX_POINTS];
    DWORD count = 0, moved;
    *changed = FALSE;
    *why = "The room or translation is invalid.";
    if (!bg || !setup || !stan || !room || room > bg->roomcount || !bg->rooms
        || !RoomEditOffset(bg->levelscale, requested, offset)) { return FALSE; }
    if (!offset[0] && !offset[1] && !offset[2]) { return TRUE; }
    if (stan->data && stan->levelscale != bg->levelscale)
    { *why = "The background and Stan scales do not match."; return FALSE; }
    /* Resolve every pad before moving any floors: spatial fallback must use
     * the original room layout. Ordinary/door pad tables have distinct IDs. */
    pads = calloc(setup->padcount ? setup->padcount : 1, 1);
    boundpads = calloc(setup->boundpadcount ? setup->boundpadcount : 1, 1);
    if (!pads || !boundpads) { *why = "Out of memory collecting the room's pads."; goto fail; }
    for (DWORD i = 0; i < setup->padcount; i++)
    { pads[i] = RoomEditPadRoom(stan, &setup->pads[i], bg->levelscale) == room; }
    for (DWORD i = 0; i < setup->boundpadcount; i++)
    { boundpads[i] = RoomEditPadRoom(stan, &setup->boundpads[i].pad, bg->levelscale) == room; }
    for (int a = 0; a < 3; a++)
    {
        double value = bg->rooms[room].origin[a] + offset[a] * bg->levelscale;
        if (!isfinite(value) || fabs(value) > 100000000)
        { *why = "The move exceeds the background coordinate range."; goto fail; }
    }
    if (!StanTranslateRoom(stan, room, offset, why)) { goto fail; }
    for (int bound = 0; bound < 2; bound++)
        for (DWORD i = 0; i < (bound ? setup->boundpadcount : setup->padcount); i++)
            if ((bound ? boundpads : pads)[i])
            {
                SetupPadRef ref = {i, bound}; BOOL movedpad;
                if (!SetupFileTranslateRoomPad(setup, &ref, bg->levelscale, offset, &movedpad, why)) { goto fail; }
            }
    for (DWORD i = 0; i < setup->objectcount; i++)
    {
        DoorShadowProperties shadow;
        if (DoorShadowGet(setup, i, &shadow) && shadow.room == room
            && !DoorShadowTranslate(setup, i, bg->levelscale, offset, why)) { goto fail; }
    }
    for (DWORD i = 0; i < bg->portals.portalcount; i++)
        if (RoomEditPortalConnected(&bg->portals, i, room))
            for (DWORD p = 0; p < bg->portals.portals[i].pointcount; p++)
            { points[count++] = (BgPortalPointRef){i,p}; }
    if (count && !BgDocumentTranslatePortalPoints(bg, points, count, offset, &moved, why)) { goto fail; }
    for (int a = 0; a < 3; a++)
    { bg->rooms[room].origin[a] = (float)(bg->rooms[room].origin[a] + offset[a] * bg->levelscale); }
    bg->dirty = *changed = TRUE;
    free(pads); free(boundpads); *why = ""; return TRUE;
fail:
    free(pads); free(boundpads); return FALSE;
}
