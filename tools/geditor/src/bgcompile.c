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
    BOOL packvertices;
    const unsigned char *pinnedvertices;
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


static void BgCompileEncodeVertex(unsigned char encoded[16],
                                   const BgDocumentVertex *vertex)
{
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
}

static BOOL BgCompileWriteVertex(BgCompileBuffer *buffer,
                                 const BgDocumentVertex *vertex)
{
    unsigned char encoded[16];
    BgCompileEncodeVertex(encoded, vertex);
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


/* Material commands and alpha scopes belong to faces. Replaying an old C0
 * marker could load an unused image or retain an obsolete alpha scope. */
static BOOL BgCompileWriteGroupState(BgCompileBuffer *gdl,
                                     const unsigned char *commands, DWORD size,
                                     BOOL *cullbackfaces, BgRenderState *renderstate,
                                     const BgMaterial *material)
{
    DWORD offset;

    for (offset = 0; offset + 8 <= size; offset += 8)
    {
        const unsigned char *command = commands + offset;
        DWORD word0 = BgCompileRead32(command);
        DWORD word1 = BgCompileRead32(command + 4);
        DWORD oldpolicy = renderstate->surfacepolicy;

        BgRenderStateRead(renderstate, word0, word1);
        if ((command[0] == BG_G_SETTEXTURE && !BG_SURFACE_IS_MARKER(word0, word1))
            || command[0] == BG_G_TEXTURE
            || command[0] == BG_G_SETCOMBINE)
        {
            continue;
        }
        if (!BgCompileWriteCommand(gdl, word0, word1)) { return FALSE; }
        /* The runtime resolves Cutout's alpha without changing the authored
         * mux. Re-emit it on both scope boundaries, even when adjacent faces
         * share a material or the restoring group has no faces. */
        if (oldpolicy != renderstate->surfacepolicy
            && (oldpolicy == BG_SURFACE_CUTOUT || renderstate->surfacepolicy == BG_SURFACE_CUTOUT)
            && (material->combineword0 >> 24) == BG_G_SETCOMBINE)
        {
            if ((!gdl->pipesynced && !BgCompileWriteCommand(gdl, BG_G_PIPESYNC << 24, 0))
                || !BgCompileWriteCommand(gdl, material->combineword0, material->combineword1))
            { return FALSE; }
        }
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

static BOOL BgCompileMaterialScope(BgCompileBuffer *gdl, BgMaterial *current, DWORD source, DWORD fog)
{
    DWORD slot;
    BOOL fogscope = fog != BG_FOG_AUTO || current->fog != BG_FOG_AUTO;
    if (current->alphasource == source && current->fog == fog) { return TRUE; }
    if (current->alphasource != source
        && !BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ALPHA_TAG | source)) { return FALSE; }
    if (current->fog != fog
        && !BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_FOG_TAG | fog)) { return FALSE; }
    for (slot = BG_ALPHA_SYNC; slot <= BG_ALPHA_LAST_SLOT; slot++)
    {
        if (!BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ALPHA_TAG | slot)) { return FALSE; }
        if (fogscope && slot == BG_ALPHA_SYNC
            && !BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_FOG_TAG | BG_FOG_CYCLE)) { return FALSE; }
    }
    if (fogscope) for (slot = BG_FOG_BLENDER; slot <= BG_FOG_LAST_SLOT; slot++)
    {
        if (!BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_FOG_TAG | slot)) { return FALSE; }
    }
    gdl->pipesynced = TRUE;
    current->alphasource = source;
    current->fog = fog;
    /* Leaving a scope must also restore the authored combiner at a list's
     * end, even if there are no more faces. Entry writes the next face's mux. */
    return source != BG_ALPHA_AUTO || fog != BG_FOG_AUTO
        || BgCompileWriteCommand(gdl, current->combineword0, current->combineword1);
}


/* Restore native geometry/texture state on every boundary. Light slots are
 * needed only when entering/leaving generated normals, which use white light. */
