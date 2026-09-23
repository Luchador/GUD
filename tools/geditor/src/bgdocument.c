/*
 * Editable, room-aware view of a GoldenEye background.
 *
 * BgDocument supplies stable room, face, and shared-vertex identities to the
 * editor. It also retains opaque display-list state so the BG compiler can
 * regenerate geometry without discarding authored rendering behavior. Both
 * primary and secondary display lists reference the same room vertex table,
 * so preserving those indices is essential for topology editing and vertex
 * painting.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include "scaling.h"
#include <stdint.h>

#include "bgdocument.h"

#define BGDOC_ROOM_RECORD_SIZE 24u
#define BGDOC_MAX_ROOMS        256u
#define BGDOC_G_VTX               0x04
#define BGDOC_G_TRI4              0xB1
#define BGDOC_G_CLEARGEOMETRYMODE 0xB6
#define BGDOC_G_SETGEOMETRYMODE   0xB7
#define BGDOC_G_ENDDL             0xB8
#define BGDOC_G_TRI1              0xBF

#define BGDOC_G_CULL_BACK 0x00002000u


static DWORD BgDocumentRead32(const unsigned char *src)
{
    return ((DWORD)src[0] << 24) | ((DWORD)src[1] << 16)
         | ((DWORD)src[2] << 8) | (DWORD)src[3];
}


static short BgDocumentRead16(const unsigned char *src)
{
    return (short)(((unsigned int)src[0] << 8) | src[1]);
}


static float BgDocumentReadFloat(const unsigned char *src)
{
    union { DWORD word; float value; } convert;

    convert.word = BgDocumentRead32(src);
    return convert.value;
}


static DWORD BgDocumentBlockSize(const unsigned char *data, DWORD size,
                                 DWORD offset)
{
    DWORD blocksize;

    if (offset < 4 || offset >= size)
    {
        return 0;
    }

    blocksize = BgDocumentRead32(data + offset - 4);
    if (blocksize == 0 || blocksize > size - offset)
    {
        return 0;
    }

    return blocksize;
}


static BOOL BgDocumentAppendFace(BgDocument *document,
                                 BgDocumentRoom *room,
                                 unsigned short roomnumber,
                                 const DWORD vertexindices[3],
                                 DWORD drawgroup,
                                 BgGeometryLayer layer,
                                 const BgMaterial *material,
                                 BOOL cullbackfaces)
{
    BgDocumentFace *face;
    DWORD nextcapacity;
    BgDocumentFace *grown;
    int corner;

    if (room->facecount == room->facecapacity)
    {
        nextcapacity = room->facecapacity ? room->facecapacity * 2 : 128;
        if (nextcapacity < room->facecapacity
            || nextcapacity > ((DWORD)-1) / sizeof(*room->faces))
        {
            return FALSE;
        }

        grown = (BgDocumentFace *)realloc(room->faces,
                    (size_t)nextcapacity * sizeof(*room->faces));
        if (grown == NULL)
        {
            return FALSE;
        }

        room->faces = grown;
        room->facecapacity = nextcapacity;
    }

    if (document->nextfaceid == BG_FACE_ID_NONE)
    {
        return FALSE;
    }

    face = &room->faces[room->facecount++];
    ZeroMemory(face, sizeof(*face));
    face->id = document->nextfaceid++;
    face->room = roomnumber;
    face->drawgroup = drawgroup;
    face->layer = (unsigned char)layer;
    face->cullbackfaces = (unsigned char)cullbackfaces;
    face->material = *material;
    face->textureid = BgMaterialTextureId(material);

    for (corner = 0; corner < 3; corner++)
    {
        face->vertexindices[corner] = vertexindices[corner];
        room->vertices[vertexindices[corner]].usecount++;
    }

    document->facecount++;
    return TRUE;
}


static BOOL BgDocumentAppendDrawGroup(BgDocumentLayerData *layer)
{
    BgDocumentDrawGroup *grown;
    DWORD nextcapacity;

    if (layer->groupcount < layer->groupcapacity)
    {
        ZeroMemory(&layer->groups[layer->groupcount],
                   sizeof(layer->groups[layer->groupcount]));
        layer->groupcount++;
        return TRUE;
    }

    nextcapacity = layer->groupcapacity ? layer->groupcapacity * 2 : 8;
    if (nextcapacity < layer->groupcapacity
        || nextcapacity > ((DWORD)-1) / sizeof(*layer->groups))
    {
        return FALSE;
    }

    grown = (BgDocumentDrawGroup *)realloc(layer->groups,
                (size_t)nextcapacity * sizeof(*layer->groups));
    if (grown == NULL)
    {
        return FALSE;
    }

    layer->groups = grown;
    layer->groupcapacity = nextcapacity;
    ZeroMemory(&layer->groups[layer->groupcount],
               sizeof(layer->groups[layer->groupcount]));
    layer->groupcount++;
    return TRUE;
}


static BOOL BgDocumentAppendGroupCommand(BgDocumentDrawGroup *group,
                                         const unsigned char command[8])
{
    DWORD nextcapacity;
    unsigned char *grown;

    if (group->commandsize > (DWORD)-1 - 8)
    {
        return FALSE;
    }

    if (group->commandsize + 8 > group->commandcapacity)
    {
        nextcapacity = group->commandcapacity
            ? group->commandcapacity * 2 : 64;
        if (nextcapacity < group->commandcapacity)
        {
            return FALSE;
        }
        while (nextcapacity < group->commandsize + 8)
        {
            if (nextcapacity > (DWORD)-1 / 2)
            {
                return FALSE;
            }
            nextcapacity *= 2;
        }

        grown = (unsigned char *)realloc(group->commands, nextcapacity);
        if (grown == NULL)
        {
            return FALSE;
        }
        group->commands = grown;
        group->commandcapacity = nextcapacity;
    }

    memcpy(group->commands + group->commandsize, command, 8);
    group->commandsize += 8;
    return TRUE;
}


static BOOL BgDocumentReadTriangleIndices(const unsigned char *command,
                                          int triangle, int indices[3])
{
    if (command[0] == BGDOC_G_TRI1)
    {
        indices[0] = command[5] / 10;
        indices[1] = command[6] / 10;
        indices[2] = command[7] / 10;
        return TRUE;
    }

    if (command[0] != BGDOC_G_TRI4 || triangle < 0 || triangle > 3)
    {
        return FALSE;
    }

    switch (triangle)
    {
    case 0:
        indices[0] = command[7] & 0xF;
        indices[1] = command[7] >> 4;
        indices[2] = command[3] & 0xF;
        break;
    case 1:
        indices[0] = command[6] & 0xF;
        indices[1] = command[6] >> 4;
        indices[2] = command[3] >> 4;
        break;
    case 2:
        indices[0] = command[5] & 0xF;
        indices[1] = command[5] >> 4;
        indices[2] = command[2] & 0xF;
        break;
    default:
        indices[0] = command[4] & 0xF;
        indices[1] = command[4] >> 4;
        indices[2] = command[2] >> 4;
        break;
    }

    return TRUE;
}


static BOOL BgDocumentWalkDisplayList(BgDocument *document,
                                      BgDocumentRoom *room,
                                      unsigned short roomnumber,
                                      const unsigned char *data, DWORD size,
                                      DWORD gdloffset, DWORD gdlsize,
                                      BgGeometryLayer layer)
{
    BgDocumentLayerData *layerdata = &room->layers[layer];
    DWORD pc;
    DWORD cachevertices[16];
    BOOL cachevalid[16];
    BgMaterial material;
    BOOL cullbackfaces = FALSE;
    DWORD drawgroup = 0;
    BOOL grouphasfaces = FALSE;

    BgMaterialInit(&material);
    ZeroMemory(cachevertices, sizeof(cachevertices));
    ZeroMemory(cachevalid, sizeof(cachevalid));
    layerdata->sourcepresent = TRUE;
    if (!BgDocumentAppendDrawGroup(layerdata))
    {
        return FALSE;
    }

    for (pc = gdloffset;
         pc + 8 <= gdloffset + gdlsize && pc + 8 <= size;
         pc += 8)
    {
        const unsigned char *command = data + pc;

        if (command[0] == BGDOC_G_ENDDL)
        {
            return TRUE;
        }

        if (command[0] == BGDOC_G_VTX)
        {
            DWORD addressoffset = BgDocumentRead32(command + 4)
                                & 0x00FFFFFFu;
            DWORD batchcount = ((command[1] >> 4) & 0xF) + 1;
            DWORD batchv0 = command[1] & 0xF;
            DWORD batchfirst;
            DWORD vertex;

            if ((addressoffset & 15) != 0
                || addressoffset / 16 >= room->vertexcount
                || batchcount > room->vertexcount - addressoffset / 16
                || batchv0 + batchcount > 16)
            {
                continue;
            }

            batchfirst = addressoffset / 16;
            for (vertex = 0; vertex < batchcount; vertex++)
            {
                cachevertices[batchv0 + vertex] = batchfirst + vertex;
                cachevalid[batchv0 + vertex] = TRUE;
            }
            continue;
        }

        if (command[0] == BGDOC_G_TRI1 || command[0] == BGDOC_G_TRI4)
        {
            int trianglecount = command[0] == BGDOC_G_TRI1 ? 1 : 4;
            int triangle;

            for (triangle = 0; triangle < trianglecount; triangle++)
            {
                int cacheindices[3];
                DWORD vertexindices[3];
                int corner;
                BOOL valid = TRUE;

                BgDocumentReadTriangleIndices(command, triangle,
                                               cacheindices);

                /* Only 0,0,0 in a TRI4 is an unused packed slot. TRI1 and
                   nonzero degenerate triangles remain authored geometry. */
                if (command[0] == BGDOC_G_TRI4
                    && cacheindices[0] == 0
                    && cacheindices[1] == 0
                    && cacheindices[2] == 0)
                {
                    continue;
                }

                for (corner = 0; corner < 3; corner++)
                {
                    if (cacheindices[corner] < 0
                        || cacheindices[corner] >= 16
                        || !cachevalid[cacheindices[corner]])
                    {
                        valid = FALSE;
                        break;
                    }
                    vertexindices[corner] =
                        cachevertices[cacheindices[corner]];
                }

                if (valid
                    && !BgDocumentAppendFace(document, room, roomnumber,
                                             vertexindices,
                                             drawgroup,
                                             layer, &material,
                                             cullbackfaces))
                {
                    return FALSE;
                }
                else if (valid)
                {
                    grouphasfaces = TRUE;
                }
            }

            continue;
        }

        /* Preserve every non-geometry command. A new group starts when state
         * changes after geometry, anchoring that transition even if all faces
         * in the preceding group are later deleted. */
        if (grouphasfaces)
        {
            if (!BgDocumentAppendDrawGroup(layerdata))
            {
                return FALSE;
            }
            drawgroup++;
            grouphasfaces = FALSE;
        }

        if (!BgDocumentAppendGroupCommand(&layerdata->groups[drawgroup],
                                          command))
        {
            return FALSE;
        }

        BgMaterialReadCommand(&material, BgDocumentRead32(command),
                               BgDocumentRead32(command + 4));
        if (command[0] == BGDOC_G_SETGEOMETRYMODE
                 && (BgDocumentRead32(command + 4) & BGDOC_G_CULL_BACK))
        {
            cullbackfaces = TRUE;
        }
        else if (command[0] == BGDOC_G_CLEARGEOMETRYMODE
                 && (BgDocumentRead32(command + 4) & BGDOC_G_CULL_BACK))
        {
            cullbackfaces = FALSE;
        }
    }

    /* A truncated list is still bounded safely, matching the preview parser's
     * behavior. Its complete faces remain useful to the editor. */
    return TRUE;
}


