#include <math.h>
#include <float.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "roomstats.h"

#define ROOM_UNKNOWN ((DWORD)-1)

static BOOL PlacedObject(const SetupObject *object)
{
    return !object->deleted && (object->type == PROPDEF_DOOR
        || !(object->flags & (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_INSIDEANOTHEROBJ)));
}

static DWORD PadRoom(const BgDocument *bg, const SetupFile *setup, const StanFile *stan,
                     DWORD selection, DWORD *cache)
{
    SetupPadRef ref;
    float position[3];
    if (!cache || !isfinite(bg->levelscale) || bg->levelscale <= 0
        || !stan->tiles || !stan->tilecount || !isfinite(stan->levelscale) || stan->levelscale <= 0
        || !SetupFileGetModelPad(setup, selection, &ref)) { return 0; }
    size_t slot = ref.bound ? (size_t)setup->padcount + ref.index : ref.index;
    if (cache[slot] != ROOM_UNKNOWN) { return cache[slot]; }
    cache[slot] = 0;
    const SetupPad *pad = ref.bound ? &setup->boundpads[ref.index].pad : &setup->pads[ref.index];
    if (pad->deleted) { return 0; }
    for (int axis = 0; axis < 3; axis++)
    {
        position[axis] = pad->pos[axis] * (1.0f / bg->levelscale);
        if (!isfinite(position[axis])) { return 0; }
    }
    DWORD tile = StanResolvePadTile(stan, pad->stanname, position);
    if (tile != STAN_TILE_NONE && tile < stan->tilecount)
    {
        DWORD room = stan->tiles[tile].room;
        if (room && room <= bg->roomcount) { cache[slot] = room; }
    }
    return cache[slot];
}

static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }

static BOOL MonitorOwner(const SetupFile *setup, DWORD index, DWORD *owner)
{
    const SetupObject *object = &setup->objects[index];
    DWORD at = object->sourceoffset;
    return object->type == PROPDEF_MONITOR && object->pad < 0 && setup->data
        && at <= setup->size && setup->size - at >= 256
        && SetupObjectRelativeTarget(setup, at, (LONG)Read32(setup->data + at + 244), owner)
        && *owner < setup->objectcount;
}

void RoomStatsFree(RoomStats *stats)
{
    if (!stats) { return; }
    free(stats->rooms); memset(stats, 0, sizeof(*stats));
}