static BOOL BgCompileEnvironmentScope(BgCompileBuffer *gdl, BgMaterial *current, DWORD mode)
{
    DWORD slot;
    BOOL lights = BG_ENV_GENERATED(mode) || BG_ENV_GENERATED(current->environment);
    if (current->environment == mode) { return TRUE; }
    if (!BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ENV_TAG | mode)) { return FALSE; }
    for (slot = BG_ENV_CLEAR; slot <= (lights ? BG_ENV_LIGHT_AMBIENT : BG_ENV_SET); slot++)
        if (!BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ENV_TAG | slot)) { return FALSE; }
    if (!BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ENV_TAG | BG_ENV_SCALE)) { return FALSE; }
    current->environment = mode;
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
    BOOL dense = vertexdata->packvertices;

    for (index = 0; dense && index < *count; index++)
    {
        if (vertexdata->pinnedvertices && vertexdata->pinnedvertices[vertices[index]]) { dense = FALSE; }
    }

    /* The CPU's BG bullet tests use the most recent G_VTX as a contiguous
     * array, not the RSP's accumulated cache. Every triangle in a batch must
     * therefore use vertices from ONE load. Static batches can use an exact
     * dense copy; mutable vertices retain their original shared addresses. */
    if (!dense && span <= 16)
    {
        *count = span;
        for (index = 0; index < span; index++)
        {
            vertices[index] = first + index;
        }
    }
    else
    {
        /* Source index distance is not a cache limit. Remap up to sixteen
         * distinct vertices into consecutive output records without changing
         * the live document's selection, seam or undo identities. */
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
                    || material->modeword1 != current->modeword1
                    || material->environment != current->environment;
    BOOL combinechanged = material->combineword0 != current->combineword0
                       || material->combineword1 != current->combineword1
                       || material->alphasource != current->alphasource
                       || material->fog != current->fog;
    BOOL textured = face->textureid != BG_TEX_NONE;
    BOOL texturechanged = material->textureword0 != current->textureword0
                       || material->textureword1 != current->textureword1;

    if ((material->modeword0 >> 24) != BG_G_TEXTURE
        || (material->combineword0 >> 24) != BG_G_SETCOMBINE
        || face->textureid != BgMaterialTextureId(material)
        || !BG_ALPHA_IS_PRESET(material->alphasource) || material->fog > BG_FOG_OFF
        || material->environment > BG_ENV_LINEAR
        || !BgMaterialEnvironmentImageSupported(material))
    {
        *reasonout = "a bg face contains invalid material state.";
        return FALSE;
    }
    if (!BgCompileEnvironmentScope(gdl, current, material->environment)) { return FALSE; }
    if (!BgCompileMaterialScope(gdl, current, material->alphasource, material->fog)) { return FALSE; }
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
    if (textured && (modechanged || texturechanged))
    {
        if (!BgCompileWriteCommand(gdl, material->textureword0, material->textureword1))
        {
            return FALSE;
        }
        current->textureword0 = material->textureword0;
        current->textureword1 = material->textureword1;
    }
    if (BG_ENV_GENERATED(material->environment) && (modechanged || texturechanged)
        && !BgCompileWriteCommand(gdl, BG_SURFACE_MARKER, BG_ENV_TAG | BG_ENV_SCALE)) { return FALSE; }
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
        const BgDocumentFace *mapped = &room->faces[faceindices[batchstart]];
        if (BG_ENV_GENERATED(mapped->material.environment))
        {
            DWORD indices[16], offset = vertexdata->size;
            if (offset > 0x01000000u - 48u) { *reasonout = "The generated vertex stream is too large."; return FALSE; }
            if (!BgCompileEmitFaceState(gdl, mapped, material, cullbackfaces, reasonout)
                || !BgCompileWriteCommand(gdl, BG_ENV_NORMAL_MARKER,
                    BG_ENV_NORMAL_TAG | BgDocumentEnvironmentNormal(room, mapped))) { return FALSE; }
            for (DWORD c = 0; c < 3; c++)
            {
                indices[c] = mapped->vertexindices[c];
                if (!BgCompileWriteVertex(vertexdata, &room->vertices[indices[c]])) { return FALSE; }
            }
            if (!BgCompileWriteCommand(gdl, 0x04200030u, BGCOMPILE_VERTEX_SEGMENT | offset)
                || !BgCompileEmitTriangles(gdl, room, faceindices + batchstart, 1,
                    indices, 3, material, cullbackfaces, reasonout)) { return FALSE; }
            batchstart++;
            continue;
        }
        DWORD vertices[16];
        DWORD vertexcount = 0;
        DWORD batchend = batchstart;
        DWORD minvertex = (DWORD)-1;
        DWORD maxvertex = 0;
        BOOL batchdense = vertexdata->packvertices;

        while (batchend < facecount)
        {
            const BgDocumentFace *face = &room->faces[faceindices[batchend]];
            DWORD additions[3];
            DWORD additioncount = 0;
            DWORD nextmin = minvertex;
            DWORD nextmax = maxvertex;
            BOOL facedense = vertexdata->packvertices;
            int corner;
            const BgMaterial *firstmaterial = &room->faces[faceindices[batchstart]].material;

            /* Texture scale/enable and fog affect vertex processing. Reload
             * shared vertices when either changes, even if they fit. */
            if (batchend > batchstart
                && (face->material.modeword0 != firstmaterial->modeword0
                    || face->material.modeword1 != firstmaterial->modeword1
                    || face->material.alphasource != firstmaterial->alphasource
                    || face->material.fog != firstmaterial->fog
                    || face->material.environment != firstmaterial->environment))
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
                if (vertexdata->pinnedvertices && vertexdata->pinnedvertices[vertex]) { facedense = FALSE; }
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
                || (batchend > batchstart && (facedense != batchdense
                    || (!batchdense && nextmax - nextmin >= 16))))
            {
                break;
            }
            for (corner = 0; corner < (int)additioncount; corner++)
            {
                vertices[vertexcount++] = additions[corner];
            }
            minvertex = nextmin;
            maxvertex = nextmax;
            batchdense = facedense;
            batchend++;
        }

        if (batchend == batchstart)
        {
            *reasonout = "a bg face cannot fit in the Fast3D vertex cache.";
            return FALSE;
        }

        /* Dense blocks use first-reference order, independent of editor or
         * previously exported indices. This makes save/reload deterministic. */
        if (!batchdense) { BgCompileSortVertices(vertices, vertexcount); }
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

/* Only material commands can move across a draw-group boundary. Everything
 * else (matrices, lights, fog, render modes, nested lists, etc.) is a barrier. */
static BOOL BgCompileMaterialGroup(const BgDocumentDrawGroup *group)
{
    DWORD i;
    if ((group->commandsize & 7) || (group->commandsize && !group->commands)) { return FALSE; }
    for (i = 0; i < group->commandsize; i += 8)
    {
        DWORD a = BgCompileRead32(group->commands + i), b = BgCompileRead32(group->commands + i + 4);
        unsigned int op = a >> 24;
        if ((op == BG_G_SETTEXTURE && !BG_SURFACE_IS_MARKER(a, b))
            || op == BG_G_TEXTURE || op == BG_G_SETCOMBINE || op == BG_G_PIPESYNC) { continue; }
        if ((op == BGCOMPILE_G_SETGEOMETRYMODE || op == BGCOMPILE_G_CLEARGEOMETRYMODE)
            && !(b & ~BGCOMPILE_G_CULL_BACK)) { continue; }
        return FALSE;
    }
    return TRUE;
}

static BOOL BgCompileOpaqueState(const BgRenderState *state)
{
    /* Primary geometry inherits the game's opaque Z-buffered defaults.
     * Explicit cutout, blend, decal, copy/fill and generated-UV modes stay put. */
    BgRenderFlags flags = BgRenderStateFlags(state);
    return (flags & (BG_RENDER_DEPTH_TEST | BG_RENDER_DEPTH_WRITE))
                == (BG_RENDER_DEPTH_TEST | BG_RENDER_DEPTH_WRITE)
        && !(flags & (BG_RENDER_BLEND | BG_RENDER_ALPHA_TEST | BG_RENDER_DECAL | BG_RENDER_ENVIRONMENT_MASK))
        && !(state->othermode & 0xc00u) && !(state->othermodehigh & 0x200000u)
        && (state->surfacepolicy == BG_SURFACE_AUTO || state->surfacepolicy == BG_SURFACE_OPAQUE);
}