static BOOL BgDocumentLoadRoomVertices(BgDocument *document,
                                       BgDocumentRoom *room,
                                       unsigned short roomnumber,
                                       const unsigned char *vertices,
                                       DWORD vertexsize)
{
    DWORD index;

    if ((vertexsize & 15) != 0)
    {
        return FALSE;
    }

    room->vertexcount = vertexsize / 16;
    if (room->vertexcount == 0)
    {
        return TRUE;
    }

    room->vertices = (BgDocumentVertex *)calloc(room->vertexcount,
                                                 sizeof(*room->vertices));
    if (room->vertices == NULL)
    {
        return FALSE;
    }

    for (index = 0; index < room->vertexcount; index++)
    {
        const unsigned char *src = vertices + index * 16;
        BgDocumentVertex *dst = &room->vertices[index];

        if (document->nextvertexid == 0)
        {
            return FALSE;
        }
        dst->id = document->nextvertexid++;
        dst->room = roomnumber;
        dst->x = BgDocumentRead16(src + 0);
        dst->y = BgDocumentRead16(src + 2);
        dst->z = BgDocumentRead16(src + 4);
        dst->flag = (unsigned short)BgDocumentRead16(src + 6);
        dst->s = BgDocumentRead16(src + 8);
        dst->t = BgDocumentRead16(src + 10);
        dst->r = src[12];
        dst->g = src[13];
        dst->b = src[14];
        dst->a = src[15];
    }

    return TRUE;
}


BOOL BgDocumentLoad(const unsigned char *data, DWORD size, float levelscale,
                    BgDocument *out, const char **reasonout)
{
    DWORD roomtable;
    DWORD roomcount = 0;
    DWORD roomindex;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (data == NULL || size < 0x40)
    {
        *reasonout = "bg file is too small to create an editable document.";
        return FALSE;
    }
    if (!(levelscale > 0.0f))
    {
        *reasonout = "level scale must be greater than zero.";
        return FALSE;
    }
    if (BgDocumentRead32(data) != 0)
    {
        *reasonout = "bg file is a single display list, not a room document.";
        return FALSE;
    }

    roomtable = BgDocumentRead32(data + 4) & 0x00FFFFFFu;
    if (roomtable >= size)
    {
        *reasonout = "bg room list offset is outside the file.";
        return FALSE;
    }

    for (roomindex = 1; roomindex < BGDOC_MAX_ROOMS; roomindex++)
    {
        DWORD displacement = roomindex * BGDOC_ROOM_RECORD_SIZE;
        DWORD record;

        if (displacement > size - roomtable
            || BGDOC_ROOM_RECORD_SIZE > size - roomtable - displacement)
        {
            break;
        }

        record = roomtable + displacement;
        if (BgDocumentRead32(data + record + 4) == 0)
        {
            break;
        }
        roomcount = roomindex;
    }

    if (roomcount == 0)
    {
        *reasonout = "bg file contains no rooms.";
        return FALSE;
    }

    out->rooms = (BgDocumentRoom *)calloc(roomcount + 1,
                                           sizeof(*out->rooms));
    if (out->rooms == NULL)
    {
        *reasonout = "out of memory creating the editable bg document.";
        return FALSE;
    }

    out->roomcount = roomcount;
    out->nextvertexid = 1;
    out->nextfaceid = 1;
    out->levelscale = levelscale;

    for (roomindex = 1; roomindex <= roomcount; roomindex++)
    {
        DWORD record = roomtable + roomindex * BGDOC_ROOM_RECORD_SIZE;
        DWORD vertexoffset = BgDocumentRead32(data + record) & 0x00FFFFFFu;
        DWORD primaryoffset = BgDocumentRead32(data + record + 4)
                            & 0x00FFFFFFu;
        DWORD secondaryoffset = BgDocumentRead32(data + record + 8)
                              & 0x00FFFFFFu;
        DWORD vertexsize = BgDocumentBlockSize(data, size, vertexoffset);
        DWORD primarysize = BgDocumentBlockSize(data, size, primaryoffset);
        DWORD secondarysize = BgDocumentBlockSize(data, size,
                                                  secondaryoffset);
        BgDocumentRoom *room = &out->rooms[roomindex];

        room->origin[0] = BgDocumentReadFloat(data + record + 12);
        room->origin[1] = BgDocumentReadFloat(data + record + 16);
        room->origin[2] = BgDocumentReadFloat(data + record + 20);

        if (vertexoffset == 0 || vertexsize == 0)
        {
            continue;
        }

        if (!BgDocumentLoadRoomVertices(out, room,
                                        (unsigned short)roomindex,
                                        data + vertexoffset,
                                        vertexsize)
            || (primaryoffset != 0 && primarysize != 0
                && !BgDocumentWalkDisplayList(out, room,
                    (unsigned short)roomindex, data, size,
                    primaryoffset, primarysize, BG_GEOMETRY_PRIMARY))
            || (secondaryoffset != 0 && secondarysize != 0
                && !BgDocumentWalkDisplayList(out, room,
                    (unsigned short)roomindex, data, size,
                    secondaryoffset, secondarysize,
                    BG_GEOMETRY_SECONDARY)))
        {
            BgDocumentFree(out);
            *reasonout = "out of memory decoding the editable bg document.";
            return FALSE;
        }
    }

    /* Room transfers in older editors replayed whole layers, including the
     * state of faces left behind. Repair that accumulated history on load.
     * Material packets are already captured on faces; stripping those alone
     * is not an edit. Only redundant native state requires a repair save. */
    for (roomindex = 1; roomindex <= roomcount; roomindex++)
    {
        BOOL changed;
        if (!BgDocumentCompactRoomState(&out->rooms[roomindex], &changed, reasonout))
        { BgDocumentFree(out); return FALSE; }
        if (changed) { out->dirty = TRUE; }
    }

    /* As with the original overlay, malformed portals must not prevent
     * inspecting the rest of a level. Such tables cannot be edited. */
    if (BgLoadPortals(data, size, levelscale, &out->portals, &out->portalwarning))
    { out->portalwarning = NULL; }
    return TRUE;
}


BOOL BgDocumentClone(const BgDocument *source, BgDocument *out,
                     const char **reasonout)
{
    DWORD roomindex;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (source == NULL || source->rooms == NULL)
    {
        *reasonout = "there is no bg document to copy.";
        return FALSE;
    }

    out->rooms = (BgDocumentRoom *)calloc(source->roomcount + 1,
                                           sizeof(*out->rooms));
    if (out->rooms == NULL)
    {
        *reasonout = "out of memory copying the bg document.";
        return FALSE;
    }

    out->roomcount = source->roomcount;
    out->facecount = source->facecount;
    out->nextvertexid = source->nextvertexid;
    out->nextfaceid = source->nextfaceid;
    out->levelscale = source->levelscale;
    out->dirty = source->dirty;
    out->portalwarning = source->portalwarning;
    if (source->portals.portalcount)
    {
        out->portals.portals = malloc(source->portals.portalcount * sizeof(*out->portals.portals));
        if (!out->portals.portals)
        {
            BgDocumentFree(out);
            *reasonout = "out of memory copying bg portals.";
            return FALSE;
        }
        out->portals.portalcount = source->portals.portalcount;
        memcpy(out->portals.portals, source->portals.portals,
            source->portals.portalcount * sizeof(*out->portals.portals));
    }

    for (roomindex = 0; roomindex <= source->roomcount; roomindex++)
    {
        const BgDocumentRoom *srcroom = &source->rooms[roomindex];
        BgDocumentRoom *dstroom = &out->rooms[roomindex];
        int layer;

        memcpy(dstroom->origin, srcroom->origin, sizeof(dstroom->origin));

        if (srcroom->vertexcount > 0)
        {
            dstroom->vertices = (BgDocumentVertex *)malloc(
                (size_t)srcroom->vertexcount * sizeof(*dstroom->vertices));
            if (dstroom->vertices == NULL)
            {
                BgDocumentFree(out);
                *reasonout = "out of memory copying bg vertices.";
                return FALSE;
            }

            memcpy(dstroom->vertices, srcroom->vertices,
                   (size_t)srcroom->vertexcount * sizeof(*dstroom->vertices));
            dstroom->vertexcount = srcroom->vertexcount;
        }

        if (srcroom->facecount > 0)
        {
            dstroom->faces = (BgDocumentFace *)malloc(
                (size_t)srcroom->facecount * sizeof(*dstroom->faces));
            if (dstroom->faces == NULL)
            {
                BgDocumentFree(out);
                *reasonout = "out of memory copying bg faces.";
                return FALSE;
            }

            memcpy(dstroom->faces, srcroom->faces,
                   (size_t)srcroom->facecount * sizeof(*dstroom->faces));
            dstroom->facecount = srcroom->facecount;
            dstroom->facecapacity = srcroom->facecount;
        }

        for (layer = 0; layer < 2; layer++)
        {
            const BgDocumentLayerData *srclayer = &srcroom->layers[layer];
            BgDocumentLayerData *dstlayer = &dstroom->layers[layer];
            DWORD groupindex;

            dstlayer->sourcepresent = srclayer->sourcepresent;
            if (srclayer->groupcount == 0)
            {
                continue;
            }

            dstlayer->groups = (BgDocumentDrawGroup *)calloc(
                srclayer->groupcount, sizeof(*dstlayer->groups));
            if (dstlayer->groups == NULL)
            {
                BgDocumentFree(out);
                *reasonout = "out of memory copying bg draw groups.";
                return FALSE;
            }
            dstlayer->groupcount = srclayer->groupcount;
            dstlayer->groupcapacity = srclayer->groupcount;

            for (groupindex = 0; groupindex < srclayer->groupcount;
                 groupindex++)
            {
                const BgDocumentDrawGroup *srcgroup =
                    &srclayer->groups[groupindex];
                BgDocumentDrawGroup *dstgroup =
                    &dstlayer->groups[groupindex];

                if (srcgroup->commandsize == 0)
                {
                    continue;
                }

                dstgroup->commands = (unsigned char *)malloc(
                    srcgroup->commandsize);
                if (dstgroup->commands == NULL)
                {
                    BgDocumentFree(out);
                    *reasonout = "out of memory copying bg display-list state.";
                    return FALSE;
                }
                memcpy(dstgroup->commands, srcgroup->commands,
                       srcgroup->commandsize);
                dstgroup->commandsize = srcgroup->commandsize;
                dstgroup->commandcapacity = srcgroup->commandsize;
            }
        }
    }

    return TRUE;
}