BOOL RoomStatsBuild(const BgDocument *bg, const SetupFile *setup, const StanFile *stan,
                    RoomStats *out, const char **why)
{
    RoomStats result = {0};
    DWORD *padrooms = NULL, *objectrooms = NULL;
    *why = "The level's room data is incomplete.";
    memset(out, 0, sizeof(*out));
    if (!bg || !setup || !stan || (bg->roomcount && !bg->rooms)
        || (stan->tilecount && !stan->tiles) || (setup->padcount && !setup->pads)
        || (setup->boundpadcount && !setup->boundpads) || (setup->objectcount && !setup->objects)
        || (setup->charactercount && !setup->characters) || bg->roomcount > 65535) { return FALSE; }
    *why = "Out of memory counting room contents.";
    size_t padcount = (size_t)setup->padcount + setup->boundpadcount;
    if (padcount < setup->padcount || padcount > SIZE_MAX / sizeof(*padrooms)) { return FALSE; }
    result.rooms = calloc((size_t)bg->roomcount + 1, sizeof(*result.rooms));
    padrooms = padcount ? malloc(padcount * sizeof(*padrooms)) : NULL;
    objectrooms = setup->objectcount ? calloc(setup->objectcount, sizeof(*objectrooms)) : NULL;
    if (!result.rooms || (padcount && !padrooms) || (setup->objectcount && !objectrooms)) { goto fail; }
    for (size_t i = 0; i < padcount; i++) { padrooms[i] = ROOM_UNKNOWN; }
    result.roomcount = bg->roomcount;
    for (DWORD room = 1; room <= bg->roomcount; room++)
    {
        const BgDocumentRoom *r = &bg->rooms[room];
        if (r->facecount && !r->faces) { *why = "A room's background faces are missing."; goto fail; }
        for (DWORD i = 0; i < r->facecount; i++)
        {
            if (r->faces[i].layer > BG_GEOMETRY_SECONDARY)
            { *why = "A background face has an invalid layer."; goto fail; }
            result.rooms[room].values[r->faces[i].layer == BG_GEOMETRY_SECONDARY ? ROOM_SECONDARY_TRIS : ROOM_PRIMARY_TRIS]++;
        }
    }
    for (DWORD i = 0; i < stan->tilecount; i++)
    {
        DWORD room = stan->tiles[i].room;
        result.rooms[room <= bg->roomcount ? room : 0].values[ROOM_STAN_TILES]++;
    }
    for (DWORD i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *object = &setup->objects[i];
        objectrooms[i] = !PlacedObject(object) ? 0
            : object->type == PROPDEF_MONITOR && object->pad < 0 ? ROOM_UNKNOWN
            : PadRoom(bg, setup, stan, i, padrooms);
    }
    for (DWORD i = 0; i < setup->objectcount; i++)
    {
        if (!PlacedObject(&setup->objects[i])) { continue; }
        if (objectrooms[i] == ROOM_UNKNOWN)
        {
            DWORD owner = i, room = 0;
            /* Follow relative monitor attachments; bound the walk for cycles. */
            for (DWORD step = 0; step < setup->objectcount; step++)
            {
                if (!MonitorOwner(setup, owner, &owner)) { break; }
                if (objectrooms[owner] != ROOM_UNKNOWN) { room = objectrooms[owner]; break; }
            }
            objectrooms[i] = room;
        }
        result.rooms[objectrooms[i]].values[ROOM_OBJECTS]++;
    }
    for (DWORD i = 0; i < setup->charactercount; i++)
    {
        if (setup->characters[i].deleted) { continue; }
        DWORD room = PadRoom(bg, setup, stan, i | SETUP_CHARACTER_SELECTION_BIT, padrooms);
        result.rooms[room].values[ROOM_CHARACTERS]++;
    }
    for (DWORD room = 0; room <= result.roomcount; room++)
        for (int column = 0; column < ROOM_COUNT_COLUMNS; column++)
            result.total.values[column] += result.rooms[room].values[column];
    free(padrooms); free(objectrooms); *out = result; *why = ""; return TRUE;
fail:
    free(padrooms); free(objectrooms); RoomStatsFree(&result); return FALSE;
}

static BOOL RoomBoundsExtend(double min[3], double max[3], const float position[3])
{
    for (int axis = 0; axis < 3; axis++)
    {
        if (!isfinite(position[axis])) { return FALSE; }
        min[axis] = fmin(min[axis], position[axis]);
        max[axis] = fmax(max[axis], position[axis]);
    }
    return TRUE;
}

BOOL RoomStatsGetBounds(const BgDocument *bg, const StanFile *stan, DWORD room,
                        double min[3], double max[3])
{
    if (!bg || !bg->rooms || !room || room > bg->roomcount || !min || !max
        || !isfinite(bg->levelscale) || bg->levelscale <= 0) { return FALSE; }
    const BgDocumentRoom *r = &bg->rooms[room];
    if (r->facecount && (!r->faces || !r->vertices)) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { min[axis] = DBL_MAX; max[axis] = -DBL_MAX; }
    /* Only live face corners contribute: deleted faces can leave unused
     * vertices behind until compaction. Include both BG layers. */
    for (DWORD face = 0; face < r->facecount; face++)
        for (int corner = 0; corner < 3; corner++)
        {
            DWORD index = r->faces[face].vertexindices[corner];
            float position[3];
            if (index >= r->vertexcount) { return FALSE; }
            BgDocumentGetWorldPosition(bg, r, &r->vertices[index], position);
            if (!RoomBoundsExtend(min, max, position)) { return FALSE; }
        }
    if (r->facecount) { return TRUE; }
    if (stan && stan->tiles)
        for (DWORD tile = 0; tile < stan->tilecount; tile++)
        {
            const StanTile *t = &stan->tiles[tile];
            if (t->room != room) { continue; }
            if (t->pointcount > STAN_TILE_MAX_POINTS) { return FALSE; }
            for (int point = 0; point < t->pointcount; point++)
            {
                const StanPoint *p = &t->points[point];
                float position[3] = {p->x, p->y, p->z};
                if (!RoomBoundsExtend(min, max, position)) { return FALSE; }
            }
        }
    if (min[0] != DBL_MAX) { return TRUE; }
    /* Even a completely empty authored room can be visited. */
    float position[3];
    for (int axis = 0; axis < 3; axis++) { position[axis] = r->origin[axis] / bg->levelscale; }
    return RoomBoundsExtend(min, max, position);
}
