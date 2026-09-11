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
    const double world[][3], DWORD count, BgDocumentVertex local[])
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

/* Shared append path for every primitive: coordinates and winding are checked
 * before changing the document, then one group is added with shared vertices. */
static BOOL BgPrimitiveAppend(BgDocument *document, DWORD roomnumber,
    const double position[3], const double world[][3], DWORD vertices,
    const unsigned char corners[][3], DWORD faces, BgFaceRef *out,
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
    DWORD groups;
    BgDocumentVertex local[BG_PRIMITIVE_MAX_SIDES * 2] = {0}, *newvertices = NULL;
    BgDocumentFace *newfaces = NULL;
    BgDocumentDrawGroup *newgroups = NULL;
    unsigned char *state = NULL;
    BgDocumentRoom *room;
    BgDocumentLayerData *layer;
    double closest = DBL_MAX, point[3];
    *countout = 0;
    *reasonout = "No editable background is loaded.";
    if (!document || !document->rooms || !document->roomcount || !position || !out) { return FALSE; }
    *reasonout = "The primitive has an invalid position or level scale.";
    if (!isfinite(document->levelscale) || document->levelscale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++)
    { if (!isfinite(position[axis])) { return FALSE; } }
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
    /* Rounding to native room coordinates must not collapse or reverse a
     * triangle. This matters for small circles with many closely spaced sides.
     * Use doubles: differences of signed 16-bit coordinates can overflow an
     * integer cross product. Compare against the intended world-space winding. */
    for (DWORD i = 0; i < faces; i++)
    {
        double a[3], b[3], wa[3], wb[3], alignment = 0;
        const BgDocumentVertex *v[3];
        for (int c = 0; c < 3; c++) { v[c] = &local[corners[i][c]]; }
        a[0] = (double)v[1]->x - v[0]->x; a[1] = (double)v[1]->y - v[0]->y; a[2] = (double)v[1]->z - v[0]->z;
        b[0] = (double)v[2]->x - v[0]->x; b[1] = (double)v[2]->y - v[0]->y; b[2] = (double)v[2]->z - v[0]->z;
        for (int axis = 0; axis < 3; axis++)
        {
            wa[axis] = world[corners[i][1]][axis] - world[corners[i][0]][axis];
            wb[axis] = world[corners[i][2]][axis] - world[corners[i][0]][axis];
        }
        for (int axis = 0; axis < 3; axis++)
        {
            int j = (axis+1)%3, k = (axis+2)%3;
            alignment += (a[j]*b[k] - a[k]*b[j]) * (wa[j]*wb[k] - wa[k]*wb[j]);
        }
        if (!(alignment > 0))
        {
            *reasonout = "This shape is too small for the level's coordinate precision. Increase its size or use fewer sides.";
            return FALSE;
        }
    }
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

BOOL BgDocumentAddPrimitive(BgDocument *document, BOOL quad, DWORD room,
    const double position[3], const double right[3], BgFaceRef out[2],
    DWORD *countout, const char **reasonout)
{
    static const unsigned char corners[2][3] = {{0,1,2}, {0,2,3}};
    DWORD vertices = quad ? 4 : 3;
    double world[4][3], length;
    *countout = 0;
    *reasonout = "The primitive has an invalid position or direction.";
    if (!position || !right) { return FALSE; }
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
    return BgPrimitiveAppend(document, room, position, world, vertices,
        corners, quad ? 2 : 1, out, countout, reasonout);
}

BOOL BgDocumentAddRoundPrimitive(BgDocument *document, BOOL cylinder, DWORD room,
    const double position[3], double radius, double height, DWORD sides,
    BgFaceRef out[BG_PRIMITIVE_MAX_FACES], DWORD *countout, const char **reasonout)
{
    double world[BG_PRIMITIVE_MAX_SIDES * 2][3];
    unsigned char corners[BG_PRIMITIVE_MAX_FACES][3];
    DWORD faces = 0;
    *countout = 0;
    *reasonout = "Use a positive radius and height, and 3 to 64 sides.";
    if (!position || !isfinite(radius) || radius <= 0
        || (cylinder && (!isfinite(height) || height <= 0))
        || sides < 3 || sides > BG_PRIMITIVE_MAX_SIDES) { return FALSE; }
    /* Start on +X and proceed toward -Z: viewed from above, the perimeter
     * winds counterclockwise. Both rings lie exactly in horizontal XZ planes. */
    for (DWORD i = 0; i < sides; i++)
    {
        double angle = 6.28318530717958647692 * i / sides;
        world[i][0] = position[0] + radius * cos(angle);
        world[i][1] = position[1];
        world[i][2] = position[2] - radius * sin(angle);
        if (cylinder)
        {
            memcpy(world[i+sides], world[i], sizeof(world[i]));
            world[i+sides][1] += height;
        }
    }
    if (cylinder)
    {
        /* Two outward-facing triangles per side; no top or bottom faces. */
        for (DWORD i = 0; i < sides; i++)
        {
            DWORD next = (i+1)%sides;
            corners[faces][0] = i; corners[faces][1] = next; corners[faces++][2] = next+sides;
            corners[faces][0] = i; corners[faces][1] = next+sides; corners[faces++][2] = i+sides;
        }
    }
    else
    {
        /* Root the fan at perimeter vertex 0, with no centre vertex. */
        for (DWORD i = 1; i+1 < sides; i++)
        { corners[faces][0] = 0; corners[faces][1] = i; corners[faces++][2] = i+1; }
    }
    return BgPrimitiveAppend(document, room, position, world, cylinder ? sides*2 : sides,
        corners, faces, out, countout, reasonout);
}
