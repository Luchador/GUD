#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void *HWND;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLubyte;
#define WM_APP 0x8000
#include "uvcanvas.h"
#include "types.inc"

typedef struct ViewportState {
    Vertex *scene;
    BgFaceRef *scenefacerefs;
    BgDocumentVertexRef *scenevertexrefs;
    SceneBatch *batches;
    int scenecount, batchcount;
    ViewportUVPreviewCorner *uvpreview;
    int uvpreviewcount;
    BOOL uvpreviewactive;
} ViewportState;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
static int invalidations, paints, budget = -1, allocations;
static void InvalidateRect(HWND hwnd, const void *rect, BOOL erase) { assert(!erase); invalidations++; }
static void UpdateWindow(HWND hwnd) { paints++; }
static BOOL ViewportGetTextureSize(HWND hwnd, unsigned short id, int *w, int *h)
{ *w = id == 1 ? 32 : 64; *h = id == 1 ? 64 : 32; return TRUE; }
static void *TestMalloc(size_t size)
{ allocations++; if (!budget) return NULL; if (budget > 0) budget--; return malloc(size); }
#define malloc TestMalloc
#include "logic.inc"
#undef malloc

static void Background(void)
{
    Vertex scene[18] = {0}, original[18]; BgFaceRef faces[6] = {0};
    BgDocumentVertexRef refs[18]; SceneBatch batches[6] = {0};
    BgDocumentVertex vertices[2][3] = {{{0}}}; BgDocumentRoom rooms[3] = {0};
    BgDocument doc = {.rooms=rooms, .roomcount=2};
    for (DWORD r=0;r<2;r++)
    {
        rooms[r+1].vertices=vertices[r]; rooms[r+1].vertexcount=3;
        for (DWORD v=0;v<3;v++) vertices[r][v]=(BgDocumentVertex){.id=10+r*10+v,.room=r+1,.s=64+v*32,.t=32+v*64};
    }
    for (int t=0;t<6;t++)
    {
        DWORD room=t==2?2:1;
        faces[t]=(BgFaceRef){.faceid=t+1,.room=room};
        batches[t]=(SceneBatch){.first=t*3,.count=3,.gltex=1,.textureid=t==1?2:1};
        for (int c=0;c<3;c++)
        {
            refs[t*3+c]=(BgDocumentVertexRef){room,c};
            scene[t*3+c].s=vertices[room-1][c].s/(t==1?2048.0f:1024.0f);
            scene[t*3+c].t=vertices[room-1][c].t/(t==1?1024.0f:2048.0f);
            scene[t*3+c].x=t*10+c; scene[t*3+c].r=177;
        }
    }
    batches[1].secondary=TRUE; batches[3].object=TRUE;
    batches[4].renderflags=BG_RENDER_ENVIRONMENT_MASK; batches[5].gltex=0;
    memcpy(original,scene,sizeof(scene));
    ViewportState s={.scene=scene,.scenefacerefs=faces,.scenevertexrefs=refs,.batches=batches,.scenecount=18,.batchcount=6};
    BgDocumentUVEdit edits[2]={{.vertex={1,0},.vertexid=10,.s=192,.t=32},
                              {.vertex={1,2},.vertexid=12,.s=-96,.t=96}};
    UVCanvasPreview preview={.vertices=edits,.count=2};
    budget=0; assert(!ViewportPreviewUVs(&s,&doc,&preview)); budget=-1;
    assert(!memcmp(scene,original,sizeof(scene)) && !s.uvpreviewactive);
    assert(ViewportPreviewUVs(&s,&doc,&preview) && s.uvpreviewcount==4);
    assert(scene[0].s==192/1024.0f && scene[3].s==192/2048.0f);
    assert(scene[2].t==96/2048.0f && scene[5].t==96/1024.0f);
    assert(scene[0].t==original[0].t && !memcmp(&scene[6],&original[6],12*sizeof(Vertex)));
    assert(!memcmp(&scene[1],&original[1],sizeof(Vertex)) && !memcmp(&scene[4],&original[4],sizeof(Vertex)));
    assert(vertices[0][0].s==64 && vertices[0][2].s==128 && !doc.dirty);
    /* Subsequent updates reuse the mapping even if allocation would fail. */
    int allocated=allocations, painted=paints; budget=0;
    edits[0].s=320; assert(ViewportPreviewUVs(&s,&doc,&preview));
    assert(scene[0].s==320/1024.0f && paints==painted+1 && allocations==allocated);
    assert(ViewportPreviewUVs(&s,&doc,&preview) && paints==painted+1); /* no redundant paint */
    edits[0].s=64; edits[1].s=128; edits[1].t=160;
    assert(ViewportPreviewUVs(&s,&doc,&preview) && !memcmp(scene,original,sizeof(scene))); budget=-1;
    edits[0].s=-32768; edits[1].t=32767;
    assert(ViewportPreviewUVs(&s,&doc,&preview));
    painted=paints;
    assert(ViewportPreviewUVs(&s,&doc,NULL) && paints==painted); /* no intermediate old frame on commit */
    assert(!memcmp(scene,original,sizeof(scene)) && !s.uvpreview && !s.uvpreviewactive);
    assert(ViewportPreviewUVs(&s,&doc,&preview)); edits[0].s=32768;
    assert(!ViewportPreviewUVs(&s,&doc,&preview) && !memcmp(scene,original,sizeof(scene)));
    puts("PASS: live BG UVs update shared vertices in both layers using each texture's dimensions; room/object/reflection isolation, native limits, cancellation, allocation reuse and unchanged document.");
}

