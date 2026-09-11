#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
/* BG-only history, with real document snapshots and compiler. */
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations=-1;
static BOOL FailAllocation(void)
{ if (!allocations) { return TRUE; } if (allocations>0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t size) { return FailAllocation() ? NULL : malloc(size); }
static void *TestCalloc(size_t count, size_t size) { return FailAllocation() ? NULL : calloc(count,size); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "bgprimitive.c"
#undef malloc
#undef calloc

static void Put(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void Float(unsigned char *p, float f) { DWORD v; memcpy(&v,&f,4); Put(p,v); }
static BgFile Fixture(void)
{
    BgFile bg={0}; bg.size=416; bg.data=calloc(bg.size,1); assert(bg.data);
    unsigned char *d=bg.data; strcpy(bg.name,"bg/bg_primitive_test.seg");
    Put(d+4,0x0e000020); Put(d+56,0x0e000100); Put(d+60,0x0e000140);
    Put(d+84,0x0e000190); /* Empty room 2 remains a real room. */
    Float(d+68,120.5f); Float(d+72,-31.25f); Float(d+76,84);
    Float(d+92,2000); Float(d+96,100); Float(d+100,100);
    Put(d+252,48); d[256+16+1]=10; d[256+32+3]=10;
    for (int v=0; v<3; v++) { memset(d+256+v*16+12,255,4); }
    /* Deliberately hostile state before adding the primitive: transparency,
       generated UVs, lighting, no depth, zero environment alpha, copy cycle. */
    const DWORD words[]={0xb6000000,1, 0xb7000000,0xe0000, 0xb9000020,0x4800,
        0xfb000000,0, 0xba001402,0x00300000, 0x04200030,0, 0xbf000000,0xa14, 0xb8000000,0};
    Put(d+316,sizeof(words));
    for (unsigned i=0; i<sizeof(words)/4; i++) { Put(d+320+i*4,words[i]); }
    Put(d+396,8); Put(d+400,0xb8000000);
    return bg;
}

static void CheckNew(const BgDocument *doc, const BgFaceRef *refs, DWORD count)
{
    BgDocumentRenderMesh mesh={0}; const char *why="";
    assert(BgDocumentBuildRenderMesh(doc,&mesh,&why));
    for (DWORD f=0; f<count; f++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face=BgDocumentFindFace(doc,&refs[f],&room); assert(face);
        assert(face->layer==BG_GEOMETRY_PRIMARY && face->textureid==BG_TEX_NONE && face->cullbackfaces);
        assert(BgMaterialTextureId(&face->material)==BG_TEX_NONE);
        for (int c=0; c<3; c++)
        {
            const BgDocumentVertex *v=&room->vertices[face->vertexindices[c]];
            assert(v->r==192 && v->g==192 && v->b==192 && v->a==255 && v->usecount);
        }
        BOOL found=FALSE;
        for (DWORD i=0; i<mesh.facecount; i++)
        {
            if (mesh.facerefs[i].faceid!=face->id) { continue; }
            found=TRUE;
            assert((mesh.renderflags[i]&(BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE))==3);
            assert(!(mesh.renderflags[i]&(BG_RENDER_ENVIRONMENT_MASK|BG_RENDER_ALPHA_TEST|BG_RENDER_DECAL|BG_RENDER_BLEND)));
            for (int c=0; c<3; c++)
            { BgVertex *v=&mesh.vertices[i*3+c]; assert(v->r==192 && v->g==192 && v->b==192 && v->a==255); }
        }
        assert(found);
    }
    BgDocumentRenderMeshFree(&mesh);
}

static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled={0}, saved={0}, again={0}; BgDocument loaded={0};
    BgDocumentRenderMesh a={0},b={0}; const char *why=""; char path[MAX_PATH];
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    /* The ROM builder runs this same native batch validator. */
    assert(BgFileValidateVertexBatches(&compiled,&why));
    snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    assert(saved.size==compiled.size && !memcmp(saved.data,compiled.data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    assert(loaded.facecount==doc->facecount && loaded.roomcount==doc->roomcount);
    assert(BgDocumentBuildRenderMesh(doc,&a,&why) && BgDocumentBuildRenderMesh(&loaded,&b,&why));
    assert(a.facecount==b.facecount);
    for (DWORD i=0; i<a.facecount; i++)
    {
        assert(a.tags[i]==b.tags[i] && a.renderflags[i]==b.renderflags[i]);
        for (int c=0; c<3; c++)
        {
            const BgVertex *v=&a.vertices[i*3+c], *w=&b.vertices[i*3+c];
            assert(v->x==w->x && v->y==w->y && v->z==w->z && v->s==w->s && v->t==w->t);
            assert(v->r==w->r && v->g==w->g && v->b==w->b && v->a==w->a);
        }
    }
    assert(BgDocumentCompile(&loaded,&saved,&again,&why) && BgFileValidateVertexBatches(&again,&why));
    /* The original room/portal/visibility metadata is not rewritten. */
    assert(!memcmp(source->data+8,compiled.data+8,24));
    BgDocumentRenderMeshFree(&a); BgDocumentRenderMeshFree(&b); BgDocumentFree(&loaded);
    BgFileFree(&compiled); BgFileFree(&saved); BgFileFree(&again);
}

static void CheckRound(const BgDocument *doc, BOOL cylinder, DWORD sides, double radius,
    double height, const double position[3], const BgFaceRef *refs, DWORD count)
{
    const BgDocumentRoom *room = &doc->rooms[2];
    DWORD vertices = cylinder ? sides*2 : sides;
    unsigned char edges[BG_PRIMITIVE_MAX_SIDES*2][BG_PRIMITIVE_MAX_SIDES*2] = {{0}};
    double tolerance = .51/doc->levelscale + .01;
    assert(count == (cylinder ? sides*2 : sides-2));
    assert(room->vertexcount == vertices && room->facecount == count);
    CheckNew(doc, refs, count);
    for (DWORD i = 0; i < vertices; i++)
    {
        float p[3];
        const BgDocumentVertex *v = &room->vertices[i];
        BgDocumentGetWorldPosition(doc, room, v, p);
        assert(fabs(hypot(p[0]-position[0], p[2]-position[2])-radius) < tolerance*1.5);
        assert(fabs(p[1]-position[1]-(i >= sides ? height : 0)) < tolerance);
        assert(v->y == room->vertices[i >= sides ? sides : 0].y);
        if (i >= sides)
        { assert(v->x == room->vertices[i-sides].x && v->z == room->vertices[i-sides].z); }
        for (DWORD j = 0; j < i; j++)
        {
            const BgDocumentVertex *w = &room->vertices[j];
            assert(v->x != w->x || v->y != w->y || v->z != w->z);
        }
    }
    for (DWORD i = 0; i < count; i++)
    {
        const BgDocumentFace *f = BgDocumentFindFace(doc, &refs[i], NULL);
        float p[3][3]; double a[3], b[3], normal[3];
        for (int c = 0; c < 3; c++)
        {
            DWORD x = f->vertexindices[c], y = f->vertexindices[(c+1)%3];
            assert(x < vertices && y < vertices && x != y);
            edges[x<y ? x : y][x<y ? y : x]++;
            BgDocumentGetWorldPosition(doc, room, &room->vertices[x], p[c]);
        }
        for (int axis = 0; axis < 3; axis++)
        { a[axis] = p[1][axis]-p[0][axis]; b[axis] = p[2][axis]-p[0][axis]; }
        for (int axis = 0; axis < 3; axis++)
        { int j=(axis+1)%3, k=(axis+2)%3; normal[axis]=a[j]*b[k]-a[k]*b[j]; }
        if (cylinder)
        {
            assert(normal[1] == 0);
            assert(normal[0]*((p[0][0]+p[1][0]+p[2][0])/3-position[0])
                 + normal[2]*((p[0][2]+p[1][2]+p[2][2])/3-position[2]) > 0);
        }
        else
        { assert(f->vertexindices[0] == 0 && normal[1] > 0 && normal[0] == 0 && normal[2] == 0); }
    }
    DWORD boundary = 0;
    for (DWORD i = 0; i < vertices; i++) for (DWORD j = i+1; j < vertices; j++)
    { assert(edges[i][j] <= 2); boundary += edges[i][j] == 1; }
    assert(boundary == (cylinder ? sides*2 : sides));
}

static void RoundPrimitives(const char *dir)
{
    const float scales[] = {.06f, .1f, .236f, 1};
    const DWORD counts[] = {3, 4, 8, 16, 64};
    const double position[3] = {1000, -200, 300};
    const char *why = "";
    for (unsigned scale=0; scale<4; scale++) for (unsigned n=0; n<5; n++) for (BOOL cylinder=0; cylinder<2; cylinder++)
    {
        DWORD sides=counts[n], count;
        /* Default size at eight sides, plus larger finely divided shapes. */
        double radius=sides<=8 ? 100 : 5000, height=100;
        BgFile source=Fixture(); BgDocument doc={0}; BgFaceRef refs[BG_PRIMITIVE_MAX_FACES];
        EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
        SetupFile setup={0}; StanFile stan={0};
        assert(BgDocumentLoad(source.data, source.size, scales[scale], &doc, &why));
        EditHistoryReset(&history, &doc, &setup, &stan);
        assert(EditHistoryBeginBgEdit(&history, &doc, "Round primitive", &tx, &why));
        assert(BgDocumentAddRoundPrimitive(&doc, cylinder, 2, position, radius, height, sides, refs, &count, &why));
        CheckRound(&doc, cylinder, sides, radius, height, position, refs, count);
        assert(EditHistoryCommitEdit(&history, &doc, &setup, &stan, &tx, &why));
        RoundTrip(&doc, &source, dir);
        assert(EditHistoryUndo(&history, &doc, &setup, &stan, &asset, &why));
        assert(doc.facecount==1 && !doc.rooms[2].vertexcount && !doc.dirty);
        assert(EditHistoryRedo(&history, &doc, &setup, &stan, &asset, &why));
        CheckRound(&doc, cylinder, sides, radius, height, position, refs, count);
        EditHistoryFree(&history); BgDocumentFree(&doc); BgFileFree(&source);
    }
    /* Invalid inputs, coarse quantization and all allocation failures are atomic. */
    BgFile source=Fixture(); BgDocument doc={0}; BgFaceRef refs[BG_PRIMITIVE_MAX_FACES]; DWORD count;
    assert(BgDocumentLoad(source.data, source.size, .06f, &doc, &why));
    BgDocument before=doc; BgDocumentRoom room=doc.rooms[1];
    for (BOOL cylinder=0; cylinder<2; cylinder++)
    {
        for (int budget=0; budget<4; budget++)
        {
            allocations=budget;
            assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 1, position, 100, 100, 8, refs, &count, &why));
            allocations=-1;
            assert(!count && !memcmp(&before,&doc,sizeof(doc)) && !memcmp(&room,&doc.rooms[1],sizeof(room)));
        }
        const double bad[]={0, -1, NAN, INFINITY, 1e-12, 1e20};
        for (unsigned i=0; i<sizeof(bad)/sizeof(*bad); i++)
        {
            assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 1, position, bad[i], 100, 8, refs, &count, &why));
            assert(!count);
            if (cylinder)
            { assert(!BgDocumentAddRoundPrimitive(&doc, TRUE, 1, position, 100, bad[i], 8, refs, &count, &why) && !count); }
        }
        const DWORD badsides[]={0, 1, 2, 65, 0xffffffffu};
        for (unsigned i=0; i<sizeof(badsides)/sizeof(*badsides); i++)
        { assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 1, position, 100, 100, badsides[i], refs, &count, &why) && !count); }
        assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 1, position, 1, 100, 64, refs, &count, &why) && !count);
        assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 99, position, 100, 100, 8, refs, &count, &why) && !count);
        assert(!BgDocumentAddRoundPrimitive(&doc, cylinder, 1, (double[3]){0,NAN,0}, 100, 100, 8, refs, &count, &why) && !count);
        assert(!memcmp(&before,&doc,sizeof(doc)) && !memcmp(&room,&doc.rooms[1],sizeof(room)));
    }
    /* Drop into the void inherits the same nearest-room choice. */
    assert(BgDocumentAddRoundPrimitive(&doc, FALSE, 0, position, 100, 100, 8, refs, &count, &why) && refs[0].room==1);
    CheckNew(&doc, refs, count); RoundTrip(&doc, &source, dir);
    BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS: circle perimeter fans and uncapped cylinders (3-64 sides), axes, exterior winding, shared vertices, dimensions, save/ROM batches, undo/redo, and atomic failures.");
}