void BgDocumentFree(BgDocument *document)
{
    DWORD room;

    if (document == NULL)
    {
        return;
    }

    for (room = 0; document->rooms != NULL
         && room <= document->roomcount; room++)
    {
        int layer;

        for (layer = 0; layer < 2; layer++)
        {
            BgDocumentLayerData *layerdata =
                &document->rooms[room].layers[layer];
            DWORD group;

            for (group = 0; group < layerdata->groupcount; group++)
            {
                free(layerdata->groups[group].commands);
            }
            free(layerdata->groups);
        }
        free(document->rooms[room].vertices);
        free(document->rooms[room].faces);
    }
    free(document->rooms);
    BgPortalFileFree(&document->portals);
    ZeroMemory(document, sizeof(*document));
}


BOOL BgDocumentSetPortalRooms(BgDocument *document, DWORD index, DWORD room1, DWORD room2,
                              BOOL *changed, const char **reasonout)
{
    BgPortal *portal;
    *changed = FALSE;
    *reasonout = "The selected portal is no longer available.";
    if (!document || !document->rooms || document->portalwarning || !document->portals.portals
        || index >= document->portals.portalcount) { return FALSE; }
    if (!room1 || !room2 || room1 > document->roomcount || room2 > document->roomcount
        || room1 > 255 || room2 > 255)
    { *reasonout = "Choose two existing rooms from this level."; return FALSE; }
    if (room1 == room2)
    { *reasonout = "A portal must connect two different rooms."; return FALSE; }
    portal = &document->portals.portals[index];
    *changed = portal->connectedroom1 != room1 || portal->connectedroom2 != room2;
    portal->connectedroom1 = (unsigned char)room1;
    portal->connectedroom2 = (unsigned char)room2;
    document->dirty |= *changed;
    *reasonout = "";
    return TRUE;
}


BOOL BgDocumentSetPortalMargin(BgDocument *document, DWORD index, double margin,
                               BOOL *changed, const char **reasonout)
{
    unsigned char encoded;
    BgPortal *portal;
    *changed = FALSE;
    *reasonout = "The selected portal is no longer available.";
    if (!document || !document->rooms || document->portalwarning || !document->portals.portals
        || index >= document->portals.portalcount) { return FALSE; }
    if (!isfinite(document->levelscale) || document->levelscale <= 0
        || !isfinite(margin) || margin < 0 || margin > 122880.0 / document->levelscale
        /* Division followed by multiplication can land one ULP above the
           maximum. The world-unit range check above makes this clamp safe. */
        || !BgPortalEncodeMargin(fmin(margin * (double)document->levelscale, 122880.0), &encoded))
    { *reasonout = "Enter a nonnegative, finite margin within this level's supported range."; return FALSE; }
    portal = &document->portals.portals[index];
    /* Preserve authored aliases, including zero with a nonzero exponent. */
    if (BgPortalGetMargin(portal) != (encoded & 15u) * .25f * (1u << (encoded >> 4)))
    {
        portal->controlbytes2 = encoded;
        document->dirty = TRUE;
        *changed = TRUE;
    }
    *reasonout = "";
    return TRUE;
}


BOOL BgDocumentDeleteFaces(BgDocument *document, const BgFaceRef *refs,
                           DWORD refcount, DWORD *deletedout,
                           const char **reasonout)
{
    DWORD refindex;
    DWORD roomindex;
    DWORD deleted = 0;

    if (deletedout != NULL)
    {
        *deletedout = 0;
    }
    if (reasonout != NULL)
    {
        *reasonout = "";
    }

    if (document == NULL || document->rooms == NULL
        || refs == NULL || refcount == 0)
    {
        if (reasonout != NULL)
        {
            *reasonout = "there are no bg faces to delete.";
        }
        return FALSE;
    }

    /* Validate the entire selection before changing the document. */
    for (refindex = 0; refindex < refcount; refindex++)
    {
        DWORD duplicate;

        if (BgDocumentFindFace(document, &refs[refindex], NULL) == NULL)
        {
            if (reasonout != NULL)
            {
                *reasonout = "a selected bg face no longer exists.";
            }
            return FALSE;
        }
        for (duplicate = 0; duplicate < refindex; duplicate++)
        {
            if (refs[duplicate].faceid == refs[refindex].faceid
                && refs[duplicate].room == refs[refindex].room
                && refs[duplicate].layer == refs[refindex].layer)
            {
                if (reasonout != NULL)
                {
                    *reasonout = "the bg face selection contains duplicates.";
                }
                return FALSE;
            }
        }
    }

    for (roomindex = 1; roomindex <= document->roomcount; roomindex++)
    {
        BgDocumentRoom *room = &document->rooms[roomindex];
        DWORD sourceindex;
        DWORD targetindex = 0;

        for (sourceindex = 0; sourceindex < room->facecount; sourceindex++)
        {
            BgDocumentFace *face = &room->faces[sourceindex];
            BOOL remove = FALSE;

            for (refindex = 0; refindex < refcount; refindex++)
            {
                if (face->id == refs[refindex].faceid
                    && face->room == refs[refindex].room
                    && face->layer == refs[refindex].layer)
                {
                    int corner;

                    remove = TRUE;
                    for (corner = 0; corner < 3; corner++)
                    {
                        DWORD vertexindex = face->vertexindices[corner];

                        if (vertexindex < room->vertexcount
                            && room->vertices[vertexindex].usecount > 0)
                        {
                            room->vertices[vertexindex].usecount--;
                        }
                    }
                    deleted++;
                    break;
                }
            }

            if (!remove)
            {
                if (targetindex != sourceindex)
                {
                    room->faces[targetindex] = room->faces[sourceindex];
                }
                targetindex++;
            }
        }
        room->facecount = targetindex;
    }

    if (deleted != refcount || deleted > document->facecount)
    {
        if (reasonout != NULL)
        {
            *reasonout = "the bg face deletion was incomplete.";
        }
        return FALSE;
    }

    document->facecount -= deleted;
    if (deletedout != NULL)
    {
        *deletedout = deleted;
    }
    return TRUE;
}


BOOL BgDocumentFlipFaces(BgDocument *document, const BgFaceRef *refs,
                         DWORD refcount, const char **reasonout)
{
    DWORD index;

    if (reasonout != NULL) { *reasonout = ""; }
    if (document == NULL || document->rooms == NULL
        || refs == NULL || refcount == 0)
    {
        if (reasonout != NULL) { *reasonout = "there are no bg faces to flip."; }
        return FALSE;
    }

    /* Validate the entire selection first. In particular, a duplicate must
       not silently flip the same face twice and leave its winding unchanged. */
    for (index = 0; index < refcount; index++)
    {
        DWORD duplicate;
        if (BgDocumentFindFace(document, &refs[index], NULL) == NULL)
        {
            if (reasonout != NULL) { *reasonout = "a selected bg face no longer exists."; }
            return FALSE;
        }
        for (duplicate = 0; duplicate < index; duplicate++)
        {
            if (refs[duplicate].faceid == refs[index].faceid
                && refs[duplicate].room == refs[index].room
                && refs[duplicate].layer == refs[index].layer)
            {
                if (reasonout != NULL) { *reasonout = "the bg face selection contains duplicates."; }
                return FALSE;
            }
        }
    }

    for (index = 0; index < refcount; index++)
    {
        BgDocumentFace *face = (BgDocumentFace *)BgDocumentFindFace(document, &refs[index], NULL);
        DWORD vertex = face->vertexindices[1];
        /* UVs and colors belong to the vertex records, so only reorder the
           references. Shared vertices and neighboring faces remain intact. */
        face->vertexindices[1] = face->vertexindices[2];
        face->vertexindices[2] = vertex;
        face->uvseams = ((face->uvseams & 1) << 2) | (face->uvseams & 2) | ((face->uvseams & 4) >> 2);
    }
    document->dirty = TRUE;
    return TRUE;
}


static BOOL BgDocumentFaceHasArea(const BgDocumentRoom *room, const BgDocumentFace *face)
{
    const BgDocumentVertex *a = &room->vertices[face->vertexindices[0]];
    const BgDocumentVertex *b = &room->vertices[face->vertexindices[1]];
    const BgDocumentVertex *c = &room->vertices[face->vertexindices[2]];
    int64_t abx = (int64_t)b->x - a->x, aby = (int64_t)b->y - a->y, abz = (int64_t)b->z - a->z;
    int64_t acx = (int64_t)c->x - a->x, acy = (int64_t)c->y - a->y, acz = (int64_t)c->z - a->z;

    /* Exact local-coordinate arithmetic catches coincident AND collinear
       corners without an epsilon that could discard small valid triangles.
       Products of 16-bit coordinate differences can overflow 32 bits. */
    return aby * acz != abz * acy || abz * acx != abx * acz || abx * acy != aby * acx;
}

