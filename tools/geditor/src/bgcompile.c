/*
 * Compiler for the editable, room-aware background document.
 *
 * The header, portal table, visibility data, and other unknown data before
 * the room streams are preserved. New portal tables/polygons are appended
 * to this prefix; polygon addresses stay stable when connections are deleted.
 * Room vertex streams, Fast3D display lists and per-face
 * materials are regenerated from BgDocument.
 * Other commands captured by the parser retain the authored render state.
 */

#include <windows.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "bgdocument.h"

#define BGCOMPILE_ROOM_RECORD_SIZE 24u
#define BGCOMPILE_SEGMENT          0x0F000000u
#define BGCOMPILE_VERTEX_SEGMENT   0x0E000000u

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
    BOOL pipesynced;
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
    DWORD opcode = word0 >> 24;

    if (!BgCompileWrite32(buffer, word0)
        || !BgCompileWrite32(buffer, word1)) { return FALSE; }
    if (opcode == BG_G_PIPESYNC) { buffer->pipesynced = TRUE; }
    else if (opcode == BGCOMPILE_G_TRI1 || opcode == BGCOMPILE_G_TRI4)
    {
        buffer->pipesynced = FALSE;
    }
    return TRUE;
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


/* Material commands now belong to faces. Replaying an old C0 marker here
 * would load an image even if every face using it was made untextured. */