int main(int argc, char **argv)
{
    const float scales[]={.06f,.1f,.236f,1};
    const double rights[][3]={{1,0,0},{0,0,-1},{.6,0,.8}};
    const double position[3]={1000,-200,300};
    const char *why=""; assert(argc==2);
    RoundPrimitives(argv[1]);
    for (unsigned scale=0; scale<4; scale++) for (unsigned direction=0; direction<3; direction++)
    {
        BgFile source=Fixture(); BgDocument doc={0}; BgFaceRef refs[2]; DWORD count;
        assert(BgDocumentLoad(source.data,source.size,scales[scale],&doc,&why));
        assert(doc.facecount==1 && doc.roomcount==2);
        EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
        SetupFile setup={0}; StanFile stan={0};
        EditHistoryReset(&history,&doc,&setup,&stan);
        BgDocumentVertex original[3]; memcpy(original,doc.rooms[1].vertices,sizeof(original));
        assert(EditHistoryBeginBgEdit(&history,&doc,"Add BG Triangle",&tx,&why));
        assert(BgDocumentAddPrimitive(&doc,FALSE,1,position,rights[direction],refs,&count,&why) && count==1);
        assert(doc.facecount==2 && doc.rooms[1].vertexcount==6 && doc.dirty);
        assert(!memcmp(original,doc.rooms[1].vertices,sizeof(original)));
        CheckNew(&doc,refs,count);
        const BgDocumentFace *f=BgDocumentFindFace(&doc,refs,NULL); float p[3][3];
        for (int c=0; c<3; c++) { BgDocumentGetWorldPosition(&doc,&doc.rooms[1],&doc.rooms[1].vertices[f->vertexindices[c]],p[c]); }
        double width=hypot(p[1][0]-p[0][0],p[1][2]-p[0][2]);
        assert(fabs(width-100)<1.5/doc.levelscale && fabs(p[2][1]-p[0][1]-100)<1.1/doc.levelscale);
        assert((p[1][0]-p[0][0])*rights[direction][0]+(p[1][2]-p[0][2])*rights[direction][2]>0);
        assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&tx,&why));
        RoundTrip(&doc,&source,argv[1]); EditHistoryMarkBgSaved(&history,&doc);
        assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why) && doc.facecount==1 && doc.dirty);
        assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why) && doc.facecount==2 && !doc.dirty);
        assert(EditHistoryBeginBgEdit(&history,&doc,"Add BG Quad",&tx,&why));
        /* Explicit placement into an empty room creates its first draw group. */
        assert(BgDocumentAddPrimitive(&doc,TRUE,2,position,rights[direction],refs,&count,&why) && count==2);
        assert(doc.rooms[2].vertexcount==4 && doc.rooms[2].facecount==2);
        const BgDocumentFace *a=BgDocumentFindFace(&doc,&refs[0],NULL), *b=BgDocumentFindFace(&doc,&refs[1],NULL);
        assert(a->vertexindices[0]==b->vertexindices[0] && a->vertexindices[2]==b->vertexindices[1]);
        CheckNew(&doc,refs,count); RoundTrip(&doc,&source,argv[1]);
        EditHistoryRollbackEdit(&tx,&doc,&setup,&stan); assert(doc.facecount==2 && !doc.rooms[2].facecount);
        /* No-hint placement uses nearby occupied geometry instead of an unused room. */
        assert(BgDocumentAddPrimitive(&doc,TRUE,0,position,rights[direction],refs,&count,&why) && refs[0].room==1);
        CheckNew(&doc,refs,count); RoundTrip(&doc,&source,argv[1]);
        EditHistoryFree(&history); BgDocumentFree(&doc); BgFileFree(&source);
    }
    BgFile source=Fixture(); BgDocument doc={0}; BgFaceRef refs[2]; DWORD count;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    for (int budget=0; budget<4; budget++)
    {
        BgDocument before=doc; BgDocumentRoom room=doc.rooms[1]; allocations=budget;
        assert(!BgDocumentAddPrimitive(&doc,TRUE,1,position,rights[0],refs,&count,&why)); allocations=-1;
        assert(!count && !memcmp(&before,&doc,sizeof(doc)) && !memcmp(&room,&doc.rooms[1],sizeof(room)));
    }
    assert(!BgDocumentAddPrimitive(&doc,FALSE,99,position,rights[0],refs,&count,&why));
    assert(!BgDocumentAddPrimitive(&doc,FALSE,1,(double[3]){1e10,0,0},rights[0],refs,&count,&why));
    assert(!BgDocumentAddPrimitive(&doc,FALSE,1,(double[3]){NAN,0,0},rights[0],refs,&count,&why));
    assert(!BgDocumentAddPrimitive(&doc,FALSE,1,position,(double[3]){0,1,0},refs,&count,&why));
    doc.levelscale=.00001f;
    assert(!BgDocumentAddPrimitive(&doc,FALSE,1,position,rights[0],refs,&count,&why));
    assert(doc.facecount==1 && !doc.dirty);
    BgDocumentFree(&doc); BgFileFree(&source);
    puts("PASS: triangle/quad dimensions, shared vertices, colors/state, room choice, native save/reload, ROM batch validation, undo/redo, and atomic failures.");
    return 0;
}
