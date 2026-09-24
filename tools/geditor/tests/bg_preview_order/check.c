#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
typedef unsigned int GLuint, GLenum;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
#include "types.inc"
enum { GL_FRONT = 1, GL_BACK, GL_FRONT_AND_BACK };
typedef struct ViewportState {
    BOOL cullbackfaces, showbgprimary, showbgsecondary, showobjects;
    ViewportRenderMode rendermode;
    Vertex *scene;
    SceneBatch *batches;
    int batchcount;
    unsigned char *hiddentris;
    ViewportTexture *texturecache;
} ViewportState;
static void ViewportEnvironmentAxes(const ViewportState *s, float *r, float *u) { abort(); }
static void ViewportEnvironmentCoordinates(const ViewportState *s, int i, BgRenderFlags f,
    const float *r, const float *u, float *uv) { abort(); }
#include "logic.inc"

/* One batch per face keeps exactly the production mesh order. The production
 * picker replays depth tests and the decal bias used by the GL preview. */
static DWORD Visible(const BgDocumentRenderMesh *mesh, const double point[3], BOOL secondary)
{
    ViewportState state = {0};
    ViewportPickRay ray = {{point[0], point[1], point[2] + 100}, {0, 0, -1}, 0, 200};
    double distance;
    state.showbgprimary = state.cullbackfaces = TRUE;
    state.showbgsecondary = secondary;
    state.scene = calloc(mesh->facecount * 3, sizeof(*state.scene));
    state.batches = calloc(mesh->facecount, sizeof(*state.batches));
    assert(state.scene && state.batches);
    state.batchcount = (int)mesh->facecount;
    for (DWORD i = 0; i < mesh->facecount; i++)
    {
        SceneBatch *b = &state.batches[i];
        b->first = (int)i * 3; b->count = 3;
        b->secondary = BG_TRI_IS_SECONDARY(mesh->tags[i]);
        b->cullbackfaces = (mesh->tags[i] & BG_TRI_CULL_BACK) != 0;
        b->renderflags = mesh->renderflags[i];
        for (int c = 0; c < 3; c++)
        {
            const BgVertex *v = &mesh->vertices[i * 3 + c];
            Vertex *out = &state.scene[i * 3 + c];
            out->x = v->x; out->y = v->y; out->z = v->z;
            out->a = v->a;
        }
    }
    int hit = ViewportFindSceneTriangle(&state, &ray, &distance, TRUE);
    DWORD id = hit < 0 ? BG_FACE_ID_NONE : mesh->facerefs[hit].faceid;
    free(state.scene); free(state.batches);
    return id;
}

