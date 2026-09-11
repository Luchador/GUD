/* Background primitive authoring. The ordinary BG compiler/save path owns
 * serialization, including native vertex-cache batches used by bullet tests. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

static BOOL BgPrimitiveLocal(const BgDocumentRoom *room, float scale,
    const double world[4][3], DWORD count, BgDocumentVertex local[4])
{
    for (DWORD i = 0; i < count; i++)
    {
        short coords[3];
        for (int axis = 0; axis < 3; axis++)
        {
            double value = round(world[i][axis] * scale - room->origin[axis]);
            if (!isfinite(value) || value < SHRT_MIN || value > SHRT_MAX) { return FALSE; }
            coords[axis] = (short)value;
        }
        local[i].x = coords[0]; local[i].y = coords[1]; local[i].z = coords[2];
    }
    return TRUE;
}

static double BgPrimitiveRoomDistance(const BgDocumentRoom *room, const double point[3])
{
    double lo[3], hi[3], distance = 0;
    BOOL found = FALSE;
    for (DWORD i = 0; i < room->vertexcount; i++)
    {
        const BgDocumentVertex *v = &room->vertices[i];
        double p[3] = {v->x, v->y, v->z};
        if (!v->usecount) { continue; }
        for (int axis = 0; axis < 3; axis++)
        {
            if (!found) { lo[axis] = hi[axis] = p[axis]; }
            else { lo[axis] = fmin(lo[axis], p[axis]); hi[axis] = fmax(hi[axis], p[axis]); }
        }
        found = TRUE;
    }
    for (int axis = 0; axis < 3; axis++)
    {
        double p = point[axis] - room->origin[axis];
        double delta = found ? fmax(lo[axis] - p, fmax(p - hi[axis], 0)) : p;
        distance += delta * delta;
    }
    return distance;
}

BOOL BgDocumentAddPrimitive(BgDocument *document, BOOL quad, DWORD roomnumber,
    const double position[3], const double right[3], BgFaceRef out[2],
    DWORD *countout, const char **reasonout)
{
    /* Explicit opaque, vertex-shaded state: do not inherit a preceding
     * decal, environment map, alpha test, or disabled depth buffer. Keep fog
     * geometry state intact; the render mode is recognized by GUD's fog LUT. */
    static const DWORD commands[][2] = {
        {0xE7000000, 0},          /* Pipe sync. */
        {0xB6000000, 0x000C3000}, /* Clear texgen/linear and both cull modes. */
        {0xB6000000, 0x00020000}, /* Clear lighting: RGBA bytes are colors. */
        {0xB7000000, 0x00000205}, /* Z buffer, shade, smooth shading. */
        {0xBA001402, 0x00100000}, /* Two-cycle combiner. */
        {0xB9000002, 0},          /* No alpha compare. */
        {0xB9000201, 0},          /* Pixel depth source. */
        {0xB900031D, 0x0C192078}, /* G_RM_PASS, G_RM_AA_ZB_OPA_SURF2. */
        {0xFB000000, 0xFFFFFFFF}  /* Opaque environment alpha for BG fog remapping. */
    };
    static const unsigned char corners[2][3] = {{0,1,2}, {0,2,3}};
    DWORD vertices = quad ? 4 : 3, faces = quad ? 2 : 1, groups;
    BgDocumentVertex local[4] = {0}, *newvertices = NULL;
    BgDocumentFace *newfaces = NULL;
    BgDocumentDrawGroup *newgroups = NULL;
    unsigned char *state = NULL;
    BgDocumentRoom *room;
    BgDocumentLayerData *layer;
    double world[4][3], length, closest = DBL_MAX, point[3];
    *countout = 0;
    *reasonout = "No editable background is loaded.";
    if (!document || !document->rooms || !document->roomcount || !position || !right || !out) { return FALSE; }
    *reasonout = "The primitive has an invalid position, direction, or level scale.";
    if (!isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++)
    { if (!isfinite(position[axis]) || !isfinite(right[axis])) { return FALSE; } }
    length = hypot(right[0], right[2]);
    if (!isfinite(length) || length < 1e-8) { return FALSE; }
    for (DWORD i = 0; i < vertices; i++)
    {
        double side = i == 0 || i == 3 ? -50 : i == 1 || quad ? 50 : 0;
        world[i][0] = position[0] + right[0] / length * side;
        world[i][1] = position[1] + (i >= 2 ? 100 : 0);
        world[i][2] = position[2] + right[2] / length * side;
    }
    *reasonout = "The primitive is outside the available background rooms' coordinate range.";
    if (roomnumber > document->roomcount) { return FALSE; }
    if (!roomnumber)
    {
        BOOL occupied = FALSE;
        for (DWORD i = 1; i <= document->roomcount; i++) { occupied |= document->rooms[i].facecount != 0; }
        for (int axis = 0; axis < 3; axis++) { point[axis] = position[axis] * document->levelscale; }
        for (DWORD i = 1; i <= document->roomcount; i++)
        {
            const BgDocumentRoom *candidate = &document->rooms[i];
            double distance;
            if (occupied && !candidate->facecount) { continue; }
            if (!BgPrimitiveLocal(candidate, document->levelscale, world, vertices, local)) { continue; }
            distance = BgPrimitiveRoomDistance(candidate, point);
            if (distance < closest) { closest = distance; roomnumber = i; }
        }
    }
    if (!roomnumber || roomnumber > USHRT_MAX) { return FALSE; }
    room = &document->rooms[roomnumber]; layer = &room->layers[BG_GEOMETRY_PRIMARY];
    if (!BgPrimitiveLocal(room, document->levelscale, world, vertices, local)) { return FALSE; }
    /* Refuse a collapsed primitive at exceptionally coarse native scales. */
    if ((local[0].x == local[1].x && local[0].z == local[1].z) || local[0].y == local[2].y)
    { *reasonout = "This level's coordinate precision is too coarse for a one-metre primitive."; return FALSE; }
    *reasonout = "The background has reached its vertex, face, or draw-group limit.";
    if (!document->nextvertexid || !document->nextfaceid
        || document->nextvertexid > UINT32_MAX - vertices || document->nextfaceid > UINT32_MAX - faces
        || room->vertexcount > 0x100000u - vertices || document->facecount > UINT32_MAX - faces
        || room->facecount > UINT32_MAX - faces || layer->groupcount == UINT32_MAX) { return FALSE; }
    groups = layer->groupcount + 1;
    /* Match the document's 32-bit buffer limits, including 32-bit hosts.
       The native vertex-stream limit above already bounds that allocation. */
    if (room->facecount > UINT32_MAX / sizeof(*newfaces) - faces
        || groups > UINT32_MAX / sizeof(*newgroups)) { return FALSE; }
    newvertices = malloc((size_t)(room->vertexcount + vertices) * sizeof(*newvertices));
    newfaces = malloc((size_t)(room->facecount + faces) * sizeof(*newfaces));
    newgroups = calloc(groups, sizeof(*newgroups));
    state = malloc(sizeof(commands));
    if (!newvertices || !newfaces || !newgroups || !state)
    {
        free(newvertices); free(newfaces); free(newgroups); free(state);
        *reasonout = "Out of memory creating background geometry."; return FALSE;
    }
    if (room->vertexcount) { memcpy(newvertices, room->vertices, room->vertexcount * sizeof(*newvertices)); }
    if (room->facecount) { memcpy(newfaces, room->faces, room->facecount * sizeof(*newfaces)); }
    if (layer->groupcount) { memcpy(newgroups, layer->groups, layer->groupcount * sizeof(*newgroups)); }
    for (size_t i = 0; i < sizeof(commands) / sizeof(DWORD); i++)
    {
        DWORD word = commands[i/2][i%2];
        state[i*4] = word >> 24; state[i*4+1] = word >> 16;
        state[i*4+2] = word >> 8; state[i*4+3] = word;
    }
    newgroups[groups-1] = (BgDocumentDrawGroup){state, sizeof(commands), sizeof(commands)};
    for (DWORD i = 0; i < vertices; i++)
    {
        local[i].id = document->nextvertexid + i; local[i].room = (unsigned short)roomnumber;
        local[i].r = local[i].g = local[i].b = 192; local[i].a = 255;
        newvertices[room->vertexcount+i] = local[i];
    }
    for (DWORD i = 0; i < faces; i++)
    {
        BgDocumentFace *face = &newfaces[room->facecount+i];
        memset(face, 0, sizeof(*face));
        face->id = document->nextfaceid+i; face->room = (unsigned short)roomnumber;
        face->drawgroup = groups-1; face->layer = BG_GEOMETRY_PRIMARY; face->cullbackfaces = TRUE;
        BgMaterialInit(&face->material); BgMaterialSetTexture(&face->material, BG_TEX_NONE);
        face->textureid = BG_TEX_NONE;
        for (int c = 0; c < 3; c++)
        {
            face->vertexindices[c] = room->vertexcount + corners[i][c];
            newvertices[face->vertexindices[c]].usecount++;
        }
        out[i] = (BgFaceRef){face->id, face->room, face->layer, 0};
    }
    /* Publish only after every allocation and coordinate validation succeeds. */
    free(room->vertices); free(room->faces); free(layer->groups);
    room->vertices = newvertices; room->vertexcount += vertices;
    room->faces = newfaces; room->facecount += faces; room->facecapacity = room->facecount;
    layer->groups = newgroups; layer->groupcount = layer->groupcapacity = groups; layer->sourcepresent = TRUE;
    document->nextvertexid += vertices; document->nextfaceid += faces; document->facecount += faces;
    document->dirty = TRUE; *countout = faces; *reasonout = "";
    return TRUE;
}