static BOOL BgCompileKnownGroup(const BgDocumentDrawGroup *group)
{
    for (DWORD i = 0; i < group->commandsize; i += 8)
    {
        switch (group->commands[i])
        {
        case 0x01: case 0x03: case 0xbc: /* matrix, lights, move-word */
        case 0xb6: case 0xb7: case 0xb9: case 0xba: case 0xef:
        case 0xfa: case 0xfb: case 0xe7: case 0xe8:
        case BG_G_TEXTURE: case BG_G_SETTEXTURE: case BG_G_SETCOMBINE:
            break;
        default: return FALSE; /* Nested/dynamic lists and raw TMEM commands. */
        }
    }
    return TRUE;
}

BOOL BgCompileDoorShadow(const BgDocumentFace *face, const BgRenderState *state,
    unsigned char **data, DWORD *size, const char **why)
{
    BgCompileBuffer gdl = {0};
    BgMaterial current;
    BOOL cull = FALSE, ok = FALSE;
    DWORD geometry = state->geometrymode;
    /* Inherited world defaults: perspective, bilinear filtering, filtered
     * conversion. Preserve every high-mode field the material authors. */
    DWORD high = (0x00082c00u & ~state->othermodehighknown) | state->othermodehigh;
    *data = NULL; *size = 0; *why = "Could not compile the Door Shadow material.";
    BgMaterialInit(&current);
    current.textureword0 = current.textureword1 = current.modeword0 = current.modeword1 = 0;
    current.combineword0 = current.combineword1 = 0;
    if (!BgCompileWriteCommand(&gdl, 0xe7000000u, 0)
        || !BgCompileWriteCommand(&gdl, 0xb6000000u, 0x001e3205u | (state->geometryknown & 0x10000u))
        || !BgCompileWriteCommand(&gdl, 0xb7000000u, geometry & ~0x2000u)
        /* Resolve inherited fog to the level's actual runtime setting before
         * loading vertices, even if the preceding BG material disabled it. */
        || !BgCompileWriteCommand(&gdl, BG_SURFACE_MARKER, BG_ALPHA_TAG | BG_ALPHA_FOG)
        /* Dithering belongs to the world pass. Do not turn inherited values
         * into zero when making the texture pipeline self-contained. */
        || !BgCompileWriteCommand(&gdl, 0xba000818u, high & 0xffffff00u)
        || !BgCompileWriteCommand(&gdl, 0xb900031du, state->othermode & 0xfffffff8u)
        || !BgCompileWriteCommand(&gdl, 0xb9000003u, state->othermode & 7u)
        || !BgCompileWriteCommand(&gdl, 0xfb000000u, state->environmentword1)
        || !BgCompileWriteCommand(&gdl, state->primitiveword0, state->primitiveword1)
        || !BgCompileWriteCommand(&gdl, BG_SURFACE_MARKER,
            BG_SURFACE_TAG_VALUE(state->surfacepolicy, state->surfacebasemode))) goto done;
    for (DWORD bit = 0; bit < 8; bit++)
        if ((state->othermodehighknown & (1u << bit))
            && !BgCompileWriteCommand(&gdl, 0xba000001u | (bit << 8), high & (1u << bit))) goto done;
    if (!BgCompileEmitFaceState(&gdl, face, &current, &cull, why)) goto done;
    for (DWORD batch = 0; batch < 2; batch++)
    {
        if (!BgCompileWriteCommand(&gdl, 0x04800090u, BGCOMPILE_VERTEX_SEGMENT | (batch * 144))) goto done;
        for (DWORD t = 0; t < 3; t++)
            if (!BgCompileWriteCommand(&gdl, 0xbf000000u,
                ((t*3*10) << 16) | ((t*3+1)*10 << 8) | ((t*3+2)*10))) goto done;
    }
    if (!BgCompileMaterialScope(&gdl, &current, BG_ALPHA_AUTO, BG_FOG_AUTO)
        || !BgCompileWriteCommand(&gdl, 0xb8000000u, 0)) goto done;
    *data = gdl.data; *size = gdl.size; gdl.data = NULL; *why = ""; ok = TRUE;
done:
    free(gdl.data); return ok;
}

static BOOL BgCompileBatchableFace(const BgDocumentFace *face)
{
    /* The runtime water bindings inject extra state outside BgMaterial.
     * Alpha scopes and diagnostic point triangles are also ordering barriers. */
    return face->material.environment == BG_ENV_AUTO
        && face->material.alphasource == BG_ALPHA_AUTO && face->material.fog == BG_FOG_AUTO
        && !BgCompileFaceIsPoint(face)
        && face->textureid != BG_TEX_NONE
        && face->textureid != 1508 && face->textureid != 1511
        && (face->textureid == BG_TEX_NONE || (face->material.textureword0 & 7) <= 4);
}

typedef struct BgCompileSortFace { DWORD index, position; BOOL last; const BgDocumentFace *face; } BgCompileSortFace;

static int BgCompileCompareFaces(const void *left, const void *right)
{
    const BgCompileSortFace *a = left, *b = right;
    if (a->last != b->last) { return a->last ? 1 : -1; }
#define COMPARE(field) if (a->face->field != b->face->field) { return a->face->field < b->face->field ? -1 : 1; }
    COMPARE(textureid)
    COMPARE(material.textureword0) COMPARE(material.textureword1)
    COMPARE(material.modeword0) COMPARE(material.modeword1)
    COMPARE(material.combineword0) COMPARE(material.combineword1)
    COMPARE(material.alphasource) COMPARE(material.fog) COMPARE(cullbackfaces)
#undef COMPARE
    return a->position < b->position ? -1 : a->position != b->position;
}