static void Synthetic(void)
{
    /* A square sign straddles the diagonal between two wall triangles. The
     * upper wall was appended after the sign, as happens with bridge edits. */
    BgDocumentVertex vertices[8] = {
        {.x=0,.y=0,.a=255}, {.x=100,.y=0,.a=255},
        {.x=0,.y=100,.a=255}, {.x=100,.y=100,.a=255},
        {.x=60,.y=60,.a=255}, {.x=40,.y=60,.a=255},
        {.x=40,.y=40,.a=255}, {.x=60,.y=40,.a=255}
    };
    unsigned char decal[] = {0xB9,0,3,0x1D, 0,0,0x0C,0x10};
    BgDocumentDrawGroup primary[] = {{0}, {.commands=decal,.commandsize=8}};
    BgDocumentDrawGroup secondary[] = {{.commands=decal,.commandsize=8}, {0}};
    BgDocumentFace faces[] = {
        {.id=1,.vertexindices={0,1,2},.drawgroup=0},
        {.id=5,.vertexindices={4,5,6},.drawgroup=1,.layer=1},
        {.id=2,.vertexindices={4,5,6},.drawgroup=1},
        {.id=6,.vertexindices={4,5,6},.drawgroup=0,.layer=1},
        {.id=3,.vertexindices={4,6,7},.drawgroup=1},
        {.id=4,.vertexindices={2,1,3},.drawgroup=0},
        {.id=7,.vertexindices={4,5,6},.drawgroup=1,.layer=1}
    };
    BgDocumentRoom rooms[2] = {0};
    BgDocument document = {.rooms=rooms,.roomcount=1,.facecount=7,.levelscale=1};
    BgDocumentRenderMesh mesh = {0};
    BgDocumentFace original[7];
    const char *reason;
    const DWORD expected[] = {1,4,2,3,6,5,7};
    const double upper[] = {57,58,0}, lower[] = {43,42,0};
    rooms[1] = (BgDocumentRoom){.vertices=vertices,.vertexcount=8,.faces=faces,.facecount=7,
        .layers={{.groups=primary,.groupcount=2},{.groups=secondary,.groupcount=2}}};
    for (unsigned i = 0; i < 7; i++)
    {
        faces[i].room = faces[i].cullbackfaces = 1;
        faces[i].uvseams = i % 8;
        BgMaterialInit(&faces[i].material);
    }
    memcpy(original, faces, sizeof(faces));
    assert(BgDocumentBuildRenderMesh(&document, &mesh, &reason));
    assert(Visible(&mesh, upper, FALSE) == 2); /* Used to hit the appended wall. */
    assert(Visible(&mesh, lower, FALSE) == 3);
    assert(Visible(&mesh, upper, TRUE) == 7); /* Last overlapping decal wins. */
    assert(!memcmp(original, faces, sizeof(faces)) && !document.dirty);
    for (DWORD i = 0; i < mesh.facecount; i++)
    {
        const BgDocumentFace *face = BgDocumentFindFace(&document, &mesh.facerefs[i], NULL);
        assert(mesh.facerefs[i].faceid == expected[i] && face);
        assert(mesh.facerefs[i].seams == face->uvseams);
        for (int c = 0; c < 3; c++)
        {
            const BgDocumentVertexRef *ref = &mesh.vertexrefs[i * 3 + c];
            assert(ref->room == 1 && ref->index == face->vertexindices[c]);
            assert(mesh.vertices[i * 3 + c].x == vertices[ref->index].x);
        }
    }
    BgDocumentRenderMeshFree(&mesh);
    /* Decals must still be hidden by genuinely closer geometry. */
    for (int i = 0; i < 4; i++) { vertices[i].z = 1; }
    assert(BgDocumentBuildRenderMesh(&document, &mesh, &reason));
    assert(Visible(&mesh, upper, TRUE) == 4);
    assert(Visible(&mesh, lower, TRUE) == 1);
    BgDocumentRenderMeshFree(&mesh);
    puts("PASS: appended wall, decal overlap/layers, stable face/vertex references, occlusion");
}

static void Control(const char *path)
{
    FILE *file = fopen(path, "rb");
    assert(file && !fseek(file, 0, SEEK_END));
    long size = ftell(file);
    assert(size > 0 && size <= UINT32_MAX);
    rewind(file);
    unsigned char *data = malloc((size_t)size);
    assert(data && fread(data, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    BgDocument document = {0};
    BgDocumentRenderMesh mesh = {0};
    const char *reason;
    assert(BgDocumentLoad(data, (DWORD)size, 0.49886572f, &document, &reason));
    assert(document.roomcount >= 14);
    BgDocumentRoom *room = &document.rooms[14];
    DWORD wall = 0;
    while (wall < room->facecount && room->faces[wall].id != 2690) { wall++; }
    assert(wall < room->facecount);
    BgDocumentFace appended = room->faces[wall];
    memmove(room->faces + wall, room->faces + wall + 1,
        (room->facecount - wall - 1) * sizeof(*room->faces));
    appended.id = 13792; /* Reproduce an unsaved newly created face. */
    room->faces[room->facecount - 1] = appended;
    assert(BgDocumentBuildRenderMesh(&document, &mesh, &reason));
    const short native[2][3] = {{-61,36,-85}, {-72,16,-85}};
    for (int i = 0; i < 2; i++)
    {
        double point[3];
        for (int axis = 0; axis < 3; axis++)
        { point[axis] = (native[i][axis] + room->origin[axis]) / document.levelscale; }
        assert(Visible(&mesh, point, FALSE) == (DWORD)(2706 + i));
    }
    assert(room->faces[room->facecount - 1].id == 13792);
    BgDocumentRenderMeshFree(&mesh); BgDocumentFree(&document); free(data);
    puts("PASS: Control warning faces 2706/2707 over appended wall");
}

int main(int argc, char **argv)
{
    Synthetic();
    if (argc > 1) { Control(argv[1]); }
    return 0;
}