BOOL BgDocumentFindCollapsedFaces(const BgDocument *before, const BgDocument *after,
                                  BgFaceRef **facesout, DWORD *countout,
                                  const char **reasonout)
{
    BgFaceRef *faces = NULL;
    DWORD count = 0, capacity = 0, roomindex;

    *facesout = NULL;
    *countout = 0;
    *reasonout = "The background topology changed while checking the snap.";
    if (before == NULL || after == NULL || before->rooms == NULL || after->rooms == NULL
        || before->roomcount != after->roomcount || before->facecount != after->facecount)
    {
        return FALSE;
    }
    for (roomindex = 1; roomindex <= after->roomcount; roomindex++)
    {
        const BgDocumentRoom *oldroom = &before->rooms[roomindex];
        const BgDocumentRoom *room = &after->rooms[roomindex];
        DWORD index;

        if (room->facecount != oldroom->facecount || room->vertexcount != oldroom->vertexcount
            || (room->facecount && (!room->faces || !oldroom->faces
                                    || !room->vertices || !oldroom->vertices))) { goto fail; }
        for (index = 0; index < room->facecount; index++)
        {
            const BgDocumentFace *oldface = &oldroom->faces[index];
            const BgDocumentFace *face = &room->faces[index];
            int corner;

            if (face->id != oldface->id || face->room != oldface->room
                || face->layer != oldface->layer) { goto fail; }
            for (corner = 0; corner < 3; corner++)
            {
                if (face->vertexindices[corner] >= room->vertexcount
                    || face->vertexindices[corner] != oldface->vertexindices[corner]) { goto fail; }
            }
            if (BgDocumentFaceHasArea(room, face) || !BgDocumentFaceHasArea(oldroom, oldface))
            {
                continue;
            }
            if (count == capacity)
            {
                DWORD next = capacity ? capacity * 2 : 16;
                BgFaceRef *grown;

                if (next < capacity || next > (DWORD)-1 / sizeof(*faces))
                {
                    *reasonout = "Too many collapsed background triangles.";
                    goto fail;
                }
                grown = realloc(faces, (size_t)next * sizeof(*faces));
                if (grown == NULL)
                {
                    *reasonout = "Out of memory collecting collapsed background triangles.";
                    goto fail;
                }
                faces = grown;
                capacity = next;
            }
            faces[count++] = (BgFaceRef){face->id, face->room, face->layer, 0};
        }
    }
    *facesout = faces;
    *countout = count;
    *reasonout = "";
    return TRUE;

fail:
    free(faces);
    return FALSE;
}

static int BgDocumentCompareVertexRefs(const void *left, const void *right)
{
    const BgDocumentVertexRef *a = (const BgDocumentVertexRef *)left;
    const BgDocumentVertexRef *b = (const BgDocumentVertexRef *)right;

    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    if (a->index != b->index) { return a->index < b->index ? -1 : 1; }
    return 0;
}


BOOL BgDocumentTranslateVertices(BgDocument *document, const BgDocumentVertexRef *refs,
                              DWORD refcount, const double offset[3],
                              double appliedoffset[3], DWORD *movedout,
                              const char **reasonout)
{
    BgDocumentVertexRef *vertices = NULL;
    size_t vertexcount = 0;
    size_t unique = 0;
    size_t i;
    int delta[3];
    int axis;
    const char *reason = "";

    if (movedout != NULL) { *movedout = 0; }
    if (reasonout != NULL) { *reasonout = ""; }

    if (document == NULL || document->rooms == NULL
        || refs == NULL || refcount == 0 || offset == NULL)
    {
        reason = "There are no background vertices to move.";
        goto fail;
    }
    if (!isfinite(document->levelscale) || document->levelscale <= 0.0f)
    {
        reason = "The background has an invalid world scale.";
        goto fail;
    }

    /* Round the displacement once, uniformly for every selected vertex.
       The largest possible displacement between two signed 16-bit positions
       is 65535; reject larger values before converting them to integers. */
    for (axis = 0; axis < 3; axis++)
    {
        double localoffset = round(offset[axis] * document->levelscale);

        if (!isfinite(offset[axis]) || !isfinite(localoffset)
            || localoffset < -65535.0 || localoffset > 65535.0)
        {
            reason = "The move exceeds the background coordinate range.";
            goto fail;
        }
        delta[axis] = (int)localoffset;
    }

    if (refcount > (DWORD)-1 / sizeof(*vertices))
    {
        reason = "Too many vertices to move.";
        goto fail;
    }
    vertices = (BgDocumentVertexRef *)malloc((size_t)refcount * sizeof(*vertices));
    if (vertices == NULL) { reason = "Out of memory collecting vertices."; goto fail; }
    memcpy(vertices, refs, (size_t)refcount * sizeof(*vertices));
    vertexcount = refcount;
    for (i = 0; i < vertexcount; i++)
    {
        const BgDocumentRoom *room = BgDocumentGetRoom(document, vertices[i].room);
        if (room == NULL || room->vertices == NULL || vertices[i].index >= room->vertexcount)
        {
            reason = "A selected background vertex no longer exists.";
            goto fail;
        }
    }

    qsort(vertices, vertexcount, sizeof(*vertices), BgDocumentCompareVertexRefs);
    for (i = 0; i < vertexcount; i++)
    {
        if (unique == 0
            || BgDocumentCompareVertexRefs(&vertices[i], &vertices[unique - 1]) != 0)
        {
            vertices[unique++] = vertices[i];
        }
    }

    /* Validate every destination first, including vertices in other rooms. */
    for (i = 0; i < unique; i++)
    {
        const BgDocumentVertex *vertex =
            &document->rooms[vertices[i].room].vertices[vertices[i].index];
        int position[3] = { vertex->x, vertex->y, vertex->z };

        for (axis = 0; axis < 3; axis++)
        {
            int value = position[axis] + delta[axis];

            if (value < SHRT_MIN || value > SHRT_MAX)
            {
                reason = "A moved vertex would exceed its room's coordinate range."
                         " Use a smaller offset.";
                goto fail;
            }
        }
    }

    if (delta[0] != 0 || delta[1] != 0 || delta[2] != 0)
    {
        for (i = 0; i < unique; i++)
        {
            BgDocumentVertex *vertex =
                &document->rooms[vertices[i].room].vertices[vertices[i].index];

            vertex->x = (short)(vertex->x + delta[0]);
            vertex->y = (short)(vertex->y + delta[1]);
            vertex->z = (short)(vertex->z + delta[2]);
        }
        if (movedout != NULL) { *movedout = (DWORD)unique; }
    }
    if (appliedoffset != NULL)
    {
        for (axis = 0; axis < 3; axis++)
        {
            appliedoffset[axis] = delta[axis] / (double)document->levelscale;
        }
    }
    free(vertices);
    return TRUE;

fail:
    free(vertices);
    if (reasonout != NULL) { *reasonout = reason; }
    return FALSE;
}


void BgDocumentGetWorldPosition(const BgDocument *document,
                                const BgDocumentRoom *room,
                                const BgDocumentVertex *vertex,
                                float position[3])
{
    float inverse = 1.0f / document->levelscale;

    position[0] = (room->origin[0] + vertex->x) * inverse;
    position[1] = (room->origin[1] + vertex->y) * inverse;
    position[2] = (room->origin[2] + vertex->z) * inverse;
}


BOOL BgDocumentSetFaceTexture(BgDocument *document, const BgFaceRef *refs,
                              DWORD refcount, DWORD textureid,
                              BOOL *changedout, const char **reasonout)
{
    DWORD i;

    *changedout = FALSE;
    *reasonout = "";
    if (refs == NULL || refcount == 0 || textureid > BG_TEX_NONE)
    {
        *reasonout = "The background texture assignment is invalid.";
        return FALSE;
    }
    /* Do not partially edit a group if one reference is stale or unsupported. */
    for (i = 0; i < refcount; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);

        if (face == NULL)
        {
            *reasonout = "A background face to texture is no longer available.";
            return FALSE;
        }
    }
    for (i = 0; i < refcount; i++)
    {
        BgDocumentFace *face = (BgDocumentFace *)BgDocumentFindFace(document, &refs[i], NULL);
        BgMaterial material = face->material;

        BgMaterialSetTexture(&material, textureid);
        if (face->textureid != textureid || !BgMaterialEqual(&face->material, &material))
        {
            face->textureid = (unsigned short)textureid;
            face->material = material;
            *changedout = TRUE;
        }
    }
    if (*changedout) { document->dirty = TRUE; }
    return TRUE;
}


/* Store edits as ordinary native state groups. The existing preview, history,
 * project save and ROM compiler then all consume the same representation. */
static void BgDocumentFreeLayer(BgDocumentLayerData *layer)
{
    DWORD i;
    for (i = 0; i < layer->groupcount; i++) { free(layer->groups[i].commands); }
    free(layer->groups);
    ZeroMemory(layer, sizeof(*layer));
}

/* Registers whose writes have no side effects before the next draw. Unknown
 * commands are barriers: do not assume that matrices, lights, DMA, etc. leave
 * these registers alone or do not observe them. Track complete colour words,
 * not just the alpha subset used by the preview. */
static BOOL BgDocumentStateWrite(DWORD w0, DWORD w1, DWORD masks[6])
{
    DWORD opcode = w0 >> 24, shift = (w0 >> 8) & 255, count = w0 & 255;
    ZeroMemory(masks, 6 * sizeof(*masks));
    if (opcode == 0xBA || opcode == 0xB9)
    {
        if (!count || count > 32 || shift > 31 || count > 32 - shift) { return FALSE; }
        masks[opcode == 0xBA ? 0 : 1] = count == 32 ? 0xffffffffu : ((1u << count) - 1) << shift;
    }
    else if (opcode == 0xEF) { masks[0] = 0x00ffffffu; masks[1] = 0xffffffffu; }
    else if (opcode == 0xB6 || opcode == 0xB7) { masks[2] = w1; }
    else if (opcode == 0xFA) { masks[3] = 0xffffffffu; }
    else if (opcode == 0xFB) { masks[4] = 0xffffffffu; }
    else if (BG_SURFACE_IS_MARKER(w0, w1)) { masks[5] = 0xffffffffu; }
    else { return opcode == BG_G_PIPESYNC; }
    return TRUE;
}

