#ifndef GEDITOR_BGDOCUMENT_H
#define GEDITOR_BGDOCUMENT_H

#include <windows.h>

#include "bgload.h"

/* Stable identity used between the editable document and presentation
 * layers. A face remains identifiable even when the viewport sorts its
 * triangles by texture or a room's arrays are later reallocated. */
#define BG_FACE_ID_NONE 0u

typedef enum BgGeometryLayer {
    BG_GEOMETRY_PRIMARY = 0,
    BG_GEOMETRY_SECONDARY = 1
} BgGeometryLayer;

typedef struct BgFaceRef {
    DWORD faceid;
    unsigned short room;
    unsigned char layer;
    unsigned char reserved;
} BgFaceRef;

/* Exact, room-local N64 vertex data. Positions and UVs deliberately remain
 * integral here so opening and inspecting a level can never introduce float
 * round-trip error. */
typedef struct BgDocumentVertex {
    DWORD id;
    unsigned short room;
    short x, y, z;
    unsigned short flag;
    short s, t;
    unsigned char r, g, b, a;
    DWORD usecount;
} BgDocumentVertex;

typedef struct BgDocumentFace {
    DWORD id;
    DWORD vertexindices[3];
    unsigned short room;
    unsigned short textureid;
    unsigned char layer;
    unsigned char cullbackfaces;

    /* Keep the complete source marker. The low 12 bits identify the image,
     * while the remaining bits select GoldenEye texture-loading behavior. */
    DWORD textureword0;
    DWORD textureword1;
} BgDocumentFace;

typedef struct BgDocumentRoom {
    float origin[3];
    BgDocumentVertex *vertices;
    DWORD vertexcount;
    BgDocumentFace *faces;
    DWORD facecount;
    DWORD facecapacity;
} BgDocumentRoom;

typedef struct BgDocument {
    BgDocumentRoom *rooms; /* indexed by authored room number; room 0 unused */
    DWORD roomcount;
    DWORD facecount;
    DWORD nextvertexid;
    DWORD nextfaceid;
    float levelscale;
    BOOL dirty;
} BgDocument;

typedef struct BgDocumentRenderMesh {
    BgVertex *vertices;       /* facecount * 3 world-space vertices */
    unsigned short *tags;     /* one BG_* tag per face */
    BgFaceRef *facerefs;      /* one stable document identity per face */
    DWORD facecount;
} BgDocumentRenderMesh;

BOOL BgDocumentLoad(const unsigned char *data, DWORD size, float levelscale,
                    BgDocument *out, const char **reasonout);
void BgDocumentFree(BgDocument *document);

BOOL BgDocumentBuildRenderMesh(const BgDocument *document,
                               BgDocumentRenderMesh *out,
                               const char **reasonout);
void BgDocumentRenderMeshFree(BgDocumentRenderMesh *mesh);

const BgDocumentRoom *BgDocumentGetRoom(const BgDocument *document,
                                        DWORD room);
const BgDocumentFace *BgDocumentFindFace(const BgDocument *document,
                                         const BgFaceRef *ref,
                                         const BgDocumentRoom **roomout);
void BgDocumentGetWorldPosition(const BgDocument *document,
                                const BgDocumentRoom *room,
                                const BgDocumentVertex *vertex,
                                float position[3]);

#endif /* GEDITOR_BGDOCUMENT_H */