typedef struct BgCompileCost { DWORD textures, loads, vertices; } BgCompileCost;
static BgCompileCost BgCompileMeasure(const unsigned char *data, DWORD size)
{
    BgCompileCost cost = {0};
    for (DWORD i = 0; i + 8 <= size; i += 8)
    {
        DWORD a = BgCompileRead32(data + i), b = BgCompileRead32(data + i + 4);
        if (data[i] == BG_G_SETTEXTURE && !BG_EDITOR_IS_MARKER(a, b)) { cost.textures++; }
        if (data[i] == BGCOMPILE_G_VTX) { cost.loads++; cost.vertices += ((a >> 20) & 15) + 1; }
    }
    return cost;
}

/* Compile both orders before choosing. Never buy fewer texture selections
 * with larger streams, more vertex loads, or more transformed vertices. The
 * source document/vertex IDs remain untouched (selection, undo and seams). */
static BOOL BgCompileBatchFaces(BgCompileBuffer *gdl, BgCompileBuffer *vertexdata,
    const BgDocumentRoom *room, const DWORD *indices, DWORD count,
    BgMaterial *material, BOOL *cullbackfaces, const char **reasonout)
{
    BgCompileSortFace *sort = NULL;
    DWORD *order = NULL, start, i;
    BgCompileBuffer lists[2] = {{0}}, vertices[2] = {{0}};
    BgMaterial states[2] = {*material, *material};
    BOOL culls[2] = {*cullbackfaces, *cullbackfaces}, changed = FALSE, ok = FALSE;
    unsigned int choice = 0;
    sort = malloc((size_t)count * sizeof(*sort)); order = malloc((size_t)count * sizeof(*order));
    if (!sort || !order) { *reasonout = "out of memory batching background textures."; goto done; }
    for (i = 0; i < count; i++)
    { sort[i].index = indices[i]; sort[i].position = i; sort[i].last = FALSE; sort[i].face = &room->faces[indices[i]]; }
    for (start = 0; start < count; start = i)
    {
        if (!BgCompileBatchableFace(sort[start].face)) { i = start + 1; continue; }
        for (i = start + 1; i < count && BgCompileBatchableFace(sort[i].face); i++) {}
        /* End with the same binding/state. Untextured BG bullet hits and
         * light-fixture ranges can inherit the preceding texture command. */
        for (DWORD j = start; j < i; j++)
        { sort[j].last = BgCompileSameFaceState(sort[j].face, sort[i - 1].face); }
        qsort(sort + start, i - start, sizeof(*sort), BgCompileCompareFaces);
    }
    for (i = 0; i < count; i++) { order[i] = sort[i].index; changed |= order[i] != indices[i]; }
    if (!changed)
    {
        ok = BgCompileEmitGroupFaces(gdl, vertexdata, room, indices, count, material, cullbackfaces, reasonout);
        goto done;
    }
    for (i = 0; i < 2; i++)
    {
        lists[i].pipesynced = gdl->pipesynced;
        /* Only appended bytes are consumed; existing vertex offsets stay valid. */
        vertices[i].size = vertexdata->size;
        vertices[i].packvertices = vertexdata->packvertices;
        vertices[i].pinnedvertices = vertexdata->pinnedvertices;
        if (!BgCompileEmitGroupFaces(&lists[i], &vertices[i], room, i ? order : indices,
                count, &states[i], &culls[i], reasonout)) { goto done; }
    }
    {
        BgCompileCost before = BgCompileMeasure(lists[0].data, lists[0].size);
        BgCompileCost after = BgCompileMeasure(lists[1].data, lists[1].size);
        if (after.textures < before.textures && after.loads <= before.loads && after.vertices <= before.vertices
            && lists[1].size <= lists[0].size && vertices[1].size <= vertices[0].size) { choice = 1; }
    }
    if (!BgCompileAppend(gdl, lists[choice].data, lists[choice].size)) { goto done; }
    if (vertices[choice].size > vertexdata->size && !BgCompileAppend(vertexdata,
            vertices[choice].data + vertexdata->size, vertices[choice].size - vertexdata->size)) { goto done; }
    gdl->pipesynced = lists[choice].pipesynced;
    *material = states[choice]; *cullbackfaces = culls[choice]; ok = TRUE;
done:
    free(sort); free(order);
    for (i = 0; i < 2; i++) { free(lists[i].data); free(vertices[i].data); }
    return ok;
}


static BOOL BgCompileLayer(const BgDocumentRoom *room,
                           BgGeometryLayer layer,
                           BgCompileBuffer *gdl,
                           BgCompileBuffer *vertexdata,
                           const char **reasonout)
{
    const BgDocumentLayerData *layerdata = &room->layers[layer];
    BgMaterial material = {0};
    BgRenderState renderstate;
    BOOL cullbackfaces = FALSE;
    BOOL knownstate = TRUE;
    DWORD groupcount = layerdata->groupcount ? layerdata->groupcount : 1;
    DWORD groupindex;

    BgRenderStateInit(&renderstate, layer == BG_GEOMETRY_SECONDARY);
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
        DWORD groupend = groupindex + 1;
        BOOL batch;

        /* Scope only face draws, never the following group's authored state.
         * Old generated alpha packets are discarded by WriteGroupState. */
        if (!BgCompileEnvironmentScope(gdl, &material, BG_ENV_AUTO)
            || !BgCompileMaterialScope(gdl, &material, BG_ALPHA_AUTO, BG_FOG_AUTO)) { return FALSE; }
        if (group != NULL)
        {
            if ((group->commandsize & 7) != 0
                || (group->commandsize != 0 && group->commands == NULL))
            {
                *reasonout = "a bg draw group contains malformed display-list state.";
                return FALSE;
            }
            if (!BgCompileWriteGroupState(gdl, group->commands, group->commandsize,
                                           &cullbackfaces, &renderstate, &material))
            {
                return FALSE;
            }
            knownstate &= BgCompileKnownGroup(group);
        }

        batch = layer == BG_GEOMETRY_PRIMARY && knownstate && BgCompileOpaqueState(&renderstate);
        /* Secondary/decal faces may share a load across material-only groups,
         * but only the opaque primary path is allowed to reorder triangles. */
        if ((batch || (vertexdata->packvertices && knownstate)) && layerdata->groupcount)
        {
            while (groupend < groupcount && BgCompileMaterialGroup(&layerdata->groups[groupend])) { groupend++; }
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

        for (DWORD drawgroup = groupindex; drawgroup < groupend; drawgroup++)
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
            if (face->drawgroup == drawgroup)
            {
                if (!BgRenderSupportsAlphaPreset(&renderstate, &face->material, face->material.alphasource)
                    || !BgRenderSupportsFog(&renderstate, &face->material))
                {
                    *reasonout = "an alpha/fog-preset face has unsupported or inherited render/texture state.";
                    free(faceindices);
                    return FALSE;
                }
                faceindices[facecount++] = faceindex;
            }
        }

        if (!(batch && facecount > 1 ? BgCompileBatchFaces : BgCompileEmitGroupFaces)
                (gdl, vertexdata, room, faceindices, facecount,
                                     &material,
                                     &cullbackfaces, reasonout))
        {
            free(faceindices);
            return FALSE;
        }
        free(faceindices);
        groupindex = groupend - 1;
    }

    return BgCompileEnvironmentScope(gdl, &material, BG_ENV_AUTO)
        && BgCompileMaterialScope(gdl, &material, BG_ALPHA_AUTO, BG_FOG_AUTO)
        && BgCompileWriteCommand(gdl,
                    (DWORD)BGCOMPILE_G_ENDDL << 24, 0);
}