static void BgDocumentCompactCommands(BgDocumentDrawGroup *group)
{
    DWORD covered[6] = {0}, masks[6], read, write = group->commandsize;
    BOOL synced = FALSE;
    /* Keep the last writer of each bit, without synthesizing new commands.
     * Partial other-mode and clear/set geometry writes share their register. */
    for (read = group->commandsize; read; )
    {
        BOOL keep = FALSE;
        DWORD w0, w1;
        read -= 8;
        w0 = BgDocumentRead32(group->commands + read);
        w1 = BgDocumentRead32(group->commands + read + 4);
        if (!BgDocumentStateWrite(w0, w1, masks))
        { ZeroMemory(covered, sizeof(covered)); keep = TRUE; }
        else if (w0 >> 24 == BG_G_PIPESYNC) { keep = TRUE; }
        else for (unsigned int i = 0; i < 6; i++)
        { keep |= (masks[i] & ~covered[i]) != 0; covered[i] |= masks[i]; }
        if (keep) { write -= 8; memmove(group->commands + write, group->commands + read, 8); }
    }
    group->commandsize -= write;
    if (group->commandsize) { memmove(group->commands, group->commands + write, group->commandsize); }
    /* Keep the first pipe sync before state changes, not the last one after
     * them. State-only commands do not start rendering. Opaque commands may. */
    for (read = write = 0; read < group->commandsize; read += 8)
    {
        DWORD w0 = BgDocumentRead32(group->commands + read);
        DWORD w1 = BgDocumentRead32(group->commands + read + 4);
        if (w0 >> 24 == BG_G_PIPESYNC)
        { if (synced) { continue; } synced = TRUE; }
        else if (!BgDocumentStateWrite(w0, w1, masks)) { synced = FALSE; }
        memmove(group->commands + write, group->commands + read, 8); write += 8;
    }
    group->commandsize = write;
}

BOOL BgDocumentCompactRoomState(BgDocumentRoom *room, BOOL *changedout,
    const char **reasonout)
{
    const char *previousreason = *reasonout;
    *changedout = FALSE;
    for (unsigned int layerindex = 0; layerindex < 2; layerindex++)
    {
        BgDocumentLayerData *source = &room->layers[layerindex], output = {0};
        DWORD count = source->groupcount, *map = NULL, before = 0, after = 0;
        unsigned char *used = NULL;
        if (!count) { continue; }
        *reasonout = "Out of memory compacting background render state.";
        if (!source->groups || count > (DWORD)-1 / sizeof(*map)) { goto fail; }
        used = calloc(count, 1); map = malloc((size_t)count * sizeof(*map));
        if (!used || !map) { goto fail; }
        for (DWORD f = 0; f < room->facecount; f++) if (room->faces[f].layer == layerindex)
        {
            if (room->faces[f].drawgroup >= count)
            { *reasonout = "A background face references missing render state."; goto fail; }
            used[room->faces[f].drawgroup] = TRUE;
        }
        output.sourcepresent = source->sourcepresent;
        for (DWORD g = 0; g < count; g++)
        {
            const BgDocumentDrawGroup *from = &source->groups[g];
            BgDocumentDrawGroup *to;
            if (!g || used[g - 1])
            { if (!BgDocumentAppendDrawGroup(&output)) { goto fail; } }
            to = &output.groups[output.groupcount - 1]; map[g] = output.groupcount - 1;
            if ((from->commandsize & 7u) || (from->commandsize && !from->commands))
            { *reasonout = "A background group contains malformed render state."; goto fail; }
            for (DWORD off = 0; off < from->commandsize; off += 8)
            {
                const unsigned char *cmd = from->commands + off;
                DWORD w0 = BgDocumentRead32(cmd), w1 = BgDocumentRead32(cmd + 4);
                /* Match the compiler: textures, combiners and generated alpha
                 * scopes are emitted from face materials, never this history. */
                if ((cmd[0] == BG_G_SETTEXTURE && !BG_SURFACE_IS_MARKER(w0, w1))
                    || cmd[0] == BG_G_TEXTURE || cmd[0] == BG_G_SETCOMBINE) { continue; }
                if (before > (DWORD)-1 - 8 || !BgDocumentAppendGroupCommand(to, cmd)) { goto fail; }
                before += 8;
            }
            if (used[g] || g + 1 == count)
            {
                BgDocumentCompactCommands(to); after += to->commandsize;
                /* Release oversized buffers too, so undo snapshots cannot
                 * retain the old multi-megabyte command history. */
                if (!to->commandsize) { free(to->commands); to->commands = NULL; }
                else
                {
                    unsigned char *small = realloc(to->commands, to->commandsize);
                    if (!small) { goto fail; }
                    to->commands = small;
                }
                to->commandcapacity = to->commandsize;
            }
        }
        for (DWORD f = 0; f < room->facecount; f++) if (room->faces[f].layer == layerindex)
        { room->faces[f].drawgroup = map[room->faces[f].drawgroup]; }
        BgDocumentFreeLayer(source); *source = output;
        *changedout |= before != after;
        free(used); free(map);
        continue;
fail:
        free(used); free(map); BgDocumentFreeLayer(&output);
        return FALSE;
    }
    *reasonout = previousreason;
    return TRUE;
}

static BOOL BgDocumentSurfaceCommand(BgDocumentDrawGroup *group, DWORD w0, DWORD w1)
{
    unsigned char command[8];
    unsigned int i;
    for (i = 0; i < 4; i++)
    { command[i] = (unsigned char)(w0 >> (24 - i * 8)); command[i + 4] = (unsigned char)(w1 >> (24 - i * 8)); }
    return BgDocumentAppendGroupCommand(group, command);
}

static BOOL BgDocumentSameSurface(const BgRenderState *a, const BgRenderState *b)
{
    return a->othermode == b->othermode && a->surfacepolicy == b->surfacepolicy
        && a->surfacebasemode == b->surfacebasemode
        && a->environmentword1 == b->environmentword1
        && !((a->othermodehigh ^ b->othermodehigh) & 0x00073000u);
}

static BOOL BgDocumentSurfaceTransition(BgDocumentDrawGroup *group,
    const BgRenderState *from, const BgRenderState *to)
{
    DWORD difference = from->othermode ^ to->othermode;
    DWORD highdifference = (from->othermodehigh ^ to->othermodehigh) & 0x00073000u;
    if (from->environmentword1 != to->environmentword1)
    {
        if (!BgDocumentSurfaceCommand(group, 0xE7000000u, 0)
            || !BgDocumentSurfaceCommand(group, 0xFB000000u, to->environmentword1)) { return FALSE; }
    }
    if (highdifference)
    {
        if (!BgDocumentSurfaceCommand(group, 0xE7000000u, 0)) { return FALSE; }
        if ((highdifference & 0x70000u)
            && !BgDocumentSurfaceCommand(group, 0xBA001003u, to->othermodehigh & 0x70000u)) { return FALSE; }
        if ((highdifference & 0x3000u)
            && !BgDocumentSurfaceCommand(group, 0xBA000C02u, to->othermodehigh & 0x3000u)) { return FALSE; }
    }
    /* Partial writes preserve the first-cycle fog blender, including runtime
       replacements made by bgApplyDynamicCCRMLUT. Sync before RDP changes. */
    if (difference)
    {
        if (!BgDocumentSurfaceCommand(group, 0xE7000000u, 0)) { return FALSE; }
        if (difference & 0xFFF8u)
        {
            if (!(difference & 0xF0D8u))
            {
                /* A decal-only edit changes Z update, coverage destination
                 * and Z mode. Keep the runtime AA choice and blender intact. */
                if ((difference & 0x20u)
                    && !BgDocumentSurfaceCommand(group, 0xB9000501u, to->othermode & 0x20u)) { return FALSE; }
                if ((difference & 0xF00u)
                    && !BgDocumentSurfaceCommand(group, 0xB9000804u, to->othermode & 0xF00u)) { return FALSE; }
            }
            else if (!BgDocumentSurfaceCommand(group, 0xB900030Du, to->othermode & 0xFFF8u)) { return FALSE; }
        }
        if ((difference & 0x000C0000u)
            && !BgDocumentSurfaceCommand(group, 0xB9001202u, to->othermode & 0x000C0000u)) { return FALSE; }
        if ((difference & 0x00030000u)
            && !BgDocumentSurfaceCommand(group, 0xB9001002u, to->othermode & 0x00030000u)) { return FALSE; }
    }
    /* texHandleType0/1 emits a white primitive color for minimum LOD. Restore
       the surrounding stream's color after an inserted detail span. */
    if ((from->othermodehigh & 0x60000u) == 0x40000u
        && (to->othermodehigh & 0x60000u) != 0x40000u
        && !BgDocumentSurfaceCommand(group, to->primitiveword0, to->primitiveword1)) { return FALSE; }
    if ((from->surfacepolicy != to->surfacepolicy || from->surfacebasemode != to->surfacebasemode)
        && !BgDocumentSurfaceCommand(group, BG_SURFACE_MARKER,
            BG_SURFACE_TAG_VALUE(to->surfacepolicy, to->surfacebasemode))) { return FALSE; }
    return TRUE;
}