static BOOL BgCompileWriteGroupState(BgCompileBuffer *gdl,
                                     const unsigned char *commands, DWORD size,
                                     BOOL *cullbackfaces)
{
    DWORD offset;

    for (offset = 0; offset + 8 <= size; offset += 8)
    {
        const unsigned char *command = commands + offset;
        DWORD word0 = BgCompileRead32(command);
        DWORD word1 = BgCompileRead32(command + 4);

        if ((command[0] == BG_G_SETTEXTURE && !BG_SURFACE_IS_MARKER(word0, word1))
            || command[0] == BG_G_TEXTURE
            || command[0] == BG_G_SETCOMBINE)
        {
            continue;
        }
        if (!BgCompileWriteCommand(gdl, word0, word1)) { return FALSE; }
        if (command[0] == BGCOMPILE_G_SETGEOMETRYMODE
            && (word1 & BGCOMPILE_G_CULL_BACK))
        {
            *cullbackfaces = TRUE;
        }
        else if (command[0] == BGCOMPILE_G_CLEARGEOMETRYMODE
                 && (word1 & BGCOMPILE_G_CULL_BACK))
        {
            *cullbackfaces = FALSE;
        }
    }
    return TRUE;
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


static BOOL BgCompileEmitVertexLoad(BgCompileBuffer *gdl,
                                    BgCompileBuffer *vertexdata,
                                    const BgDocumentRoom *room,
                                    DWORD vertices[16], DWORD *count,
                                    const char **reasonout)
{
    DWORD first = vertices[0];
    DWORD span = vertices[*count - 1] - first + 1;
    DWORD offset = first * 16;
    DWORD index;

    /* The CPU's BG bullet tests use the most recent G_VTX as a contiguous
     * array, not the RSP's accumulated cache. Every triangle in a batch must
     * therefore use vertices from ONE load (including gaps in source indices).
     * Keep the original vertex array/identities whenever the span fits. */
    if (span <= 16)
    {
        *count = span;
        for (index = 0; index < span; index++)
        {
            vertices[index] = first + index;
        }
    }
    else
    {
        /* A face imported from partial cache loads can itself span more than
         * 16 source vertices. Give that batch a contiguous copy in the output;
         * leave the live document and its undo/selection identities intact. */
        offset = vertexdata->size;
        if (offset > 0x01000000u - *count * 16)
        {
            *reasonout = "the compiled bg vertex stream is too large for segmented pointers.";
            return FALSE;
        }
        for (index = 0; index < *count; index++)
        {
            if (!BgCompileWriteVertex(vertexdata, &room->vertices[vertices[index]]))
            {
                *reasonout = "out of memory compiling bg vertex batches.";
                return FALSE;
            }
        }
    }

    return BgCompileWriteCommand(gdl,
        ((DWORD)BGCOMPILE_G_VTX << 24) | ((*count - 1) << 20) | (*count * 16),
        BGCOMPILE_VERTEX_SEGMENT | offset);
}


static BOOL BgCompileEmitFaceState(BgCompileBuffer *gdl,
                                   const BgDocumentFace *face,
                                   BgMaterial *current,
                                   BOOL *cullbackfaces,
                                   const char **reasonout)
{
    const BgMaterial *material = &face->material;
    BOOL modechanged = material->modeword0 != current->modeword0
                    || material->modeword1 != current->modeword1;
    BOOL combinechanged = material->combineword0 != current->combineword0
                       || material->combineword1 != current->combineword1;
    BOOL textured = face->textureid != BG_TEX_NONE;

    if ((material->modeword0 >> 24) != BG_G_TEXTURE
        || (material->combineword0 >> 24) != BG_G_SETCOMBINE
        || face->textureid != BgMaterialTextureId(material))
    {
        *reasonout = "a bg face contains invalid material state.";
        return FALSE;
    }
    /* Reuse a preserved sync until more triangles are drawn. Otherwise each
     * save/reload would retain the old sync and insert another before it. */
    if (combinechanged && !gdl->pipesynced
        && !BgCompileWriteCommand(gdl, BG_G_PIPESYNC << 24, 0)) { return FALSE; }
    if (modechanged)
    {
        /* Establish texturing before vertex loads. When turning it back on,
         * write G_TEXTURE before C0 so texLoadFromGdl updates this command's
         * LOD, instead of reusing the preceding G_OFF command. */
        if (!BgCompileWriteCommand(gdl, material->modeword0, material->modeword1))
        {
            return FALSE;
        }
        current->modeword0 = material->modeword0;
        current->modeword1 = material->modeword1;
    }
    if (textured && (modechanged
        || material->textureword0 != current->textureword0
        || material->textureword1 != current->textureword1))
    {
        if (!BgCompileWriteCommand(gdl, material->textureword0, material->textureword1))
        {
            return FALSE;
        }
        current->textureword0 = material->textureword0;
        current->textureword1 = material->textureword1;
    }
    if (combinechanged)
    {
        if (!BgCompileWriteCommand(gdl, material->combineword0, material->combineword1))
        {
            return FALSE;
        }
        current->combineword0 = material->combineword0;
        current->combineword1 = material->combineword1;
    }
    if ((BOOL)face->cullbackfaces != *cullbackfaces)
    {
        DWORD opcode = face->cullbackfaces
            ? BGCOMPILE_G_SETGEOMETRYMODE : BGCOMPILE_G_CLEARGEOMETRYMODE;
        if (!BgCompileWriteCommand(gdl, opcode << 24, BGCOMPILE_G_CULL_BACK))
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
    return BgMaterialEqual(&a->material, &b->material)
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
                                   BgMaterial *material,
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

        if (!BgCompileEmitFaceState(gdl, firstface, material, cullbackfaces, reasonout))
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
                                    BgCompileBuffer *vertexdata,
                                    const BgDocumentRoom *room,
                                    const DWORD *faceindices,
                                    DWORD facecount,
                                    BgMaterial *material,
                                    BOOL *cullbackfaces,
                                    const char **reasonout)
{
    DWORD batchstart = 0;

    while (batchstart < facecount)
    {
        DWORD vertices[16];
        DWORD vertexcount = 0;
        DWORD batchend = batchstart;
        DWORD minvertex = (DWORD)-1;
        DWORD maxvertex = 0;

        while (batchend < facecount)
        {
            const BgDocumentFace *face = &room->faces[faceindices[batchend]];
            DWORD additions[3];
            DWORD additioncount = 0;
            DWORD nextmin = minvertex;
            DWORD nextmax = maxvertex;
            int corner;
            const BgMaterial *firstmaterial = &room->faces[faceindices[batchstart]].material;

            /* G_TEXTURE scale/enable affects vertex processing. Reload the
             * shared vertices when it changes, even if they fit in the cache. */
            if (batchend > batchstart
                && (face->material.modeword0 != firstmaterial->modeword0
                    || face->material.modeword1 != firstmaterial->modeword1))
            {
                break;
            }
            for (corner = 0; corner < 3; corner++)
            {
                DWORD vertex = face->vertexindices[corner];

                if (vertex >= room->vertexcount)
                {
                    *reasonout = "a bg face references a vertex outside its room.";
                    return FALSE;
                }
                if (vertex < nextmin) { nextmin = vertex; }
                if (vertex > nextmax) { nextmax = vertex; }
                if (BgCompileFindVertex(vertices, vertexcount, vertex) < 0
                    && BgCompileFindVertex(additions, additioncount,
                                           vertex) < 0)
                {
                    additions[additioncount++] = vertex;
                }
            }

            if (vertexcount + additioncount > 16
                || (batchend > batchstart && nextmax - nextmin >= 16))
            {
                break;
            }
            for (corner = 0; corner < (int)additioncount; corner++)
            {
                vertices[vertexcount++] = additions[corner];
            }
            minvertex = nextmin;
            maxvertex = nextmax;
            batchend++;
        }

        if (batchend == batchstart)
        {
            *reasonout = "a bg face cannot fit in the Fast3D vertex cache.";
            return FALSE;
        }

        BgCompileSortVertices(vertices, vertexcount);
        if (!BgCompileEmitFaceState(gdl, &room->faces[faceindices[batchstart]],
                                     material, cullbackfaces, reasonout)
            || !BgCompileEmitVertexLoad(gdl, vertexdata, room,
                                        vertices, &vertexcount, reasonout)
            || !BgCompileEmitTriangles(gdl, room,
                    faceindices + batchstart, batchend - batchstart,
                    vertices, vertexcount, material,
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
                           BgCompileBuffer *vertexdata,
                           const char **reasonout)
{
    const BgDocumentLayerData *layerdata = &room->layers[layer];
    BgMaterial material = {0};
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
            if (!BgCompileWriteGroupState(gdl, group->commands, group->commandsize,
                                           &cullbackfaces))
            {
                return FALSE;
            }
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

        if (!BgCompileEmitGroupFaces(gdl, vertexdata, room, faceindices, facecount,
                                     &material,
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


/* Preserve all original polygon addresses: visibility commands can name them.
 * Compact surviving connections without moving native polygons. A saved
 * source may be newer than an undo snapshot, or older than a redo snapshot. */
static BOOL BgCompilePortalRooms(const BgDocument *document, BgCompileBuffer *output,
                                 DWORD newoffsets[BG_MAX_PORTALS],
                                 const char **reasonout)
{
    DWORD table, oldcount = 0, count = document->portals.portalcount;
    DWORD originalsize = output->size, pointers[BG_MAX_PORTALS] = {0};
    if (document->portalwarning) { return TRUE; }
    if (output->size < 12 || count >= BG_MAX_PORTALS
        || (count && !document->portals.portals)) { goto mismatch; }
    table = BgCompileRead32(output->data + 8) & 0x00ffffffu;
    for (;; oldcount++)
    {
        DWORD record;
        if (table > originalsize || oldcount >= BG_MAX_PORTALS
            || oldcount + 1 > (originalsize - table) / 8) { goto mismatch; }
        record = BgCompileRead32(output->data + table + oldcount * 8);
        if (!record) { break; }
        pointers[oldcount] = record;
    }
    if (count > oldcount)
    {
        if (!BgCompileAlign(output, 4)) { return FALSE; }
        table = output->size;
        for (DWORD i = 0; i <= count; i++)
        { if (!BgCompileWrite32(output, 0) || !BgCompileWrite32(output, 0)) { return FALSE; } }
        if (!BgCompilePatch32(output, 8, BGCOMPILE_SEGMENT | table)) { goto mismatch; }
    }
    for (DWORD i = 0; i < count; i++)
    {
        const BgPortal *portal = &document->portals.portals[i];
        DWORD geometry = portal->geometryoffset, record = table + i * 8;
        DWORD slot = geometry & ~BG_PORTAL_NEW_GEOMETRY;
        if (geometry & BG_PORTAL_NEW_GEOMETRY)
        {
            if (slot >= BG_MAX_PORTALS) { goto mismatch; }
            geometry = newoffsets[slot];
        }
        /* Retain original addresses, headers and padding. Exact native points
         * are part of the document/history, so Undo works after a saved move. */
        if (portal->pointcount < 3 || portal->pointcount > BG_PORTAL_MAX_POINTS) { goto mismatch; }
        if (geometry)
        {
            if (geometry > output->size || output->size - geometry < 4u + portal->pointcount * 12u
                || output->data[geometry] != portal->pointcount) { goto mismatch; }
        }
        else
        {
            if (!(portal->geometryoffset & BG_PORTAL_NEW_GEOMETRY)) { goto mismatch; }
            if (!BgCompileAlign(output, 4)) { return FALSE; }
            geometry = output->size;
            for (int word = 0; word < 1 + portal->pointcount * 3; word++)
            { if (!BgCompileWrite32(output, 0)) { return FALSE; } }
            BgCompilePatch32(output, geometry, (DWORD)portal->pointcount << 24);
            newoffsets[slot] = geometry;
        }
        for (int point = 0; point < portal->pointcount; point++)
        {
            const BgPortalPoint *p = &portal->nativepoints[point];
            const float native[3] = {p->x, p->y, p->z};
            for (int axis = 0; axis < 3; axis++)
            {
                union { float value; DWORD word; } encoded;
                if (!isfinite(native[axis])) { goto mismatch; }
                encoded.value = native[axis];
                BgCompilePatch32(output, geometry + 4 + point * 12 + axis * 4, encoded.word);
            }
        }
        /* Existing flags/margins and shared geometry are copied unchanged. */
        DWORD pointer = BGCOMPILE_SEGMENT | geometry;
        for (DWORD old = 0; old < oldcount; old++)
        { if ((pointers[old] & 0x00ffffffu) == geometry) { pointer = pointers[old]; break; } }
        BgCompilePatch32(output, record, pointer);
        output->data[record + 4] = portal->connectedroom1;
        output->data[record + 5] = portal->connectedroom2;
        output->data[record + 6] = portal->controlbytes1;
        output->data[record + 7] = portal->controlbytes2;
    }
    /* Deleted connections must no longer be traversed by the game. */
    if (!BgCompilePatch32(output, table + count * 8, 0)) { goto mismatch; }
    return TRUE;
mismatch:
    *reasonout = "The editable portal table does not match the source BG.";
    return FALSE;
}


BOOL BgDocumentCompile(const BgDocument *document, const BgFile *source,
                       BgFile *out, const char **reasonout)
{
    BgCompileBuffer output;
    DWORD roomtable;
    DWORD prefixsize;
    DWORD roomindex;
    DWORD newoffsets[BG_MAX_PORTALS];

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(&output, sizeof(output));
    *reasonout = "";
    ZeroMemory(newoffsets, sizeof(newoffsets));
    if (source) { memcpy(newoffsets, source->newportaloffsets, sizeof(newoffsets)); }

    if (!BgCompileValidateSource(document, source, &roomtable,
                                 &prefixsize, reasonout)
        || !BgCompileAppend(&output, source->data, prefixsize)
        || !BgCompilePortalRooms(document, &output, newoffsets, reasonout))
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

        if (!BgCompileLayer(room, BG_GEOMETRY_PRIMARY, &primary, &vertices, reasonout))
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
            if (!BgCompileLayer(room, BG_GEOMETRY_SECONDARY, &secondary, &vertices,
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
    memcpy(out->newportaloffsets, newoffsets, sizeof(newoffsets));
    lstrcpyn(out->name, source->name, sizeof(out->name));
    {
        BgFile cleaned = {0};
        if (!BgFileRemoveUnusedVertices(out, &cleaned, reasonout))
        { BgFileFree(out); return FALSE; }
        if (cleaned.data) { BgFileFree(out); *out = cleaned; }
    }
    return TRUE;
}


/* Validate assets without changing them. The last-load mask also catches
 * triangles whose vertices happen to resolve correctly but lie outside the
 * last load's bounding box in bgBuildRoomVtxBounds. */
static BOOL BgCompileCheckVertexBatches(const BgFile *bg, DWORD offset,
                                        DWORD vertexcount)
{
    DWORD size;
    DWORD pc;
    DWORD lastload = 0;

    if (offset < 4 || offset > bg->size) { return FALSE; }
    size = BgCompileRead32(bg->data + offset - 4);
    if (size > bg->size - offset || (size & 7)) { return FALSE; }

    for (pc = 0; pc < size; pc += 8)
    {
        const unsigned char *cmd = bg->data + offset + pc;
        DWORD word0 = BgCompileRead32(cmd);
        DWORD word1 = BgCompileRead32(cmd + 4);
        DWORD triangle;

        if (cmd[0] == BGCOMPILE_G_ENDDL) { return TRUE; }
        if (cmd[0] == BGCOMPILE_G_VTX)
        {
            DWORD first = cmd[1] & 15;
            DWORD count = (cmd[1] >> 4) + 1;
            DWORD address = word1 & 0x00FFFFFFu;

            if (first + count > 16 || (address & 15)
                || address / 16 > vertexcount
                || count > vertexcount - address / 16) { return FALSE; }
            lastload = ((1u << count) - 1) << first;
        }
        else if (cmd[0] == BGCOMPILE_G_TRI1 || cmd[0] == BGCOMPILE_G_TRI4)
        {
            DWORD count = cmd[0] == BGCOMPILE_G_TRI1 ? 1 : 4;

            for (triangle = 0; triangle < count; triangle++)
            {
                DWORD a, b, c, mask;

                if (cmd[0] == BGCOMPILE_G_TRI1)
                {
                    a = cmd[5] / 10; b = cmd[6] / 10; c = cmd[7] / 10;
                }
                else
                {
                    a = (word1 >> (triangle * 8)) & 15;
                    b = (word1 >> (triangle * 8 + 4)) & 15;
                    c = (word0 >> (triangle * 4)) & 15;
                    if (a == 0 && b == 0 && c == 0) { continue; }
                }
                if (a >= 16 || b >= 16 || c >= 16) { return FALSE; }
                mask = (1u << a) | (1u << b) | (1u << c);
                if ((mask & lastload) != mask) { return FALSE; }
            }
        }
    }
    return TRUE;
}


BOOL BgFileValidateVertexBatches(const BgFile *bg, const char **reasonout)
{
    DWORD table;
    DWORD record;

    *reasonout = "";
    /* Static single-display-list backgrounds do not use room bullet tests. */
    if (bg->size < 8 || BgCompileRead32(bg->data) != 0) { return TRUE; }
    table = BgCompileRead32(bg->data + 4) & 0x00FFFFFFu;
    if (table > bg->size || bg->size - table < BGCOMPILE_ROOM_RECORD_SIZE)
    {
        goto invalid;
    }

    for (record = table + BGCOMPILE_ROOM_RECORD_SIZE; ; record += BGCOMPILE_ROOM_RECORD_SIZE)
    {
        DWORD vertices;
        DWORD vertexsize = 0;
        int layer;

        if (record > bg->size || bg->size - record < BGCOMPILE_ROOM_RECORD_SIZE)
        {
            goto invalid;
        }
        if (BgCompileRead32(bg->data + record + 4) == 0) { break; }
        vertices = BgCompileRead32(bg->data + record) & 0x00FFFFFFu;
        /* Streets has empty rooms with a NULL vertex pointer and state-only
         * display lists. Validate those lists with zero available vertices;
         * any actual vertex load or triangle will still be rejected. */
        if (vertices != 0)
        {
            if (vertices < 4 || vertices > bg->size) { goto invalid; }
            vertexsize = BgCompileRead32(bg->data + vertices - 4);
            if (vertexsize > bg->size - vertices || (vertexsize & 15)) { goto invalid; }
        }
        for (layer = 0; layer < 2; layer++)
        {
            DWORD offset = BgCompileRead32(bg->data + record + 4 + layer * 4)
                         & 0x00FFFFFFu;
            if (offset && !BgCompileCheckVertexBatches(bg, offset, vertexsize / 16))
            {
                goto invalid;
            }
        }
    }
    return TRUE;

invalid:
    *reasonout = "the bg contains invalid room vertex batches.";
    return FALSE;
}


/* Remove orphan vertices at the file boundary. Live document indices stay
 * stable for viewport/UV selections, in-progress edits and undo snapshots. */

#define UNUSED_VERTEX ((DWORD)-1)

static DWORD BgCleanupRead32(const unsigned char *p)
{
    return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3];
}

static void BgCleanupWrite32(unsigned char *p, DWORD value)
{
    p[0] = value >> 24; p[1] = value >> 16; p[2] = value >> 8; p[3] = value;
}

/* Validation has established that every triangle uses the most recent load.
 * Pass one marks referenced source vertices across BOTH layers. Pass two
 * rewrites cache indices and load ranges using the shared compacted map. */
static BOOL BgCleanupWalk(const BgFile *source, DWORD offset, DWORD *mapping,
                          DWORD *substitute, unsigned char *output)
{
    DWORD size, pc, first = 0, start = 0, count = 0, newfirst = 0;
    if (!offset) { return TRUE; }
    size = BgCleanupRead32(source->data + offset - 4);
    for (pc = offset; pc < offset + size; pc += 8)
    {
        const unsigned char *cmd = source->data + pc;
        DWORD word0 = BgCleanupRead32(cmd), word1 = BgCleanupRead32(cmd + 4);
        if (cmd[0] == 0xB8) { break; }
        /* A nested list or a vertex-based cull/branch is not described by the
         * flat room triangle stream. Keep such rooms intact rather than guess
         * which of their additional vertex references are significant. */
        if (cmd[0] == 0x06 || cmd[0] == 0xBE || cmd[0] == 0xB0 || cmd[0] == 0xB2)
        { return FALSE; }
        if (cmd[0] == 0x04)
        {
            first = (word1 & 0xFFFFFFu) / 16;
            start = cmd[1] & 15;
            count = (cmd[1] >> 4) + 1;
            if (output)
            {
                DWORD kept = 0;
                for (DWORD i = 0; i < count; i++) if (mapping[first + i] != UNUSED_VERTEX)
                {
                    if (!kept) { newfirst = mapping[first + i]; }
                    kept++;
                }
                /* A load with no surviving references has no visible effect. */
                BgCleanupWrite32(output + pc, kept ? 0x04000000u
                    | ((kept - 1) << 20) | (start << 16) | (kept * 16) : 0);
                BgCleanupWrite32(output + pc + 4, kept ? (word1 & 0xFF000000u) | (newfirst * 16) : 0);
            }
        }
        else if (cmd[0] == 0xBF || cmd[0] == 0xB1)
        {
            for (DWORD t = 0; t < (cmd[0] == 0xBF ? 1u : 4u); t++)
            {
                DWORD indices[3];
                if (cmd[0] == 0xBF)
                { for (DWORD c = 0; c < 3; c++) { indices[c] = cmd[5 + c] / 10; } }
                else
                {
                    indices[0] = (word1 >> (t * 8)) & 15;
                    indices[1] = (word1 >> (t * 8 + 4)) & 15;
                    indices[2] = (word0 >> (t * 4)) & 15;
                    /* Only 0,0,0 in TRI4 is padding. Preserve real degenerates. */
                    if (!(indices[0] | indices[1] | indices[2])) { continue; }
                    /* A real point triangle must not become TRI4's 0,0,0
                     * padding after compaction. Keep a leading cache slot if
                     * needed, replacing its unused position with this point
                     * so an orphan cannot distort the room bounds. */
                    if (!output && !start && indices[0] == indices[1]
                        && indices[1] == indices[2] && mapping[first] != 0)
                    {
                        mapping[first] = 1;
                        substitute[first] = first + indices[0];
                    }
                }
                for (DWORD c = 0; c < 3; c++)
                {
                    DWORD vertex = first + indices[c] - start;
                    if (!output) { mapping[vertex] = 0; substitute[vertex] = vertex; }
                    else { indices[c] = start + mapping[vertex] - newfirst; }
                }
                if (!output) { continue; }
                if (cmd[0] == 0xBF)
                { for (DWORD c = 0; c < 3; c++) { output[pc + 5 + c] = (unsigned char)(indices[c] * 10); } }
                else
                {
                    word0 = (word0 & ~(15u << (t * 4))) | (indices[2] << (t * 4));
                    word1 = (word1 & ~(255u << (t * 8)))
                        | (indices[0] << (t * 8)) | (indices[1] << (t * 8 + 4));
                }
            }
            if (output && cmd[0] == 0xB1)
            { BgCleanupWrite32(output + pc, word0); BgCleanupWrite32(output + pc + 4, word1); }
        }
    }
    return TRUE;
}

BOOL BgFileRemoveUnusedVertices(const BgFile *source, BgFile *out,
                                const char **reasonout)
{
    DWORD table;
    unsigned char *copy = NULL;
    DWORD *mapping = NULL;
    if (!out || out == source) { *reasonout = "Invalid background cleanup output."; return FALSE; }
    ZeroMemory(out, sizeof(*out));
    *reasonout = "Invalid background to clean.";
    if (!source || !source->data) { return FALSE; }
    if (!BgFileValidateVertexBatches(source, reasonout)) { return FALSE; }
    if (source->size < 8 || BgCleanupRead32(source->data)) { return TRUE; }
    table = BgCleanupRead32(source->data + 4) & 0xFFFFFFu;
    for (DWORD rec = table + 24; BgCleanupRead32(source->data + rec + 4); rec += 24)
    {
        DWORD vertices = BgCleanupRead32(source->data + rec) & 0xFFFFFFu;
        DWORD primary = BgCleanupRead32(source->data + rec + 4) & 0xFFFFFFu;
        DWORD secondary = BgCleanupRead32(source->data + rec + 8) & 0xFFFFFFu;
        DWORD total, kept = 0, *substitute;
        BOOL replaced = FALSE;
        if (!vertices) { continue; }
        total = BgCleanupRead32(source->data + vertices - 4) / 16;
        if (!total) { continue; }
        mapping = malloc((size_t)total * 2 * sizeof(*mapping));
        if (!mapping) { goto nomemory; }
        substitute = mapping + total;
        for (DWORD v = 0; v < total; v++) { mapping[v] = UNUSED_VERTEX; substitute[v] = v; }
        if (BgCleanupWalk(source, primary, mapping, substitute, NULL)
            && BgCleanupWalk(source, secondary, mapping, substitute, NULL))
        {
            for (DWORD v = 0; v < total; v++)
            {
                if (mapping[v] != UNUSED_VERTEX)
                {
                    mapping[v] = kept++;
                    replaced |= substitute[v] != v && memcmp(source->data + vertices + v * 16,
                        source->data + vertices + substitute[v] * 16, 16) != 0;
                }
            }
            /* Rooms with no faces may deliberately retain vertices solely for
             * bgRoomCalcBB/bgOrderPortal. Preserve their existing bounds. */
            if (kept && (kept < total || replaced))
            {
                if (!copy)
                {
                    copy = malloc(source->size);
                    if (!copy) { goto nomemory; }
                    memcpy(copy, source->data, source->size);
                }
                for (DWORD v = 0; v < total; v++) if (mapping[v] != UNUSED_VERTEX)
                { memcpy(copy + vertices + mapping[v] * 16, source->data + vertices + substitute[v] * 16, 16); }
                /* Leave other stream/polygon addresses unchanged. Subsequent
                 * compilation can reclaim this cleared, unreferenced space. */
                memset(copy + vertices + kept * 16, 0, (total - kept) * 16);
                BgCleanupWrite32(copy + vertices - 4, kept * 16);
                BgCleanupWalk(source, primary, mapping, substitute, copy);
                BgCleanupWalk(source, secondary, mapping, substitute, copy);
            }
        }
        free(mapping); mapping = NULL;
    }
    if (copy)
    {
        *out = *source;
        out->data = copy;
        if (!BgFileValidateVertexBatches(out, reasonout)) { BgFileFree(out); return FALSE; }
    }
    return TRUE;
nomemory:
    free(mapping); free(copy);
    *reasonout = "Out of memory removing unused background vertices.";
    return FALSE;
}
