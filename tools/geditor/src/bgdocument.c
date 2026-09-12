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


BOOL BgDocumentTranslateFaces(BgDocument *document, const BgFaceRef *refs,
                              DWORD refcount, const double offset[3],
                              double appliedoffset[3], DWORD *movedout,
                              const char **reasonout)
{
    BgDocumentVertexRef *vertices;
    DWORD i;
    BOOL ok;
    const char *ignored;
    if (reasonout == NULL) { reasonout = &ignored; }
    *reasonout = "There are no valid background faces to move.";
    if (movedout != NULL) { *movedout = 0; }
    if (refs == NULL || refcount == 0 || refcount > (DWORD)-1 / (3 * sizeof(*vertices))) { return FALSE; }
    vertices = (BgDocumentVertexRef *)malloc((size_t)refcount * 3 * sizeof(*vertices));
    if (vertices == NULL) { *reasonout = "Out of memory collecting vertices."; return FALSE; }
    for (i = 0; i < refcount; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(document, &refs[i], NULL);
        int corner;
        if (face == NULL) { free(vertices); return FALSE; }
        for (corner = 0; corner < 3; corner++)
        {
            vertices[i * 3 + corner].room = refs[i].room;
            vertices[i * 3 + corner].index = face->vertexindices[corner];
        }
    }
    ok = BgDocumentTranslateVertices(document, vertices, refcount * 3, offset,
                                     appliedoffset, movedout, reasonout);
    free(vertices);
    return ok;
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


BOOL BgDocumentSetFaceProperties(BgDocument *document, const BgFaceRef *refs,
    DWORD count, const BgFacePropertiesEdit *edit, BOOL *changedout,
    const char **reasonout)
{
    DWORD i;
    *changedout = FALSE;
    *reasonout = "";
    if (document == NULL || refs == NULL || count == 0 || edit == NULL
        || edit->fields == 0 || (edit->fields & ~7u)
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
    for (i = 0; i < count; i++)
    {
        BgDocumentFace *face = (BgDocumentFace *)BgDocumentFindFace(document, &refs[i], NULL);
        BgMaterial material = face->material;
        BOOL cull = (edit->fields & BG_FACE_PROPERTY_CULL)
            ? edit->cullbackfaces : face->cullbackfaces;
        if (edit->fields & BG_FACE_PROPERTY_WRAP_U) { BgMaterialSetWrap(&material, FALSE, edit->wrapu); }
        if (edit->fields & BG_FACE_PROPERTY_WRAP_V) { BgMaterialSetWrap(&material, TRUE, edit->wrapv); }
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
            out->tags[outputface] = tag;
            out->facerefs[outputface].faceid = face->id;
            out->facerefs[outputface].room = face->room;
            out->facerefs[outputface].layer = face->layer;
            out->facerefs[outputface].reserved = 0;

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