static BOOL BgDocumentSurfaceLayer(BgDocumentRoom *room, unsigned int layerindex,
    const unsigned char *selected, const BgRenderState *targets)
{
    BgDocumentLayerData *source = &room->layers[layerindex], output = {0};
    DWORD *groups = malloc((size_t)room->facecount * sizeof(*groups));
    BgRenderState state;
    DWORD group, f;
    BOOL ok = FALSE;
    if (!groups) { return FALSE; }
    for (f = 0; f < room->facecount; f++)
    { if (room->faces[f].layer == layerindex && room->faces[f].drawgroup >= source->groupcount) { goto done; } }
    output.sourcepresent = source->sourcepresent;
    BgRenderStateInit(&state, layerindex == BG_GEOMETRY_SECONDARY);
    for (group = 0; group < source->groupcount; group++)
    {
        const BgDocumentDrawGroup *original = &source->groups[group];
        DWORD offset;
        BgRenderState live;
        BOOL hasfaces = FALSE;
        if (!BgDocumentAppendDrawGroup(&output)) { goto done; }
        for (offset = 0; offset < original->commandsize; offset += 8)
        {
            const unsigned char *command = original->commands + offset;
            if (!BgDocumentAppendGroupCommand(&output.groups[output.groupcount - 1], command)) { goto done; }
            BgRenderStateRead(&state, BgDocumentRead32(command), BgDocumentRead32(command + 4));
        }
        live = state;
        for (f = 0; f < room->facecount; f++)
        {
            const BgDocumentFace *face = &room->faces[f];
            BgRenderState target;
            if (face->layer != layerindex || face->drawgroup != group) { continue; }
            target = selected[f] ? targets[f] : state;
            if (!BgDocumentSameSurface(&target, &live))
            {
                if (hasfaces && !BgDocumentAppendDrawGroup(&output)) { goto done; }
                if (!BgDocumentSurfaceTransition(&output.groups[output.groupcount - 1], &live, &target)) { goto done; }
                live = target;
                hasfaces = FALSE;
            }
            groups[f] = output.groupcount - 1;
            hasfaces = TRUE;
        }
        /* Also restore at the end of a layer or before an empty state group.
           A following room/draw call must never inherit this selection's edit. */
        if (!BgDocumentSameSurface(&live, &state))
        {
            if (!BgDocumentAppendDrawGroup(&output)
                || !BgDocumentSurfaceTransition(&output.groups[output.groupcount - 1], &live, &state)) { goto done; }
        }
    }
    for (f = 0; f < room->facecount; f++)
    { if (room->faces[f].layer == layerindex) { room->faces[f].drawgroup = groups[f]; } }
    BgDocumentFreeLayer(source);
    *source = output;
    ZeroMemory(&output, sizeof(output));
    ok = TRUE;
done:
    free(groups);
    BgDocumentFreeLayer(&output);
    return ok;
}

static BOOL BgDocumentSetRenderProperties(BgDocument *document, const BgFaceRef *refs,
    DWORD count, const BgFacePropertiesEdit *edit, BOOL *changed, const char **reasonout)
{
    BgDocument copy = {0};
    size_t bytes = (size_t)count * sizeof(BgRenderState);
    BgRenderState *states = bytes / sizeof(*states) == count ? malloc(bytes) : NULL;
    DWORD i, roomindex;
    BOOL any = FALSE, ok = FALSE;
    *reasonout = "Out of memory editing background render properties.";
    if (!states) { return FALSE; }
    if (!BgDocumentGetFaceRenderStates(document, refs, count, states))
    { *reasonout = "The selected background render state could not be read."; goto done; }
    for (i = 0; i < count; i++)
    {
        DWORD mode;
        BgRenderState target = states[i];
        if (edit->fields & BG_FACE_PROPERTY_OPACITY)
        {
            target.environmentalpha = (unsigned char)edit->opacity;
            target.environmentword1 = (target.environmentword1 & 0xFFFFFF00u) | edit->opacity;
        }
        if (edit->fields & BG_FACE_PROPERTY_TRANSPARENCY)
        {
            BgTransparency surface = edit->transparency;
            if (!BgRenderSurfacePreset(&states[i], surface, &mode))
            {
                *reasonout = "Transparency changes require explicit, ordinary one-cycle or two-cycle render modes. The selection includes inherited or custom state.";
                goto done;
            }
            target.othermode = mode;
            target.surfacepolicy = surface == BG_TRANSPARENCY_AUTO ? BG_SURFACE_AUTO : (DWORD)surface + 1;
            target.surfacebasemode = target.surfacepolicy == BG_SURFACE_AUTO ? 0
                : states[i].surfacepolicy == BG_SURFACE_AUTO ? states[i].othermode & BG_SURFACE_MODE_MASK : states[i].surfacebasemode;
        }
        if (edit->fields & BG_FACE_PROPERTY_DECAL)
        {
            if (!BgRenderDecalPreset(&target, edit->decal, &mode))
            {
                *reasonout = "Decal changes require ordinary one-cycle or two-cycle render modes with depth testing enabled. The selection includes inherited or custom state.";
                goto done;
            }
            target.othermode = mode;
        }
        if (edit->fields & BG_FACE_PROPERTY_DETAIL_MASK)
        {
            const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
            BgMaterial material;
            BgDetailTexture old, detail;
            if (!BgDocumentDetailMaterial(&face->material, edit, &material, reasonout)) { goto done; }
            BgMaterialGetDetail(&face->material, &old);
            BgMaterialGetDetail(&material, &detail);
            if ((old.mode == BG_DETAIL_NONE) != (detail.mode == BG_DETAIL_NONE))
            {
                BOOL enabled = detail.mode != BG_DETAIL_NONE;
                if ((target.othermodehighknown & 0x373000u) != 0x373000u
                    || (enabled && (target.othermodehigh & 0x300000u) != 0x100000u))
                {
                    *reasonout = "Detail texture switching requires explicit texture settings and a two-cycle background material when enabling detail.";
                    goto done;
                }
                target.othermodehigh = (target.othermodehigh & ~0x73000u)
                    | (enabled ? 0x52000u : 0x12000u);
            }
        }
        any |= !BgDocumentSameSurface(&states[i], &target);
        states[i] = target;
    }
    if (!any) { ok = TRUE; goto done; }
    /* Build off to the side: allocation failures never leave a partial edit,
       including multi-room selections and combined property changes. */
    if (!BgDocumentClone(document, &copy, reasonout)) { goto done; }
    *reasonout = "Out of memory editing background render properties.";
    for (roomindex = 1; roomindex <= copy.roomcount; roomindex++)
    {
        BgDocumentRoom *room = &copy.rooms[roomindex];
        unsigned char *selected;
        BgRenderState *targets;
        BOOL layers[2] = {FALSE, FALSE};
        for (i = 0; i < count; i++) { if (refs[i].room == roomindex) { layers[refs[i].layer] = TRUE; } }
        if (!layers[0] && !layers[1]) { continue; }
        selected = calloc(room->facecount, 1);
        targets = malloc((size_t)room->facecount * sizeof(*targets));
        if (!selected || !targets) { free(selected); free(targets); goto done; }
        for (i = 0; i < count; i++)
        {
            const BgDocumentFace *face;
            DWORD f;
            if (refs[i].room != roomindex) { continue; }
            face = BgDocumentFindFace(&copy, &refs[i], NULL);
            f = (DWORD)(face - room->faces);
            selected[f] = TRUE; targets[f] = states[i];
        }
        ok = (!layers[0] || BgDocumentSurfaceLayer(room, 0, selected, targets))
            && (!layers[1] || BgDocumentSurfaceLayer(room, 1, selected, targets));
        free(selected); free(targets);
        if (!ok) { goto done; }
        ok = FALSE;
        /* Consecutive opacity edits replace state before the same faces.
         * Discard superseded writes now, rather than retaining them until
         * the project is closed and loaded again. */
        if (edit->fields & BG_FACE_PROPERTY_OPACITY)
        {
            BOOL compacted;
            if (!BgDocumentCompactRoomState(room, &compacted, reasonout)) { goto done; }
        }
    }
    BgDocumentFree(document);
    *document = copy;
    ZeroMemory(&copy, sizeof(copy));
    *changed = TRUE;
    ok = TRUE;
done:
    if (ok) { *reasonout = ""; }
    free(states);
    BgDocumentFree(&copy);
    return ok;
}

BOOL BgDocumentDetailMaterial(const BgMaterial *source, const BgFacePropertiesEdit *edit,
    BgMaterial *result, const char **reasonout)
{
    BgDetailTexture old, detail;
    unsigned int fields = edit->fields;
    *result = *source;
    BgMaterialGetDetail(source, &old);
    detail = old;
    if (BgMaterialTextureId(source) == BG_TEX_NONE || old.mode == BG_DETAIL_UNKNOWN)
    { *reasonout = "Assign a supported base texture to every selected face before editing detail textures."; return FALSE; }
    if (fields & BG_FACE_PROPERTY_DETAIL_MODE) { detail.mode = edit->detail.mode; }
    if (old.mode == BG_DETAIL_NONE && (detail.mode != BG_DETAIL_NONE || (fields & BG_FACE_PROPERTY_DETAIL_IMAGE)))
    {
        detail.textureid = BgMaterialTextureId(source);
        detail.shiftu = detail.shiftv = 15; /* 2x base UVs */
        detail.minlod = 0;
        detail.offset = (source->textureword0 >> 18) & 3u;
    }
    if (fields & BG_FACE_PROPERTY_DETAIL_IMAGE)
    { detail.mode = BG_DETAIL_SEPARATE_IMAGE; detail.textureid = edit->detail.textureid; }
    if (fields & BG_FACE_PROPERTY_DETAIL_U) { detail.shiftu = edit->detail.shiftu; }
    if (fields & BG_FACE_PROPERTY_DETAIL_V) { detail.shiftv = edit->detail.shiftv; }
    if (fields & BG_FACE_PROPERTY_DETAIL_MINLOD) { detail.minlod = edit->detail.minlod; }
    if (fields & BG_FACE_PROPERTY_DETAIL_OFFSET)
    { detail.offset = edit->detail.offset == 0 && old.offset != 2 ? old.offset : edit->detail.offset; }
    if (detail.mode == BG_DETAIL_NONE && (fields & (BG_FACE_PROPERTY_DETAIL_U
        | BG_FACE_PROPERTY_DETAIL_V | BG_FACE_PROPERTY_DETAIL_MINLOD | BG_FACE_PROPERTY_DETAIL_OFFSET)))
    { *reasonout = "Enable detail texturing on every selected face before changing its sampling settings."; return FALSE; }
    if ((old.mode == BG_DETAIL_NONE) != (detail.mode == BG_DETAIL_NONE)
        && !BgMaterialDetailCombiner(result, detail.mode != BG_DETAIL_NONE))
    { *reasonout = "This selection includes a custom texture combiner that cannot be switched automatically."; return FALSE; }
    BgMaterialSetDetail(result, &detail);
    return TRUE;
}

