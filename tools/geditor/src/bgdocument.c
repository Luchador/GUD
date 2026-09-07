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

#include "bgdocument.h"

#define BGDOC_ROOM_RECORD_SIZE 24u
#define BGDOC_MAX_ROOMS        256u
#define BGDOC_G_NOOP              0xC0
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
                                 DWORD textureword0, DWORD textureword1,
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
    face->textureword0 = textureword0;
    face->textureword1 = textureword1;
    face->textureid = textureword0 == 0 && textureword1 == 0
        ? BG_TEX_NONE : (unsigned short)(textureword1 & BG_TEX_ID_MASK);

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
    DWORD textureword0 = 0;
    DWORD textureword1 = 0;
    BOOL cullbackfaces = FALSE;
    DWORD drawgroup = 0;
    BOOL grouphasfaces = FALSE;

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
                                             layer, textureword0,
                                             textureword1,
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

        if (command[0] == BGDOC_G_NOOP)
        {
            textureword0 = BgDocumentRead32(command);
            textureword1 = BgDocumentRead32(command + 4);
        }
        else if (command[0] == BGDOC_G_SETGEOMETRYMODE
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
    ZeroMemory(document, sizeof(*document));
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


typedef struct BgDocumentVertexRef {
    DWORD room;
    DWORD index;
} BgDocumentVertexRef;

static int BgDocumentCompareVertexRefs(const void *left, const void *right)
{
    const BgDocumentVertexRef *a = (const BgDocumentVertexRef *)left;
    const BgDocumentVertexRef *b = (const BgDocumentVertexRef *)right;

    if (a->room != b->room) { return a->room < b->room ? -1 : 1; }
    if (a->index != b->index) { return a->index < b->index ? -1 : 1; }
    return 0;
}


BOOL BgDocumentTranslateFaces(BgDocument *document, const BgFaceRef *refs,
                              DWORD refcount, const double offset[3],
                              double appliedoffset[3], DWORD *movedout,
                              const char **reasonout)
{
    BgDocumentVertexRef *vertices = NULL;
    size_t vertexcount = 0;
    size_t unique = 0;
    size_t i;
    DWORD refindex;
    int delta[3];
    int axis;
    const char *reason = "";

    if (movedout != NULL) { *movedout = 0; }
    if (reasonout != NULL) { *reasonout = ""; }

    if (document == NULL || document->rooms == NULL
        || refs == NULL || refcount == 0 || offset == NULL)
    {
        reason = "There are no background faces to move.";
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

    if (refcount > (DWORD)-1 / (3 * sizeof(*vertices)))
    {
        reason = "Too many background faces to move.";
        goto fail;
    }
    vertices = (BgDocumentVertexRef *)malloc(
        (size_t)refcount * 3 * sizeof(*vertices));
    if (vertices == NULL)
    {
        reason = "Out of memory collecting the selected vertices.";
        goto fail;
    }

    for (refindex = 0; refindex < refcount; refindex++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face = BgDocumentFindFace(
            document, &refs[refindex], &room);
        int corner;

        if (face == NULL)
        {
            reason = "A selected background face no longer exists.";
            goto fail;
        }
        for (corner = 0; corner < 3; corner++)
        {
            if (room->vertices == NULL
                || face->vertexindices[corner] >= room->vertexcount)
            {
                reason = "A selected background face has an invalid vertex.";
                goto fail;
            }
            vertices[vertexcount].room = refs[refindex].room;
            vertices[vertexcount++].index = face->vertexindices[corner];
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
    out->tags = (unsigned short *)malloc((size_t)document->facecount
                                         * sizeof(*out->tags));
    out->facerefs = (BgFaceRef *)malloc((size_t)document->facecount
                                       * sizeof(*out->facerefs));
    if (out->vertices == NULL || out->tags == NULL || out->facerefs == NULL)
    {
        BgDocumentRenderMeshFree(out);
        *reasonout = "out of memory building the editable bg preview.";
        return FALSE;
    }

    for (roomindex = 1; roomindex <= document->roomcount; roomindex++)
    {
        const BgDocumentRoom *room = &document->rooms[roomindex];
        DWORD faceindex;

        for (faceindex = 0; faceindex < room->facecount; faceindex++)
        {
            const BgDocumentFace *face = &room->faces[faceindex];
            unsigned short tag = face->textureid;
            int corner;

            if (face->layer == BG_GEOMETRY_SECONDARY)
            {
                tag |= BG_TRI_SECONDARY;
            }
            if (face->cullbackfaces)
            {
                tag |= BG_TRI_CULL_BACK;
            }

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
                target->x = position[0];
                target->y = position[1];
                target->z = position[2];
                target->s = source->s / 32.0f;
                target->t = source->t / 32.0f;
                target->r = source->r;
                target->g = source->g;
                target->b = source->b;
                target->a = source->a;
            }

            outputface++;
        }
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
    free(mesh->facerefs);
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
