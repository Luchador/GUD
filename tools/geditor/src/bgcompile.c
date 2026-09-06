/*
 * Compiler for the editable, room-aware background document.
 *
 * The header, portal table, visibility data, and other unknown data before
 * the room streams are copied byte-for-byte. Room vertex streams and Fast3D
 * display lists are regenerated from BgDocument, while non-geometry commands
 * captured by the parser retain the authored material and render state.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "bgdocument.h"

#define BGCOMPILE_ROOM_RECORD_SIZE 24u
#define BGCOMPILE_SEGMENT          0x0F000000u
#define BGCOMPILE_VERTEX_SEGMENT   0x0E000000u

#define BGCOMPILE_G_NOOP              0xC0
#define BGCOMPILE_G_VTX               0x04
#define BGCOMPILE_G_TRI4              0xB1
#define BGCOMPILE_G_CLEARGEOMETRYMODE 0xB6
#define BGCOMPILE_G_SETGEOMETRYMODE   0xB7
#define BGCOMPILE_G_ENDDL             0xB8
#define BGCOMPILE_G_TRI1              0xBF

#define BGCOMPILE_G_CULL_BACK 0x00002000u

typedef struct BgCompileBuffer {
    unsigned char *data;
    DWORD size;
    DWORD capacity;
    BOOL failed;
} BgCompileBuffer;


static DWORD BgCompileRead32(const unsigned char *src)
{
    return ((DWORD)src[0] << 24) | ((DWORD)src[1] << 16)
         | ((DWORD)src[2] << 8) | (DWORD)src[3];
}


static BOOL BgCompileReserve(BgCompileBuffer *buffer, DWORD add)
{
    DWORD required;
    DWORD next;
    unsigned char *grown;

    if (buffer->failed || add > (DWORD)-1 - buffer->size)
    {
        buffer->failed = TRUE;
        return FALSE;
    }

    required = buffer->size + add;
    if (required <= buffer->capacity)
    {
        return TRUE;
    }

    next = buffer->capacity ? buffer->capacity : 4096;
    while (next < required)
    {
        if (next > (DWORD)-1 / 2)
        {
            next = required;
            break;
        }
        next *= 2;
    }

    grown = (unsigned char *)realloc(buffer->data, next);
    if (grown == NULL)
    {
        buffer->failed = TRUE;
        return FALSE;
    }

    buffer->data = grown;
    buffer->capacity = next;
    return TRUE;
}


static BOOL BgCompileAppend(BgCompileBuffer *buffer, const void *data,
                            DWORD size)
{
    if (!BgCompileReserve(buffer, size))
    {
        return FALSE;
    }
    if (size != 0)
    {
        memcpy(buffer->data + buffer->size, data, size);
    }
    buffer->size += size;
    return TRUE;
}


static BOOL BgCompileWrite32(BgCompileBuffer *buffer, DWORD word)
{
    unsigned char encoded[4];

    encoded[0] = (unsigned char)(word >> 24);
    encoded[1] = (unsigned char)(word >> 16);
    encoded[2] = (unsigned char)(word >> 8);
    encoded[3] = (unsigned char)word;
    return BgCompileAppend(buffer, encoded, sizeof(encoded));
}


static BOOL BgCompilePatch32(BgCompileBuffer *buffer, DWORD offset,
                             DWORD word)
{
    if (offset > buffer->size || buffer->size - offset < 4)
    {
        return FALSE;
    }

    buffer->data[offset + 0] = (unsigned char)(word >> 24);
    buffer->data[offset + 1] = (unsigned char)(word >> 16);
    buffer->data[offset + 2] = (unsigned char)(word >> 8);
    buffer->data[offset + 3] = (unsigned char)word;
    return TRUE;
}


static BOOL BgCompileAlign(BgCompileBuffer *buffer, DWORD alignment)
{
    static const unsigned char zeroes[16] = { 0 };
    DWORD padding = (alignment - (buffer->size % alignment)) % alignment;

    return BgCompileAppend(buffer, zeroes, padding);
}


static BOOL BgCompileWriteCommand(BgCompileBuffer *buffer,
                                  DWORD word0, DWORD word1)
{
    return BgCompileWrite32(buffer, word0)
        && BgCompileWrite32(buffer, word1);
}


static BOOL BgCompileWriteVertex(BgCompileBuffer *buffer,
                                 const BgDocumentVertex *vertex)
{
    unsigned char encoded[16];

    encoded[0] = (unsigned char)((unsigned short)vertex->x >> 8);
    encoded[1] = (unsigned char)vertex->x;
    encoded[2] = (unsigned char)((unsigned short)vertex->y >> 8);
    encoded[3] = (unsigned char)vertex->y;
    encoded[4] = (unsigned char)((unsigned short)vertex->z >> 8);
    encoded[5] = (unsigned char)vertex->z;
    encoded[6] = (unsigned char)(vertex->flag >> 8);
    encoded[7] = (unsigned char)vertex->flag;
    encoded[8] = (unsigned char)((unsigned short)vertex->s >> 8);
    encoded[9] = (unsigned char)vertex->s;
    encoded[10] = (unsigned char)((unsigned short)vertex->t >> 8);
    encoded[11] = (unsigned char)vertex->t;
    encoded[12] = vertex->r;
    encoded[13] = vertex->g;
    encoded[14] = vertex->b;
    encoded[15] = vertex->a;
    return BgCompileAppend(buffer, encoded, sizeof(encoded));
}


static BOOL BgCompileAppendStream(BgCompileBuffer *output,
                                  const unsigned char *stream,
                                  DWORD streamsize, DWORD *offsetout)
{
    if (!BgCompileAlign(output, 16)
        || !BgCompileWrite32(output, streamsize))
    {
        return FALSE;
    }

    *offsetout = output->size;
    return BgCompileAppend(output, stream, streamsize);
}


static void BgCompileScanState(const unsigned char *commands, DWORD size,
                               DWORD *textureword0, DWORD *textureword1,
                               BOOL *cullbackfaces)
{
    DWORD offset;

    for (offset = 0; offset + 8 <= size; offset += 8)
    {
        const unsigned char *command = commands + offset;

        if (command[0] == BGCOMPILE_G_NOOP)
        {
            *textureword0 = BgCompileRead32(command);
            *textureword1 = BgCompileRead32(command + 4);
        }
        else if (command[0] == BGCOMPILE_G_SETGEOMETRYMODE
                 && (BgCompileRead32(command + 4)
                     & BGCOMPILE_G_CULL_BACK))
        {
            *cullbackfaces = TRUE;
        }
        else if (command[0] == BGCOMPILE_G_CLEARGEOMETRYMODE
                 && (BgCompileRead32(command + 4)
                     & BGCOMPILE_G_CULL_BACK))
        {
            *cullbackfaces = FALSE;
        }
    }
}


static int BgCompileFindVertex(const DWORD *vertices, DWORD count,
                               DWORD vertex)
{
    DWORD index;

    for (index = 0; index < count; index++)
    {
        if (vertices[index] == vertex)
        {
            return (int)index;
        }
    }
    return -1;
}


static void BgCompileSortVertices(DWORD vertices[16], DWORD count)
{
    DWORD index;

    for (index = 1; index < count; index++)
    {
        DWORD value = vertices[index];
        DWORD position = index;

        while (position > 0 && vertices[position - 1] > value)
        {
            vertices[position] = vertices[position - 1];
            position--;
        }
        vertices[position] = value;
    }
}


static BOOL BgCompileEmitVertexLoads(BgCompileBuffer *gdl,
                                     const DWORD vertices[16], DWORD count)
{
    DWORD first = 0;

    while (first < count)
    {
        DWORD run = 1;
        DWORD word0;
        DWORD word1;

        while (first + run < count
               && vertices[first + run] == vertices[first] + run)
        {
            run++;
        }

        word0 = ((DWORD)BGCOMPILE_G_VTX << 24)
              | ((((run - 1) << 4) | first) << 16)
              | (run * 16);
        word1 = BGCOMPILE_VERTEX_SEGMENT | (vertices[first] * 16);
        if (!BgCompileWriteCommand(gdl, word0, word1))
        {
            return FALSE;
        }
        first += run;
    }

    return TRUE;
}


static BOOL BgCompileEmitFaceState(BgCompileBuffer *gdl,
                                   const BgDocumentFace *face,
                                   DWORD *textureword0,
                                   DWORD *textureword1,
                                   BOOL *cullbackfaces,
                                   const char **reasonout)
{
    if (face->textureword0 != *textureword0
        || face->textureword1 != *textureword1)
    {
        if (face->textureword0 == 0 && face->textureword1 == 0)
        {
            *reasonout = "a bg face requests no texture after a textured draw group.";
            return FALSE;
        }
        if ((face->textureword0 >> 24) != BGCOMPILE_G_NOOP)
        {
            *reasonout = "a bg face contains an invalid texture command.";
            return FALSE;
        }
        if (!BgCompileWriteCommand(gdl, face->textureword0,
                                   face->textureword1))
        {
            return FALSE;
        }
        *textureword0 = face->textureword0;
        *textureword1 = face->textureword1;
    }

    if ((BOOL)face->cullbackfaces != *cullbackfaces)
    {
        DWORD opcode = face->cullbackfaces
            ? BGCOMPILE_G_SETGEOMETRYMODE
            : BGCOMPILE_G_CLEARGEOMETRYMODE;

        if (!BgCompileWriteCommand(gdl, opcode << 24,
                                   BGCOMPILE_G_CULL_BACK))
        {
            return FALSE;
        }
        *cullbackfaces = face->cullbackfaces;
    }

    return TRUE;
}


static BOOL BgCompileSameFaceState(const BgDocumentFace *a,
                                   const BgDocumentFace *b)
{
    return a->textureword0 == b->textureword0
        && a->textureword1 == b->textureword1
        && a->cullbackfaces == b->cullbackfaces;
}


static BOOL BgCompileFaceIsPoint(const BgDocumentFace *face)
{
    return face->vertexindices[0] == face->vertexindices[1]
        && face->vertexindices[1] == face->vertexindices[2];
}


static BOOL BgCompileEmitTriangles(BgCompileBuffer *gdl,
                                   const BgDocumentRoom *room,
                                   const DWORD *faceindices, DWORD facecount,
                                   const DWORD vertices[16], DWORD vertexcount,
                                   DWORD *textureword0, DWORD *textureword1,
                                   BOOL *cullbackfaces,
                                   const char **reasonout)
{
    DWORD position = 0;

    while (position < facecount)
    {
        const BgDocumentFace *firstface = &room->faces[faceindices[position]];
        DWORD tricount = 1;
        DWORD cacheindices[4][3];
        DWORD triangle;
        int corner;

        if (!BgCompileEmitFaceState(gdl, firstface, textureword0,
                                    textureword1, cullbackfaces, reasonout))
        {
            return FALSE;
        }

        while (!BgCompileFaceIsPoint(firstface)
               && tricount < 4 && position + tricount < facecount
               && BgCompileSameFaceState(firstface,
                    &room->faces[faceindices[position + tricount]])
               && !BgCompileFaceIsPoint(
                    &room->faces[faceindices[position + tricount]]))
        {
            tricount++;
        }

        ZeroMemory(cacheindices, sizeof(cacheindices));
        for (triangle = 0; triangle < tricount; triangle++)
        {
            const BgDocumentFace *face =
                &room->faces[faceindices[position + triangle]];

            for (corner = 0; corner < 3; corner++)
            {
                int cache = BgCompileFindVertex(vertices, vertexcount,
                                                face->vertexindices[corner]);

                if (cache < 0)
                {
                    *reasonout = "a bg face references a vertex outside its compiled batch.";
                    return FALSE;
                }
                cacheindices[triangle][corner] = (DWORD)cache;
            }
        }

        if (tricount == 1)
        {
            DWORD word1 = (cacheindices[0][0] * 10 << 16)
                          | (cacheindices[0][1] * 10 << 8)
                          | (cacheindices[0][2] * 10);

            if (!BgCompileWriteCommand(gdl,
                    (DWORD)BGCOMPILE_G_TRI1 << 24, word1))
            {
                return FALSE;
            }
        }
        else
        {
            DWORD word0 = ((DWORD)BGCOMPILE_G_TRI4 << 24)
                | (cacheindices[3][2] << 12)
                | (cacheindices[2][2] << 8)
                | (cacheindices[1][2] << 4)
                | cacheindices[0][2];
            DWORD word1 = (cacheindices[3][1] << 28)
                | (cacheindices[3][0] << 24)
                | (cacheindices[2][1] << 20)
                | (cacheindices[2][0] << 16)
                | (cacheindices[1][1] << 12)
                | (cacheindices[1][0] << 8)
                | (cacheindices[0][1] << 4)
                | cacheindices[0][0];

            if (!BgCompileWriteCommand(gdl, word0, word1))
            {
                return FALSE;
            }
        }

        position += tricount;
    }

    return TRUE;
}


static BOOL BgCompileEmitGroupFaces(BgCompileBuffer *gdl,
                                    const BgDocumentRoom *room,
                                    const DWORD *faceindices,
                                    DWORD facecount,
                                    DWORD *textureword0,
                                    DWORD *textureword1,
                                    BOOL *cullbackfaces,
                                    const char **reasonout)
{
    DWORD batchstart = 0;

    while (batchstart < facecount)
    {
        DWORD vertices[16];
        DWORD vertexcount = 0;
        DWORD batchend = batchstart;

        while (batchend < facecount)
        {
            const BgDocumentFace *face = &room->faces[faceindices[batchend]];
            DWORD additions[3];
            DWORD additioncount = 0;
            int corner;

            for (corner = 0; corner < 3; corner++)
            {
                DWORD vertex = face->vertexindices[corner];

                if (vertex >= room->vertexcount)
                {
                    *reasonout = "a bg face references a vertex outside its room.";
                    return FALSE;
                }
                if (BgCompileFindVertex(vertices, vertexcount, vertex) < 0
                    && BgCompileFindVertex(additions, additioncount,
                                           vertex) < 0)
                {
                    additions[additioncount++] = vertex;
                }
            }

            if (vertexcount + additioncount > 16)
            {
                break;
            }
            for (corner = 0; corner < (int)additioncount; corner++)
            {
                vertices[vertexcount++] = additions[corner];
            }
            batchend++;
        }

        if (batchend == batchstart)
        {
            *reasonout = "a bg face cannot fit in the Fast3D vertex cache.";
            return FALSE;
        }

        BgCompileSortVertices(vertices, vertexcount);
        if (!BgCompileEmitVertexLoads(gdl, vertices, vertexcount)
            || !BgCompileEmitTriangles(gdl, room,
                    faceindices + batchstart, batchend - batchstart,
                    vertices, vertexcount, textureword0, textureword1,
                    cullbackfaces, reasonout))
        {
            return FALSE;
        }

        batchstart = batchend;
    }

    return TRUE;
}


static BOOL BgCompileLayer(const BgDocumentRoom *room,
                           BgGeometryLayer layer,
                           BgCompileBuffer *gdl,
                           const char **reasonout)
{
    const BgDocumentLayerData *layerdata = &room->layers[layer];
    DWORD textureword0 = 0;
    DWORD textureword1 = 0;
    BOOL cullbackfaces = FALSE;
    DWORD groupcount = layerdata->groupcount ? layerdata->groupcount : 1;
    DWORD groupindex;

    if (layerdata->groupcount != 0 && layerdata->groups == NULL)
    {
        *reasonout = "a bg layer references missing draw groups.";
        return FALSE;
    }

    for (groupindex = 0; groupindex < groupcount; groupindex++)
    {
        const BgDocumentDrawGroup *group = layerdata->groupcount
            ? &layerdata->groups[groupindex] : NULL;
        DWORD *faceindices = NULL;
        DWORD facecount = 0;
        DWORD faceindex;

        if (group != NULL)
        {
            if ((group->commandsize & 7) != 0
                || (group->commandsize != 0 && group->commands == NULL))
            {
                *reasonout = "a bg draw group contains malformed display-list state.";
                return FALSE;
            }
            if (!BgCompileAppend(gdl, group->commands, group->commandsize))
            {
                return FALSE;
            }
            BgCompileScanState(group->commands, group->commandsize,
                               &textureword0, &textureword1,
                               &cullbackfaces);
        }

        if (room->facecount != 0)
        {
            faceindices = (DWORD *)malloc((size_t)room->facecount
                                          * sizeof(*faceindices));
            if (faceindices == NULL)
            {
                *reasonout = "out of memory collecting bg faces for compilation.";
                return FALSE;
            }
        }

        for (faceindex = 0; faceindex < room->facecount; faceindex++)
        {
            const BgDocumentFace *face = &room->faces[faceindex];

            if (face->layer != (unsigned char)layer)
            {
                continue;
            }
            if (face->drawgroup >= groupcount)
            {
                free(faceindices);
                *reasonout = "a bg face references a missing draw group.";
                return FALSE;
            }
            if (face->drawgroup == groupindex)
            {
                faceindices[facecount++] = faceindex;
            }
        }

        if (!BgCompileEmitGroupFaces(gdl, room, faceindices, facecount,
                                     &textureword0, &textureword1,
                                     &cullbackfaces, reasonout))
        {
            free(faceindices);
            return FALSE;
        }
        free(faceindices);
    }

    return BgCompileWriteCommand(gdl,
                    (DWORD)BGCOMPILE_G_ENDDL << 24, 0);
}


static BOOL BgCompileHasLayerFaces(const BgDocumentRoom *room,
                                   BgGeometryLayer layer)
{
    DWORD index;

    for (index = 0; index < room->facecount; index++)
    {
        if (room->faces[index].layer == (unsigned char)layer)
        {
            return TRUE;
        }
    }
    return FALSE;
}


static BOOL BgCompileValidateSource(const BgDocument *document,
                                    const BgFile *source,
                                    DWORD *roomtableout,
                                    DWORD *prefixsizeout,
                                    const char **reasonout)
{
    DWORD roomtable;
    DWORD firststream = (DWORD)-1;
    DWORD roomindex;

    if (document == NULL || document->rooms == NULL
        || source == NULL || source->data == NULL || source->size < 0x40
        || BgCompileRead32(source->data) != 0)
    {
        *reasonout = "there is no valid room-based bg to compile.";
        return FALSE;
    }

    roomtable = BgCompileRead32(source->data + 4) & 0x00FFFFFFu;
    if (roomtable >= source->size
        || source->size - roomtable < BGCOMPILE_ROOM_RECORD_SIZE * 2
        || document->roomcount > (source->size - roomtable)
                                  / BGCOMPILE_ROOM_RECORD_SIZE - 1)
    {
        *reasonout = "the source bg room table is invalid.";
        return FALSE;
    }

    for (roomindex = 1; roomindex <= document->roomcount; roomindex++)
    {
        DWORD record = roomtable + roomindex * BGCOMPILE_ROOM_RECORD_SIZE;
        int field;

        if (BgCompileRead32(source->data + record + 4) == 0)
        {
            *reasonout = "the editable bg does not match its source room table.";
            return FALSE;
        }

        for (field = 0; field < 3; field++)
        {
            DWORD offset = BgCompileRead32(source->data + record
                                           + field * 4) & 0x00FFFFFFu;

            if (offset != 0 && offset < firststream)
            {
                firststream = offset;
            }
        }
    }

    if (BgCompileRead32(source->data + roomtable
                        + (document->roomcount + 1)
                          * BGCOMPILE_ROOM_RECORD_SIZE + 4) != 0)
    {
        *reasonout = "the editable bg room count does not match its source.";
        return FALSE;
    }

    if (firststream < 4 || firststream > source->size)
    {
        *reasonout = "the source bg contains no valid room streams.";
        return FALSE;
    }

    *roomtableout = roomtable;
    *prefixsizeout = firststream - 4;
    return TRUE;
}


BOOL BgDocumentCompile(const BgDocument *document, const BgFile *source,
                       BgFile *out, const char **reasonout)
{
    BgCompileBuffer output;
    DWORD roomtable;
    DWORD prefixsize;
    DWORD roomindex;

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(&output, sizeof(output));
    *reasonout = "";

    if (!BgCompileValidateSource(document, source, &roomtable,
                                 &prefixsize, reasonout)
        || !BgCompileAppend(&output, source->data, prefixsize))
    {
        if (output.failed && (*reasonout)[0] == '\0')
        {
            *reasonout = "out of memory compiling the bg header.";
        }
        free(output.data);
        return FALSE;
    }

    for (roomindex = 1; roomindex <= document->roomcount; roomindex++)
    {
        const BgDocumentRoom *room = &document->rooms[roomindex];
        DWORD record = roomtable + roomindex * BGCOMPILE_ROOM_RECORD_SIZE;
        BgCompileBuffer vertices;
        BgCompileBuffer primary;
        BgCompileBuffer secondary;
        DWORD vertexoffset;
        DWORD primaryoffset;
        DWORD secondaryoffset = 0;
        DWORD vertexindex;
        union { float value; DWORD word; } origin;
        int axis;

        ZeroMemory(&vertices, sizeof(vertices));
        ZeroMemory(&primary, sizeof(primary));
        ZeroMemory(&secondary, sizeof(secondary));

        if ((room->vertexcount != 0 && room->vertices == NULL)
            || (room->facecount != 0 && room->faces == NULL)
            || room->vertexcount > 0x00100000u)
        {
            *reasonout = "a bg room contains invalid geometry arrays.";
            goto room_failed;
        }

        for (vertexindex = 0; vertexindex < room->facecount; vertexindex++)
        {
            const BgDocumentFace *face = &room->faces[vertexindex];

            if (face->room != roomindex || face->layer > BG_GEOMETRY_SECONDARY
                || face->cullbackfaces > 1)
            {
                *reasonout = "a bg face contains invalid room or layer metadata.";
                goto room_failed;
            }
        }

        for (vertexindex = 0; vertexindex < room->vertexcount; vertexindex++)
        {
            if (room->vertices[vertexindex].room != roomindex
                || !BgCompileWriteVertex(&vertices,
                                         &room->vertices[vertexindex]))
            {
                *reasonout = vertices.failed
                    ? "out of memory compiling bg vertices."
                    : "a bg vertex belongs to the wrong room.";
                goto room_failed;
            }
        }

        if (!BgCompileLayer(room, BG_GEOMETRY_PRIMARY, &primary, reasonout))
        {
            if (primary.failed && (*reasonout)[0] == '\0')
            {
                *reasonout = "out of memory compiling primary bg geometry.";
            }
            goto room_failed;
        }

        if (room->layers[BG_GEOMETRY_SECONDARY].sourcepresent
            || BgCompileHasLayerFaces(room, BG_GEOMETRY_SECONDARY))
        {
            if (!BgCompileLayer(room, BG_GEOMETRY_SECONDARY, &secondary,
                                reasonout))
            {
                if (secondary.failed && (*reasonout)[0] == '\0')
                {
                    *reasonout = "out of memory compiling secondary bg geometry.";
                }
                goto room_failed;
            }
        }

        if (!BgCompileAppendStream(&output, vertices.data, vertices.size,
                                   &vertexoffset)
            || !BgCompileAppendStream(&output, primary.data, primary.size,
                                      &primaryoffset)
            || (secondary.size != 0
                && !BgCompileAppendStream(&output, secondary.data,
                                          secondary.size,
                                          &secondaryoffset)))
        {
            *reasonout = "out of memory assembling bg room streams.";
            goto room_failed;
        }

        if (vertexoffset > 0x00FFFFFFu || primaryoffset > 0x00FFFFFFu
            || secondaryoffset > 0x00FFFFFFu
            || !BgCompilePatch32(&output, record,
                                 BGCOMPILE_SEGMENT | vertexoffset)
            || !BgCompilePatch32(&output, record + 4,
                                 BGCOMPILE_SEGMENT | primaryoffset)
            || !BgCompilePatch32(&output, record + 8,
                secondaryoffset ? BGCOMPILE_SEGMENT | secondaryoffset : 0))
        {
            *reasonout = "the compiled bg is too large for segmented pointers.";
            goto room_failed;
        }

        for (axis = 0; axis < 3; axis++)
        {
            origin.value = room->origin[axis];
            if (!BgCompilePatch32(&output, record + 12 + axis * 4,
                                  origin.word))
            {
                *reasonout = "the source bg room table is incomplete.";
                goto room_failed;
            }
        }

        free(vertices.data);
        free(primary.data);
        free(secondary.data);
        continue;

room_failed:
        free(vertices.data);
        free(primary.data);
        free(secondary.data);
        free(output.data);
        return FALSE;
    }

    if (!BgCompileAlign(&output, 16))
    {
        free(output.data);
        *reasonout = "out of memory aligning the compiled bg.";
        return FALSE;
    }

    out->data = output.data;
    out->size = output.size;
    strncpy(out->name, source->name, sizeof(out->name) - 1);
    return TRUE;
}