static void Model(void)
{
    Vertex scene[9]={0}, original[9]; BgFaceRef faces[3]={{.faceid=200,.room=1},{.faceid=100,.room=1},{.faceid=300,.room=1}};
    BgDocumentVertexRef refs[9]={{0}};
    SceneBatch batch={.first=0,.count=9,.gltex=1,.textureid=1};
    ViewportState s={.scene=scene,.scenefacerefs=faces,.scenevertexrefs=refs,.batches=&batch,.scenecount=9,.batchcount=1};
    UVCanvasTriangle triangles[2]={0};
    for (int i=0;i<9;i++) { scene[i].s=.123456f+i*.0123f; scene[i].t=.2511f+i*.027f; }
    memcpy(original,scene,sizeof(scene));
    for (int i=0;i<2;i++)
    {
        triangles[i].face=(BgFaceRef){.room=1,.faceid=i==0?100:200};
        triangles[i].width=i==0?32:64; triangles[i].height=64;
        for (int c=0;c<3;c++) triangles[i].source[c]=(BgDocumentUVEdit){.vertex={1,c+10},.vertexid=c+11,.s=126,.t=257};
    }
    BgDocumentUVEdit edits[1]={triangles[0].source[0]}; edits[0].t+=32;
    UVCanvasPreview preview={edits,1,triangles,2};
    for (int fail=0;fail<2;fail++)
    {
        budget=fail; assert(!ViewportPreviewUVs(&s,NULL,&preview)); budget=-1;
        assert(!s.uvpreviewactive && !memcmp(scene,original,sizeof(scene)));
    }
    assert(ViewportPreviewUVs(&s,NULL,&preview) && s.uvpreviewcount==2);
    assert(scene[0].s==original[0].s && scene[3].s==original[3].s); /* retain sub-native authored precision */
    assert(scene[0].t==289/2048.0f && scene[3].t==289/2048.0f);
    assert(!memcmp(&scene[6],&original[6],3*sizeof(Vertex))); /* unselected face */
    edits[0].s+=32;
    assert(ViewportPreviewUVs(&s,NULL,&preview) && scene[0].s==158/2048.0f && scene[3].s==158/1024.0f);
    edits[0].vertexid++;
    assert(!ViewportPreviewUVs(&s,NULL,&preview) && !memcmp(scene,original,sizeof(scene))); /* stale source rejected */
    edits[0]=triangles[0].source[0]; edits[0].t+=64;
    assert(ViewportPreviewUVs(&s,NULL,&preview));
    assert(ViewportPreviewUVs(&s,NULL,NULL) && !memcmp(scene,original,sizeof(scene)) && !s.uvpreview);
    assert(ViewportPreviewUVs(NULL,NULL,NULL));
    puts("PASS: live model preview resolves sorted scene faces and shared UV nodes, preserves untouched UV precision/corners, restores exactly and rejects stale identities.");
}
int main(void) { Background(); Model(); return 0; }
