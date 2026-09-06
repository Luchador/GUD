/*
 * Editable, room-aware view of a GoldenEye background.
 *
 * This parser intentionally does not replace the raw BgFile yet. The raw
 * segment remains the lossless save source until the BG compiler lands, while
 * BgDocument supplies stable room, face, and shared-vertex identities to the
 * editor. Both primary and secondary display lists reference the same room
 * vertex table, so preserving those indices is essential for later topology
 * editing and vertex painting.
 */

#include <windows.h>
#include <stdlib.h>
#include <string.h>

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
    DWORD pc;
    DWORD batchfirst = 0;
    DWORD batchcount = 0;
    int batchv0 = 0;
    DWORD textureword0 = 0;
    DWORD textureword1 = 0;
    BOOL cullbackfaces = FALSE;

    for (pc = gdloffset;
         pc + 8 <= gdloffset + gdlsize && pc + 8 <= size;
         pc += 8)
    {
        const unsigned char *command = data + pc;

        if (command[0] == BGDOC_G_ENDDL)
        {
            return TRUE;
        }

        if (command[0] == BGDOC_G_NOOP)
        {
            textureword0 = BgDocumentRead32(command);
            textureword1 = BgDocumentRead32(command + 4);
            continue;
        }

        if (command[0] == BGDOC_G_SETGEOMETRYMODE)
        {
            if (BgDocumentRead32(command + 4) & BGDOC_G_CULL_BACK)
            {
                cullbackfaces = TRUE;
            }
            continue;
        }

        if (command[0] == BGDOC_G_CLEARGEOMETRYMODE)
        {
            if (BgDocumentRead32(command + 4) & BGDOC_G_CULL_BACK)
            {
                cullbackfaces = FALSE;
            }
            continue;
        }

        if (command[0] == BGDOC_G_VTX)
        {
            DWORD addressoffset = BgDocumentRead32(command + 4)
                                & 0x00FFFFFFu;

            batchcount = ((command[1] >> 4) & 0xF) + 1;
            batchv0 = command[1] & 0xF;

            if ((addressoffset & 15) != 0
                || addressoffset / 16 >= room->vertexcount
                || batchcount > room->vertexcount - addressoffset / 16)
            {
                batchcount = 0;
            }
            else
            {
                batchfirst = addressoffset / 16;
            }
            continue;
        }

        if ((command[0] == BGDOC_G_TRI1 || command[0] == BGDOC_G_TRI4)
            && batchcount != 0)
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

                /* An unused TRI4 slot is encoded as the degenerate 0,0,0. */
                if (cacheindices[0] == cacheindices[1]
                    && cacheindices[1] == cacheindices[2])
                {
                    continue;
                }

                for (corner = 0; corner < 3; corner++)
                {
                    cacheindices[corner] -= batchv0;
                    if (cacheindices[corner] < 0
                        || cacheindices[corner] >= (int)batchcount)
                    {
                        valid = FALSE;
                        break;
                    }
                    vertexindices[corner] = batchfirst
                                          + (DWORD)cacheindices[corner];
                }

                if (valid
                    && !BgDocumentAppendFace(document, room, roomnumber,
                                             vertexindices,
                                             layer, textureword0,
                                             textureword1,
                                             cullbackfaces))
                {
                    return FALSE;
                }
            }
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

    if (out->facecount == 0)
    {
        BgDocumentFree(out);
        *reasonout = "bg file produced no editable triangles.";
        return FALSE;
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
        free(document->rooms[room].vertices);
        free(document->rooms[room].faces);
    }
    free(document->rooms);
    ZeroMemory(document, sizeof(*document));
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

    if (document == NULL || document->rooms == NULL
        || document->facecount == 0)
    {
        *reasonout = "there is no editable bg geometry to render.";
        return FALSE;
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