/* Export also visits levels that were never opened/edited. Replace only a
 * primary stream that fits in its original slot and uses the original vertex
 * array. This leaves secondary commands, portals and all addresses byte-exact. */
BOOL BgFileBatchOpaque(const BgFile *source, BgFile *out, const char **reasonout)
{
    BgDocument document = {0};
    unsigned char *copy = NULL;
    DWORD table;
    BOOL ok = FALSE;
    if (!out || out == source) { *reasonout = "Invalid background batching output."; return FALSE; }
    ZeroMemory(out, sizeof(*out));
    if (!BgFileValidateVertexBatches(source, reasonout)) { return FALSE; }
    if (source->size < 8 || BgCompileRead32(source->data)) { return TRUE; }
    if (!BgDocumentLoad(source->data, source->size, 1.0f, &document, reasonout)) { return FALSE; }
    table = BgCompileRead32(source->data + 4) & 0xffffffu;
    for (DWORD r = 1; r <= document.roomcount; r++)
    {
        DWORD primary = BgCompileRead32(source->data + table + r * 24 + 4) & 0xffffffu;
        DWORD vertex = BgCompileRead32(source->data + table + r * 24) & 0xffffffu;
        DWORD size = primary ? BgCompileRead32(source->data + primary - 4) : 0;
        BgCompileBuffer list = {0}, vertices = {0};
        BgCompileCost before, after;
        BOOL shared = FALSE;
        if (!size || !vertex || !document.rooms[r].facecount) { continue; }
        /* An aliased stream is another room/pass's data too; don't rewrite it. */
        for (DWORD room = 1; room <= document.roomcount; room++)
        {
            DWORD p = BgCompileRead32(source->data + table + room * 24 + 4) & 0xffffffu;
            DWORD s = BgCompileRead32(source->data + table + room * 24 + 8) & 0xffffffu;
            if ((room != r && p == primary) || s == primary) { shared = TRUE; break; }
        }
        if (shared) { continue; }
        vertices.size = document.rooms[r].vertexcount * 16;
        if (!BgCompileLayer(&document.rooms[r], BG_GEOMETRY_PRIMARY, &list, &vertices, reasonout))
        { free(list.data); free(vertices.data); goto done; }
        before = BgCompileMeasure(source->data + primary, size);
        after = BgCompileMeasure(list.data, list.size);
        if (after.textures < before.textures && after.loads <= before.loads && after.vertices <= before.vertices
            && list.size <= size && vertices.size == document.rooms[r].vertexcount * 16)
        {
            if (!copy)
            {
                copy = malloc(source->size);
                if (!copy) { free(list.data); free(vertices.data); *reasonout = "Out of memory batching background textures."; goto done; }
                memcpy(copy, source->data, source->size);
            }
            memcpy(copy + primary, list.data, list.size);
            memset(copy + primary + list.size, 0, size - list.size);
            /* The length prefix is big-endian; unrelated stream offsets stay fixed. */
            BgCompileBuffer patch = {0}; patch.data = copy; patch.size = source->size;
            BgCompilePatch32(&patch, primary - 4, list.size);
        }
        free(list.data); free(vertices.data);
    }
    if (copy) { *out = *source; out->data = copy; copy = NULL; }
    *reasonout = ""; ok = TRUE;
done:
    free(copy); BgDocumentFree(&document); return ok;
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
        /* A deleted/undone editor identity can be reused by a pasted polygon
         * with a different vertex count. Its retired allocation cannot hold
         * that shape; append a new record and update the session mapping. */
        if ((portal->geometryoffset & BG_PORTAL_NEW_GEOMETRY) && geometry
            && geometry < output->size && output->data[geometry] != portal->pointcount)
        { geometry = 0; }
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


/* Compile a private view of each room. Native vertex attributes may merge,
 * while the editable document, source IDs and history remain untouched. */
typedef struct BgCompileRoom {
    BgDocumentRoom room;
    unsigned char *pinned;
    BOOL packvertices;
} BgCompileRoom;

typedef struct BgCompileVertexKey {
    unsigned char bytes[16];
    DWORD index;
} BgCompileVertexKey;

static int BgCompileCompareVertices(const void *aa, const void *bb)
{
    const BgCompileVertexKey *a = aa, *b = bb;
    int compare = memcmp(a->bytes, b->bytes, sizeof(a->bytes));
    return compare ? compare : a->index < b->index ? -1 : a->index != b->index;
}

static BOOL BgCompileMutableTexture(DWORD image)
{
    /* Keep in sync with check_if_imageID_is_light and texLoadFromGdl's
     * dynamic water bindings. Light darkening records physical vertex IDs. */
    switch (image)
    {
    case 201: case 203: case 205: case 252: case 253: case 254:
    case 255: case 256: case 428: case 982: case 1383:
    case 1508: case 1511:
        return TRUE;
    }
    return FALSE;
}

/* These flat state commands have no untracked references into room streams.
 * Keep nested lists, matrix/light DMA and vertex-dependent commands out of
 * automatic relocation/reuse; manual compilation retains the existing path. */
static BOOL BgCompileRelocatableGroup(const BgDocumentDrawGroup *group)
{
    if ((group->commandsize & 7) || (group->commandsize && !group->commands)) { return FALSE; }
    for (DWORD i = 0; i < group->commandsize; i += 8)
    {
        switch (group->commands[i])
        {
        case 0xb6: case 0xb7: case 0xb9: case 0xba: case 0xef:
        case 0xfa: case 0xfb: case 0xe7: case 0xe8:
        case BG_G_TEXTURE: case BG_G_SETTEXTURE: case BG_G_SETCOMBINE:
            break;
        default: return FALSE;
        }
    }
    return TRUE;
}

typedef struct BgCompileStateCache {
    DWORD value[5], known[5]; /* other H/L, geometry, environment, policy */
} BgCompileStateCache;

static BOOL BgCompileKeepState(BgCompileStateCache *state, DWORD a, DWORD b)
{
    DWORD op = a >> 24, mask = 0, value = b;
    int reg = -1;
    BOOL keep;
    if (op == 0xba || op == 0xb9)
    {
        DWORD shift = (a >> 8) & 255, count = a & 255;
        if (!count || shift >= 32 || count > 32 - shift)
        { ZeroMemory(state, sizeof(*state)); return TRUE; }
        reg = op == 0xba ? 0 : 1;
        mask = count == 32 ? 0xffffffffu : ((1u << count) - 1) << shift;
    }
    else if (op == 0xb6 || op == 0xb7)
    {
        reg = 2; mask = b; value = op == 0xb6 ? 0 : b;
        /* Culling belongs to editable faces and may change between groups. */
        state->known[2] &= ~BGCOMPILE_G_CULL_BACK;
    }
    else if (op == 0xfb) { reg = 3; mask = 0xffffffffu; }
    else if (BG_SURFACE_IS_MARKER(a, b)) { reg = 4; mask = 0xffffffffu; }
    else if (op == 0xef)
    {
        /* Track both registers, but retain this combined write as a barrier. */
        state->value[0] = a & 0xffffffu; state->known[0] = 0xffffffu;
        state->value[1] = b; state->known[1] = 0xffffffffu;
    }
    else if (op == BG_G_SETTEXTURE && !BG_EDITOR_IS_MARKER(a, b))
    {
        /* Texture expansion can write the LUT mode and primitive color/LOD.
         * Primitive commands are deliberately never removed by this pass. */
        state->known[0] &= ~(3u << 14);
        if (BgCompileMutableTexture(b & 0xfffu) || (a & 7) > 4)
        { ZeroMemory(state, sizeof(*state)); }
    }
    else if (op != BG_G_TEXTURE && op != BG_G_SETCOMBINE
        && op != BG_G_PIPESYNC && op != 0xe8 && op != 0xfa
        && !BG_EDITOR_IS_MARKER(a, b))
    { ZeroMemory(state, sizeof(*state)); }
    if (reg < 0) { return TRUE; }
    keep = (state->known[reg] & mask) != mask || ((state->value[reg] ^ value) & mask) != 0;
    state->value[reg] = (state->value[reg] & ~mask) | (value & mask);
    state->known[reg] |= mask;
    return keep;
}

static void BgCompileFreeRoom(BgCompileRoom *prepared)
{
    free(prepared->room.faces);
    free(prepared->pinned);
    for (int layer = 0; layer < 2; layer++)
    {
        BgDocumentLayerData *data = &prepared->room.layers[layer];
        for (DWORD g = 0; data->groups && g < data->groupcount; g++) { free(data->groups[g].commands); }
        free(data->groups);
    }
    ZeroMemory(prepared, sizeof(*prepared));
}

static BOOL BgCompilePrepareRoom(const BgDocumentRoom *source,
    BgCompileRoom *prepared, const char **reasonout)
{
    BgCompileVertexKey *keys = NULL;
    DWORD *map = NULL, keycount = 0;
    unsigned char *used = NULL;
    *reasonout = "out of memory preparing background batches.";
    prepared->room = *source;
    prepared->room.faces = NULL;
    for (int layer = 0; layer < 2; layer++) { prepared->room.layers[layer].groups = NULL; }
    prepared->packvertices = TRUE;
    if (source->facecount)
    {
        prepared->room.faces = malloc((size_t)source->facecount * sizeof(*source->faces));
        if (!prepared->room.faces) { goto fail; }
        memcpy(prepared->room.faces, source->faces, (size_t)source->facecount * sizeof(*source->faces));
    }
    for (int layer = 0; layer < 2; layer++)
    {
        BgCompileStateCache state = {0};
        const BgDocumentLayerData *from = &source->layers[layer];
        BgDocumentLayerData *to = &prepared->room.layers[layer];
        if (!from->groupcount) { continue; }
        if (!from->groups) { *reasonout = "a bg layer references missing draw groups."; goto fail; }
        to->groups = calloc(from->groupcount, sizeof(*to->groups));
        if (!to->groups) { goto fail; }
        for (DWORD g = 0; g < from->groupcount; g++)
        {
            const BgDocumentDrawGroup *old = &from->groups[g];
            BgDocumentDrawGroup *group = &to->groups[g];
            if ((old->commandsize & 7) || (old->commandsize && !old->commands))
            { *reasonout = "a bg draw group contains malformed display-list state."; goto fail; }
            prepared->packvertices &= BgCompileRelocatableGroup(old);
            if (old->commandsize)
            {
                group->commands = malloc(old->commandsize);
                if (!group->commands) { goto fail; }
                group->commandcapacity = old->commandsize;
            }
            for (DWORD pc = 0; pc < old->commandsize; pc += 8)
            {
                const unsigned char *command = old->commands + pc;
                if (BgCompileKeepState(&state, BgCompileRead32(command), BgCompileRead32(command + 4)))
                { memcpy(group->commands + group->commandsize, command, 8); group->commandsize += 8; }
            }
            /* Face edits, rather than the old captured commands, determine
             * the actual texture bindings written at this group boundary. */
            for (DWORD f = 0; f < source->facecount; f++)
            {
                const BgDocumentFace *face = &source->faces[f];
                if (face->layer != layer || face->drawgroup != g) { continue; }
                state.known[2] &= ~BGCOMPILE_G_CULL_BACK;
                if (face->textureid != BG_TEX_NONE) { state.known[0] &= ~(3u << 14); }
                if (BgCompileMutableTexture(face->textureid)
                    || (face->textureid != BG_TEX_NONE && (face->material.textureword0 & 7) > 4))
                { ZeroMemory(&state, sizeof(state)); }
            }
        }
    }
    if (!source->vertexcount) { *reasonout = ""; return TRUE; }
    used = calloc(source->vertexcount, 1);
    prepared->pinned = calloc(source->vertexcount, 1);
    map = malloc((size_t)source->vertexcount * sizeof(*map));
    keys = malloc((size_t)source->vertexcount * sizeof(*keys));
    if (!used || !prepared->pinned || !map || !keys) { goto fail; }
    for (DWORD f = 0; f < source->facecount; f++)
    {
        const BgDocumentFace *face = &source->faces[f];
        /* An untextured face can inherit a light texture for CPU bullet hits.
         * Preserve those addresses too, even if their bytes match a wall. */
        BOOL pinned = !prepared->packvertices || BgCompileMutableTexture(face->textureid)
            || face->textureid == BG_TEX_NONE || (face->material.textureword0 & 7) > 4;
        for (int c = 0; c < 3; c++)
        {
            DWORD v = face->vertexindices[c];
            if (v >= source->vertexcount) { *reasonout = "a bg face references a vertex outside its room."; goto fail; }
            used[v] = TRUE; prepared->pinned[v] |= pinned;
        }
    }
    for (DWORD v = 0; v < source->vertexcount; v++)
    {
        map[v] = v;
        if (!used[v] || prepared->pinned[v]) { continue; }
        BgCompileEncodeVertex(keys[keycount].bytes, &source->vertices[v]);
        keys[keycount++].index = v;
    }
    qsort(keys, keycount, sizeof(*keys), BgCompileCompareVertices);
    for (DWORD k = 1; k < keycount; k++)
    {
        if (!memcmp(keys[k - 1].bytes, keys[k].bytes, sizeof(keys[k].bytes)))
        { map[keys[k].index] = map[keys[k - 1].index]; }
    }
    for (DWORD f = 0; f < source->facecount; f++)
    {
        BgDocumentFace *face = &prepared->room.faces[f];
        /* Keep the whole source triangle contiguous if any corner has a
         * mutable alias, including its otherwise ordinary neighbor corners. */
        if (prepared->pinned[face->vertexindices[0]] || prepared->pinned[face->vertexindices[1]]
            || prepared->pinned[face->vertexindices[2]]) { continue; }
        for (int c = 0; c < 3; c++) { face->vertexindices[c] = map[face->vertexindices[c]]; }
    }
    free(keys); free(map); free(used); *reasonout = "";
    return TRUE;
fail:
    free(keys); free(map); free(used); BgCompileFreeRoom(prepared);
    return FALSE;
}

/* Keep command storage reusable within a live editing session. The file saver
 * compacts this private allocation on disk without relocating history IDs. */
static BOOL BgCompileVisCommands(const BgDocument *document, const BgFile *source,
    BgCompileBuffer *output, const DWORD portals[BG_MAX_PORTALS],
    DWORD *offset, DWORD *capacity, const char **why)
{
    *offset = source->visoffset; *capacity = source->viscapacity;
    if (!document->viscommandsloaded) { return TRUE; }
    DWORD size = document->viscommandssize;
    if (size && (!document->viscommands || (size & 7) || size > 65536u * 8
        || document->viscommands[size - 8]))
    { *why = "The BG command stream is incomplete."; return FALSE; }
    if (!*capacity)
    {
        *offset = BgCompileRead32(source->data + 12) & 0xffffffu;
        if (*offset)
        {
            while (*offset <= output->size && *capacity <= output->size - *offset
                && output->size - *offset - *capacity >= 8)
            {
                *capacity += 8;
                if (!output->data[*offset + *capacity - 8]) { break; }
            }
        }
    }
    if (*offset > output->size || *capacity > output->size - *offset)
    { *why = "The source BG command allocation is invalid."; return FALSE; }
    if (size > *capacity)
    {
        DWORD next = 64; while (next < size) { next *= 2; }
        if (!BgCompileAlign(output, 4)) { return FALSE; }
        *offset = output->size; *capacity = next;
        for (DWORD n = 0; n < next; n += 4)
        { if (!BgCompileWrite32(output, 0)) { return FALSE; } }
    }
    if (size)
    {
        if (*offset > 0xffffffu) { *why = "BG commands exceed segmented address space."; return FALSE; }
        memcpy(output->data + *offset, document->viscommands, size);
        for (DWORD at = 0; at < size; at += 8)
        {
            if (output->data[*offset + at] != 0x64) { continue; }
            DWORD arg = BgCompileRead32(output->data + *offset + at + 4);
            if (arg & BG_PORTAL_NEW_GEOMETRY)
            {
                DWORD slot = arg & ~BG_PORTAL_NEW_GEOMETRY;
                if (slot >= BG_MAX_PORTALS || !portals[slot])
                { *why = "A BG command refers to an unsaved portal that was removed. Restore the portal or remove that command."; return FALSE; }
                BgCompilePatch32(output, *offset + at + 4, BGCOMPILE_SEGMENT | portals[slot]);
            }
        }
    }
    return BgCompilePatch32(output, 12, size ? BGCOMPILE_SEGMENT | *offset : 0);
}

BOOL BgDocumentCompile(const BgDocument *document, const BgFile *source,
                       BgFile *out, const char **reasonout)
{
    BgCompileBuffer output;
    DWORD roomtable;
    DWORD prefixsize;
    DWORD roomindex;
    DWORD newoffsets[BG_MAX_PORTALS];
    DWORD visoffset, viscapacity;

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(&output, sizeof(output));
    *reasonout = "";
    ZeroMemory(newoffsets, sizeof(newoffsets));
    if (source) { memcpy(newoffsets, source->newportaloffsets, sizeof(newoffsets)); }

    if (!BgCompileValidateSource(document, source, &roomtable,
                                 &prefixsize, reasonout)
        || !BgCompileAppend(&output, source->data, prefixsize)
        || !BgCompilePortalRooms(document, &output, newoffsets, reasonout)
        || !BgCompileVisCommands(document, source, &output, newoffsets,
            &visoffset, &viscapacity, reasonout))
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
        BgCompileRoom prepared = {0};
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

        if (!BgCompilePrepareRoom(room, &prepared, reasonout)) { goto room_failed; }
        room = &prepared.room;
        vertices.packvertices = prepared.packvertices;
        vertices.pinnedvertices = prepared.pinned;

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
        BgCompileFreeRoom(&prepared);
        continue;

room_failed:
        free(vertices.data);
        free(primary.data);
        free(secondary.data);
        BgCompileFreeRoom(&prepared);
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
    out->visoffset = visoffset;
    out->viscapacity = viscapacity;
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


static BgCompileCost BgCompileFileCost(const BgFile *file)
{
    BgCompileCost total = {0};
    DWORD table = BgCompileRead32(file->data + 4) & 0xffffffu;
    for (DWORD record = table + 24; BgCompileRead32(file->data + record + 4); record += 24)
    {
        for (int layer = 0; layer < 2; layer++)
        {
            DWORD offset = BgCompileRead32(file->data + record + 4 + layer * 4) & 0xffffffu;
            BgCompileCost cost;
            if (!offset) { continue; }
            cost = BgCompileMeasure(file->data + offset, BgCompileRead32(file->data + offset - 4));
            total.textures += cost.textures; total.loads += cost.loads; total.vertices += cost.vertices;
        }
    }
    return total;
}

static BOOL BgCompileOptimizeInPlace(const BgFile *source, BgFile *out, const char **reasonout)
{
    BgFile cleaned = {0};
    BOOL ok;
    if (!BgFileRemoveUnusedVertices(source, &cleaned, reasonout)) { return FALSE; }
    ok = BgFileBatchOpaque(cleaned.data ? &cleaned : source, out, reasonout);
    if (ok && !out->data) { *out = cleaned; ZeroMemory(&cleaned, sizeof(cleaned)); }
    BgFileFree(&cleaned);
    return ok;
}

/* Apply the same compiler to unchanged/unopened levels during save/export.
 * Unknown pointer-bearing commands keep the existing in-place optimization;
 * known flat streams may relocate both layers while retaining their order. */
BOOL BgFileOptimize(const BgFile *source, BgFile *out, const char **reasonout)
{
    BgDocument document = {0};
    BgFile compiled = {0}, compact = {0};
    BgCompileCost before, after;
    BOOL cutoutalpha = FALSE;
    BOOL ok = FALSE;
    if (!out || out == source) { *reasonout = "Invalid background optimization output."; return FALSE; }
    ZeroMemory(out, sizeof(*out));
    if (!BgFileValidateVertexBatches(source, reasonout)) { return FALSE; }
    if (source->size < 8 || BgCompileRead32(source->data)) { return TRUE; }
    if (!BgDocumentLoad(source->data, source->size, 1.0f, &document, reasonout)) { return FALSE; }
    for (DWORD r = 1; r <= document.roomcount; r++) for (int layer = 0; layer < 2; layer++)
    {
        const BgDocumentLayerData *data = &document.rooms[r].layers[layer];
        BgRenderState state;
        BgRenderStateInit(&state, layer == BG_GEOMETRY_SECONDARY);
        for (DWORD g = 0; g < data->groupcount; g++)
        {
            const BgDocumentDrawGroup *group = &data->groups[g];
            for (DWORD c = 0; c + 8 <= group->commandsize; c += 8)
            { BgRenderStateRead(&state, BgCompileRead32(group->commands + c), BgCompileRead32(group->commands + c + 4)); }
            if (state.surfacepolicy == BG_SURFACE_CUTOUT) { cutoutalpha = TRUE; }
        }
        for (DWORD g = 0; g < data->groupcount; g++) if (!BgCompileRelocatableGroup(&data->groups[g]))
        {
            BgDocumentFree(&document);
            return BgCompileOptimizeInPlace(source, out, reasonout);
        }
    }
    if (!BgDocumentCompile(&document, source, &compiled, reasonout)
        || !BgFileValidateVertexBatches(&compiled, reasonout)
        || !BgFileCompact(&compiled, &compact, reasonout)) { goto done; }
    before = BgCompileFileCost(source); after = BgCompileFileCost(&compact);
    /* Older Cutout overrides may inherit an opaque mux across their tags.
     * Scope restoration is a correctness fix, even if it adds a few packets. */
    if (cutoutalpha || (after.textures <= before.textures && after.loads <= before.loads
        && after.vertices <= before.vertices && compact.size <= source->size))
    {
        if (compact.size != source->size || memcmp(compact.data, source->data, source->size))
        { *out = compact; ZeroMemory(&compact, sizeof(compact)); }
        ok = TRUE;
    }
    else { ok = BgCompileOptimizeInPlace(source, out, reasonout); }
done:
    BgDocumentFree(&document); BgFileFree(&compiled); BgFileFree(&compact);
    return ok;
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