BOOL BgDocumentSetFaceProperties(BgDocument *document, const BgFaceRef *refs,
    DWORD count, const BgFacePropertiesEdit *edit, BOOL *changedout,
    const char **reasonout)
{
    DWORD i;
    *changedout = FALSE;
    *reasonout = "";
    if (document == NULL || refs == NULL || count == 0 || edit == NULL
        || edit->fields == 0 || (edit->fields & ~8191u)
        || ((edit->fields & BG_FACE_PROPERTY_OPACITY) && edit->opacity > 255)
        || ((edit->fields & BG_FACE_PROPERTY_DECAL) && edit->decal != FALSE && edit->decal != TRUE)
        || ((edit->fields & BG_FACE_PROPERTY_ALPHA_SOURCE) && !BG_ALPHA_IS_PRESET(edit->alphasource))
        || ((edit->fields & BG_FACE_PROPERTY_DETAIL_MODE) && (unsigned int)edit->detail.mode > BG_DETAIL_SEPARATE_IMAGE)
        || ((edit->fields & BG_FACE_PROPERTY_DETAIL_IMAGE) && edit->detail.textureid >= BG_TEX_NONE)
        || ((edit->fields & BG_FACE_PROPERTY_DETAIL_U) && edit->detail.shiftu > 15)
        || ((edit->fields & BG_FACE_PROPERTY_DETAIL_V) && edit->detail.shiftv > 15)
        || ((edit->fields & BG_FACE_PROPERTY_DETAIL_OFFSET) && edit->detail.offset > 3)
        || ((edit->fields & BG_FACE_PROPERTY_TRANSPARENCY)
            && (unsigned int)edit->transparency > BG_TRANSPARENCY_BLEND
            && edit->transparency != BG_TRANSPARENCY_AUTO)
        || ((edit->fields & BG_FACE_PROPERTY_CULL)
            && edit->cullbackfaces != FALSE && edit->cullbackfaces != TRUE)
        || ((edit->fields & BG_FACE_PROPERTY_WRAP_U)
            && (unsigned int)edit->wrapu > BG_TEXTURE_MIRROR)
        || ((edit->fields & BG_FACE_PROPERTY_WRAP_V)
            && (unsigned int)edit->wrapv > BG_TEXTURE_MIRROR))
    {
        *reasonout = "The background face property change is invalid.";
        return FALSE;
    }
    for (i = 0; i < count; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        if (face == NULL)
        {
            *reasonout = "A selected background face is no longer available.";
            return FALSE;
        }
        if ((edit->fields & (BG_FACE_PROPERTY_WRAP_U | BG_FACE_PROPERTY_WRAP_V))
            && (face->textureid == BG_TEX_NONE
                || BgMaterialTextureId(&face->material) != face->textureid))
        {
            *reasonout = "Assign a texture to every selected face before changing texture wrapping.";
            return FALSE;
        }
    }
    if ((edit->fields & BG_FACE_PROPERTY_ALPHA_SOURCE) && edit->alphasource != BG_ALPHA_AUTO)
    {
        size_t bytes = (size_t)count * sizeof(BgRenderState);
        BgRenderState *states = bytes / sizeof(*states) == count ? malloc(bytes) : NULL;
        BOOL supported;
        if (!states || !BgDocumentGetFaceRenderStates(document, refs, count, states))
        {
            free(states);
            *reasonout = "Could not read the selected faces' render state for the alpha preset.";
            return FALSE;
        }
        supported = TRUE;
        for (i = 0; supported && i < count; i++)
        { supported = BgRenderSupportsAlphaPreset(&states[i],
            &BgDocumentFindFace(document, &refs[i], NULL)->material, edit->alphasource); }
        free(states);
        if (!supported)
        {
            *reasonout = "Alpha presets require explicit one- or two-cycle render state with a standard fog/pass blender. Texture presets also require a base image and explicit LOD/detail settings in two-cycle mode.";
            return FALSE;
        }
    }
    if ((edit->fields & (BG_FACE_PROPERTY_TRANSPARENCY | BG_FACE_PROPERTY_DETAIL_MASK | BG_FACE_PROPERTY_DECAL | BG_FACE_PROPERTY_OPACITY))
        && !BgDocumentSetRenderProperties(document, refs, count, edit, changedout, reasonout))
    { return FALSE; }
    for (i = 0; i < count; i++)
    {
        BgDocumentFace *face = (BgDocumentFace *)BgDocumentFindFace(document, &refs[i], NULL);
        BgMaterial material = face->material;
        BOOL cull = (edit->fields & BG_FACE_PROPERTY_CULL)
            ? edit->cullbackfaces : face->cullbackfaces;
        /* Already validated for every face before the atomic state edit. */
        if (edit->fields & BG_FACE_PROPERTY_DETAIL_MASK)
        { BgDocumentDetailMaterial(&face->material, edit, &material, reasonout); }
        if (edit->fields & BG_FACE_PROPERTY_WRAP_U) { BgMaterialSetWrap(&material, FALSE, edit->wrapu); }
        if (edit->fields & BG_FACE_PROPERTY_WRAP_V) { BgMaterialSetWrap(&material, TRUE, edit->wrapv); }
        if (edit->fields & BG_FACE_PROPERTY_ALPHA_SOURCE) { material.alphasource = edit->alphasource; }
        if (face->cullbackfaces != cull || !BgMaterialEqual(&face->material, &material))
        {
            face->cullbackfaces = (unsigned char)cull;
            face->material = material;
            *changedout = TRUE;
        }
    }
    if (*changedout) { document->dirty = TRUE; }
    return TRUE;
}


BOOL BgDocumentPaintVertex(BgDocument *document, const BgFaceRef *ref,
                           unsigned int corner, const unsigned char rgba[4],
                           BOOL *changedout, const char **reasonout)
{
    const BgDocumentRoom *room;
    const BgDocumentFace *face;
    BgDocumentVertex *vertex;

    *changedout = FALSE;
    *reasonout = "";
    face = BgDocumentFindFace(document, ref, &room);
    if (face == NULL || corner >= 3 || rgba == NULL
        || room->vertices == NULL || face->vertexindices[corner] >= room->vertexcount)
    {
        *reasonout = "the background vertex to paint is no longer available.";
        return FALSE;
    }

    vertex = &document->rooms[ref->room].vertices[face->vertexindices[corner]];
    if (vertex->r == rgba[0] && vertex->g == rgba[1]
        && vertex->b == rgba[2] && vertex->a == rgba[3])
    {
        return TRUE;
    }
    vertex->r = rgba[0];
    vertex->g = rgba[1];
    vertex->b = rgba[2];
    vertex->a = rgba[3];
    document->dirty = TRUE;
    *changedout = TRUE;
    return TRUE;
}


static BgRenderState *BgDocumentGroupRenderStates(const BgDocumentLayerData *layer, BOOL secondary)
{
    BgRenderState *states =
        (BgRenderState *)malloc((layer->groupcount ? layer->groupcount : 1) * sizeof(*states));
    BgRenderState state;
    DWORD group;
    if (!states)
    {
        return NULL;
    }
    BgRenderStateInit(&state, secondary);
    for (group = 0; group < layer->groupcount; group++)
    {
        const BgDocumentDrawGroup *source = &layer->groups[group];
        DWORD offset;
        if ((source->commandsize & 7u) || (source->commandsize && !source->commands))
        { free(states); return NULL; }
        for (offset = 0; offset + 8 <= source->commandsize; offset += 8)
        {
            BgRenderStateRead(&state, BgDocumentRead32(source->commands + offset),
                              BgDocumentRead32(source->commands + offset + 4));
        }
        states[group] = state;
    }
    return states;
}

BOOL BgDocumentGetFaceRenderStates(const BgDocument *document, const BgFaceRef *refs,
                                   DWORD count, BgRenderState *out)
{
    const BgDocumentRoom *cachedroom = NULL;
    BgRenderState *states = NULL;
    unsigned int cachedlayer = 0;
    DWORD i;
    BOOL ok = FALSE;
    if (!document || !refs || !count || !out) { return FALSE; }
    for (i = 0; i < count; i++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], &room);
        if (!face || face->layer > 1 || face->drawgroup >= room->layers[face->layer].groupcount)
        { goto done; }
        if (room != cachedroom || face->layer != cachedlayer)
        {
            free(states);
            states = BgDocumentGroupRenderStates(&room->layers[face->layer], face->layer == BG_GEOMETRY_SECONDARY);
            if (!states) { goto done; }
            cachedroom = room;
            cachedlayer = face->layer;
        }
        out[i] = states[face->drawgroup];
    }
    ok = TRUE;
done:
    free(states);
    return ok;
}

unsigned char BgDocumentPreviewVertexAlpha(const BgDocumentRoom *room, const BgDocumentFace *face,
                                           unsigned char vertexalpha)
{
    BgRenderState state;
    DWORD group;
    if (face->layer > 1 || face->drawgroup >= room->layers[face->layer].groupcount)
    {
        return vertexalpha;
    }
    BgRenderStateInit(&state, face->layer == BG_GEOMETRY_SECONDARY);
    for (group = 0; group <= face->drawgroup; group++)
    {
        const BgDocumentDrawGroup *source = &room->layers[face->layer].groups[group];
        DWORD offset;
        for (offset = 0; offset + 8 <= source->commandsize; offset += 8)
        {
            BgRenderStateRead(&state, BgDocumentRead32(source->commands + offset),
                              BgDocumentRead32(source->commands + offset + 4));
        }
    }
    return BgRenderVertexAlpha(BgRenderGetAlpha(&state, &face->material), vertexalpha);
}

BOOL BgDocumentBuildRenderMesh(const BgDocument *document,
                               BgDocumentRenderMesh *out,
                               const char **reasonout)
{
    DWORD roomindex;
    DWORD outputface = 0;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (document == NULL || document->rooms == NULL)
    {
        *reasonout = "there is no editable bg geometry to render.";
        return FALSE;
    }

    if (document->facecount == 0)
    {
        return TRUE;
    }

    if (document->facecount > ((DWORD)-1) / (3 * sizeof(*out->vertices)))
    {
        *reasonout = "the editable bg contains too many faces.";
        return FALSE;
    }

    out->vertices = (BgVertex *)malloc((size_t)document->facecount * 3
                                      * sizeof(*out->vertices));
    out->renderflags = (BgRenderFlags *)malloc(
        (size_t)document->facecount * sizeof(*out->renderflags));
    out->tags = (unsigned short *)malloc((size_t)document->facecount
                                         * sizeof(*out->tags));
    out->facerefs = (BgFaceRef *)malloc((size_t)document->facecount
                                       * sizeof(*out->facerefs));
    out->vertexrefs = (BgDocumentVertexRef *)malloc((size_t)document->facecount * 3
        * sizeof(*out->vertexrefs));
    if (out->vertices == NULL || out->tags == NULL || out->renderflags == NULL || out->facerefs == NULL
        || out->vertexrefs == NULL)
    {
        BgDocumentRenderMeshFree(out);
        *reasonout = "out of memory building the editable bg preview.";
        return FALSE;
    }

    for (roomindex = 1; roomindex <= document->roomcount; roomindex++)
    {
        const BgDocumentRoom *room = &document->rooms[roomindex];
        BgRenderState *groupstates[2];
        DWORD faceindex;
        groupstates[0] = BgDocumentGroupRenderStates(&room->layers[0], FALSE);
        groupstates[1] = BgDocumentGroupRenderStates(&room->layers[1], TRUE);
        if (!groupstates[0] || !groupstates[1])
        {
            free(groupstates[0]); free(groupstates[1]);
            BgDocumentRenderMeshFree(out);
            *reasonout = "out of memory decoding bg render state.";
            return FALSE;
        }

        for (faceindex = 0; faceindex < room->facecount; faceindex++)
        {
            const BgDocumentFace *face = &room->faces[faceindex];
            unsigned short tag = face->textureid;
            BgRenderAlpha alpha;
            int corner;

            if (face->layer == BG_GEOMETRY_SECONDARY)
            {
                tag |= BG_TRI_SECONDARY;
            }
            if (face->cullbackfaces)
            {
                tag |= BG_TRI_CULL_BACK;
            }

            if (face->layer > 1 || face->drawgroup >= room->layers[face->layer].groupcount)
            {
                free(groupstates[0]); free(groupstates[1]);
                BgDocumentRenderMeshFree(out);
                *reasonout = "a bg face has an invalid draw group.";
                return FALSE;
            }
            const BgRenderState *renderstate = &groupstates[face->layer][face->drawgroup];
            alpha = BgRenderGetAlpha(renderstate, &face->material);
            out->renderflags[outputface] = BgRenderStateFlags(renderstate)
                | BgRenderMaterialWrap(&face->material);
            if (!alpha.texture) { out->renderflags[outputface] |= BG_RENDER_IGNORE_TEXTURE_ALPHA; }
            if (BG_ALPHA_USES_VERTEX(face->material.alphasource))
            { out->renderflags[outputface] |= BG_RENDER_NO_FOG; }
            out->tags[outputface] = tag;
            out->facerefs[outputface].faceid = face->id;
            out->facerefs[outputface].room = face->room;
            out->facerefs[outputface].layer = face->layer;
            out->facerefs[outputface].seams = face->uvseams;

            for (corner = 0; corner < 3; corner++)
            {
                const BgDocumentVertex *source =
                    &room->vertices[face->vertexindices[corner]];
                BgVertex *target = &out->vertices[outputface * 3 + corner];
                float position[3];

                BgDocumentGetWorldPosition(document, room, source, position);
                out->vertexrefs[outputface * 3 + corner].room = roomindex;
                out->vertexrefs[outputface * 3 + corner].index = face->vertexindices[corner];
                target->x = position[0];
                target->y = position[1];
                target->z = position[2];
                target->s = source->s / 32.0f;
                target->t = source->t / 32.0f;
                target->r = source->r;
                target->g = source->g;
                target->b = source->b;
                target->a = BgRenderVertexAlpha(alpha, source->a);
                BgRenderPrepareEnvironment(target, out->renderflags[outputface], &face->material);
            }

            outputface++;
        }
        free(groupstates[0]); free(groupstates[1]);
    }

    out->facecount = outputface;
    return TRUE;
}


void BgDocumentRenderMeshFree(BgDocumentRenderMesh *mesh)
{
    if (mesh == NULL)
    {
        return;
    }

    free(mesh->vertices);
    free(mesh->tags);
    free(mesh->renderflags);
    free(mesh->facerefs);
    free(mesh->vertexrefs);
    ZeroMemory(mesh, sizeof(*mesh));
}


const BgDocumentRoom *BgDocumentGetRoom(const BgDocument *document,
                                        DWORD room)
{
    if (document == NULL || document->rooms == NULL
        || room == 0 || room > document->roomcount)
    {
        return NULL;
    }

    return &document->rooms[room];
}


const BgDocumentFace *BgDocumentFindFace(const BgDocument *document,
                                         const BgFaceRef *ref,
                                         const BgDocumentRoom **roomout)
{
    const BgDocumentRoom *room;
    DWORD index;

    if (roomout != NULL)
    {
        *roomout = NULL;
    }
    if (ref == NULL || ref->faceid == BG_FACE_ID_NONE)
    {
        return NULL;
    }

    room = BgDocumentGetRoom(document, ref->room);
    if (room == NULL)
    {
        return NULL;
    }

    for (index = 0; index < room->facecount; index++)
    {
        if (room->faces[index].id == ref->faceid
            && room->faces[index].room == ref->room
            && room->faces[index].layer == ref->layer)
        {
            if (roomout != NULL)
            {
                *roomout = room;
            }
            return &room->faces[index];
        }
    }

    return NULL;
}

/* Resolve and validate every destination before mutating shared vertex records. */
static BOOL BgDocumentTransformVertices(BgDocument *document, const BgDocumentVertexRef *refs,
                                        DWORD count, const Rotation *rotation, const Scaling *scale,
                                        const double pivot[3], DWORD *changed,
                                        const char **reasonout)
{
    BgDocumentVertexRef *unique = NULL;
    short (*positions)[3] = NULL;
    DWORD i, n = 0;
    int axis;
    *changed = 0;
    *reasonout = "Invalid background transform.";
    if (!document || !document->rooms || !refs || !count || !pivot ||
        (scale ? !ScalingValid(scale) : !RotationValid(rotation)) ||
        !isfinite(document->levelscale) || document->levelscale <= 0)
    {
        return FALSE;
    }
    unique = malloc((size_t)count * sizeof(*unique));
    positions = malloc((size_t)count * sizeof(*positions));
    if (!unique || !positions)
    {
        *reasonout = "Out of memory transforming background vertices.";
        goto fail;
    }
    memcpy(unique, refs, (size_t)count * sizeof(*unique));
    qsort(unique, count, sizeof(*unique), BgDocumentCompareVertexRefs);
    for (i = 0; i < count; i++)
    {
        if (!n || BgDocumentCompareVertexRefs(&unique[i], &unique[n - 1]))
        {
            unique[n++] = unique[i];
        }
    }
    for (i = 0; i < n; i++)
    {
        const BgDocumentRoom *room = BgDocumentGetRoom(document, unique[i].room);
        const BgDocumentVertex *v;
        double point[3], rotated[3];
        if (!room || !room->vertices || unique[i].index >= room->vertexcount)
        {
            goto fail;
        }
        v = &room->vertices[unique[i].index];
        point[0] = v->x;
        point[1] = v->y;
        point[2] = v->z;
        for (axis = 0; axis < 3; axis++)
        {
            point[axis] = (point[axis] + room->origin[axis]) / document->levelscale;
        }
        if (scale)
        {
            ScalingPoint(scale, point, rotated);
        }
        else
        {
            RotationPoint(rotation, pivot, point, rotated);
        }
        for (axis = 0; axis < 3; axis++)
        {
            double value = round(rotated[axis] * document->levelscale - room->origin[axis]);
            if (!isfinite(value) || value < SHRT_MIN || value > SHRT_MAX)
            {
                *reasonout = "Transform exceeds a room's coordinate range.";
                goto fail;
            }
            positions[i][axis] = (short)value;
        }
    }
    for (i = 0; i < n; i++)
    {
        BgDocumentVertex *v = &document->rooms[unique[i].room].vertices[unique[i].index];
        if (v->x != positions[i][0] || v->y != positions[i][1] || v->z != positions[i][2])
        {
            (*changed)++;
        }
        v->x = positions[i][0];
        v->y = positions[i][1];
        v->z = positions[i][2];
    }
    document->dirty |= *changed != 0;
    free(unique);
    free(positions);
    *reasonout = "";
    return TRUE;
fail:
    free(unique);
    free(positions);
    return FALSE;
}

BOOL BgDocumentRotateVertices(BgDocument *document, const BgDocumentVertexRef *refs, DWORD count,
                              const Rotation *rotation, const double pivot[3], DWORD *changed,
                              const char **reasonout)
{
    return BgDocumentTransformVertices(document, refs, count, rotation, NULL, pivot, changed,
                                       reasonout);
}
BOOL BgDocumentScaleVertices(BgDocument *document, const BgDocumentVertexRef *refs, DWORD count,
                             const Scaling *scale, DWORD *changed, const char **reasonout)
{
    return BgDocumentTransformVertices(document, refs, count, NULL, scale,
                                       scale ? scale->pivot : NULL, changed, reasonout);
}

/* Topology edits retain only portions of an existing seam, never copy the
 * source triangle's corner mask onto unrelated new edges. */
void BgDocumentInheritFaceSeams(const BgDocumentVertex *vertices,
    const BgDocumentFace *source,BgDocumentFace *target)
{
    unsigned char mask=source->uvseams;
    target->uvseams=0;
    for (int s=0;s<3;s++) if (mask&(1<<s))
    {
        const BgDocumentVertex *a=&vertices[source->vertexindices[s]],*b=&vertices[source->vertexindices[(s+1)%3]];
        double av[3]={a->x,a->y,a->z},d[3]={b->x-a->x,b->y-a->y,b->z-a->z};
        double square=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];
        if (!square) { continue; }
        for (int c=0;c<3;c++)
        {
            BOOL on=TRUE;
            for (int end=0;end<2;end++)
            {
                const BgDocumentVertex *p=&vertices[target->vertexindices[(c+end)%3]];
                double v[3]={p->x-av[0],p->y-av[1],p->z-av[2]};
                double t=(v[0]*d[0]+v[1]*d[1]+v[2]*d[2])/square,error=0;
                for (int k=0;k<3;k++) { double e=v[k]-t*d[k]; error+=e*e; }
                /* Midpoints snap to integral native positions. */
                if (t < -1e-8 || t > 1+1e-8 || error>0.76) { on=FALSE; }
            }
            if (on) { target->uvseams|=(unsigned char)(1<<c); }
        }
    }
}
