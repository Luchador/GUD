#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations = -1;
static BOOL FailAllocation(void)
{ if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t size) { return FailAllocation() ? NULL : malloc(size); }
static void *TestCalloc(size_t count, size_t size) { return FailAllocation() ? NULL : calloc(count, size); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "bgextrude.c"
#undef malloc
#undef calloc
#include "fixture.inc"

static BgDocumentEdgeRef Edge(const BgDocumentFace *face, unsigned int corner)
{ return (BgDocumentEdgeRef){{face->id,face->room,face->layer,0},corner}; }

static void MakeStrip(BgDocument *doc)
{
    BgDocumentRoom *r = &doc->rooms[1];
    r->vertices = realloc(r->vertices, 4*sizeof(*r->vertices)); assert(r->vertices);
    r->faces = realloc(r->faces, 2*sizeof(*r->faces)); assert(r->faces);
    r->vertices[3] = r->vertices[0]; r->vertices[3].id = doc->nextvertexid++;
    const short positions[4][2] = {{0,0},{100,0},{0,100},{200,0}};
    for (int i=0; i<4; i++)
    {
        BgDocumentVertex *v = &r->vertices[i];
        v->x=positions[i][0]; v->y=positions[i][1]; v->z=0;
        v->s=v->x*16; v->t=v->y*32; v->r=60+i*30; v->g=100; v->b=200; v->a=190;
        v->usecount = i==1 || i==2 ? 2 : 1;
    }
    r->vertexcount=4;
    r->faces[0].textureid=17; r->faces[0].cullbackfaces=TRUE;
    BgMaterialSetTexture(&r->faces[0].material,17);
    r->faces[1]=r->faces[0]; r->faces[1].id=doc->nextfaceid++;
    r->faces[1].vertexindices[0]=1; r->faces[1].vertexindices[1]=3; r->faces[1].vertexindices[2]=2;
    r->facecount=r->facecapacity=doc->facecount=2;
}

static void VerifySource(const BgDocument *before, const BgDocument *doc)
{
    for (DWORD r=1; r<=before->roomcount; r++)
    {
        const BgDocumentRoom *a=&before->rooms[r], *b=&doc->rooms[r];
        if (a->facecount) { assert(!memcmp(a->faces,b->faces,a->facecount*sizeof(*a->faces))); }
        for (DWORD i=0; i<a->vertexcount; i++)
        {
            BgDocumentVertex v=b->vertices[i]; v.usecount=a->vertices[i].usecount;
            assert(!memcmp(&a->vertices[i],&v,sizeof(v)));
        }
    }
}

static void VerifyPreview(const BgDocument *doc, const BgVertex *preview, const BgDocumentEdgeRef *out, DWORD n)
{
    BgDocumentRenderMesh mesh={0}; const char *why="";
    assert(BgDocumentBuildRenderMesh(doc,&mesh,&why));
    for (DWORD i=0; i<n; i++) for (int tri=0; tri<2; tri++)
    {
        DWORD face;
        for (face=0; face<mesh.facecount; face++)
        { if (mesh.facerefs[face].faceid == out[i].face.faceid-1+tri) { break; } }
        assert(face<mesh.facecount);
        for (int c=0; c<3; c++)
        {
            const BgVertex *a=&preview[i*6+tri*3+c], *b=&mesh.vertices[face*3+c];
            assert(a->x==b->x && a->y==b->y && a->z==b->z && a->s==b->s && a->t==b->t && a->a==b->a);
        }
    }
    BgDocumentRenderMeshFree(&mesh);
}

static void CheckCounts(const BgDocument *doc)
{
    for (DWORD r=1; r<=doc->roomcount; r++)
    {
        const BgDocumentRoom *room=&doc->rooms[r];
        for (DWORD v=0; v<room->vertexcount; v++)
        {
            DWORD uses=0;
            for (DWORD f=0; f<room->facecount; f++) for (int c=0; c<3; c++)
            { uses+=room->faces[f].vertexindices[c]==v; }
            assert(uses==room->vertices[v].usecount);
            for (DWORD other=0; other<v; other++) { assert(room->vertices[v].id!=room->vertices[other].id); }
        }
    }
}

int main(int argc, char **argv)
{
    assert(argc==2);
    const float scales[]={.06f,.1f,.236f,1};
    const char *why="";
    for (unsigned scale=0; scale<4; scale++) for (int direction=0; direction<3; direction++)
    {
        BgFile source=Fixture(); BgDocument doc={0}, before={0};
        EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
        SetupFile setup={0}; StanFile stan={0};
        assert(BgDocumentLoad(source.data,source.size,scales[scale],&doc,&why)); MakeStrip(&doc);
        assert(BgDocumentClone(&doc,&before,&why));
        EditHistoryReset(&history,&doc,&setup,&stan);
        BgDocumentEdgeRef edges[2]={Edge(&doc.rooms[1].faces[0],0),Edge(&doc.rooms[1].faces[1],0)}, out[2];
        double offset[3]={0,0,0}, applied[3]; BgVertex preview[12]; DWORD count;
        offset[direction==0?1:2]=(direction==2?-31:31)/doc.levelscale;
        assert(BgDocumentPreviewEdgeExtrusion(&doc,edges,2,offset,preview,applied,&why));
        assert(!doc.dirty && doc.facecount==2);
        assert(EditHistoryBeginBgEdit(&history,&doc,"Extrude BG Edges",&tx,&why));
        assert(BgDocumentExtrudeEdges(&doc,edges,2,offset,out,&count,&why) && count==2);
        assert(doc.facecount==6 && doc.rooms[1].vertexcount==7 && doc.dirty);
        VerifySource(&before,&doc); CheckCounts(&doc); VerifyPreview(&doc,preview,out,2);
        for (DWORD i=0; i<count; i++)
        {
            const BgDocumentFace *f=BgDocumentFindFace(&doc,&out[i].face,NULL);
            assert(f && f->drawgroup==before.rooms[1].faces[i].drawgroup && f->textureid==17 && f->cullbackfaces);
            assert(!memcmp(&f->material,&before.rooms[1].faces[i].material,sizeof(f->material)));
            const BgDocumentVertex *a=&doc.rooms[1].vertices[f->vertexindices[out[i].corner]];
            const BgDocumentVertex *b=&doc.rooms[1].vertices[f->vertexindices[(out[i].corner+1)%3]];
            assert(b->x-a->x==100 && a->t==(direction==0?992:-992) && b->t==a->t);
        }
        assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&tx,&why));
        RoundTrip(&doc,&source,argv[1]);
        assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why) && doc.facecount==2 && !doc.dirty);
        assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why) && doc.facecount==6 && doc.dirty);
        /* Continue directly from the returned outer edges. */
        assert(BgDocumentExtrudeEdges(&doc,out,2,offset,edges,&count,&why) && count==2 && doc.facecount==10);
        assert(doc.rooms[1].vertexcount==10); CheckCounts(&doc); RoundTrip(&doc,&source,argv[1]);
        EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
    }
    puts("PASS: snapped planar/folded extrusion at four level scales, inherited material, shared endpoints, preview agreement, repeated extrusion, undo/redo and native save/ROM validation.");
    BgFile source=Fixture(); BgDocument doc={0}, before={0};
    assert(BgDocumentLoad(source.data,source.size,.1f,&doc,&why)); MakeStrip(&doc);
    /* Give room 2 an independent secondary-layer source with a UV seam. */
    BgDocumentRoom *r=&doc.rooms[2];
    r->vertices=malloc(3*sizeof(*r->vertices)); assert(r->vertices);
    memcpy(r->vertices,doc.rooms[1].vertices,3*sizeof(*r->vertices)); r->vertexcount=3;
    r->faces=malloc(sizeof(*r->faces)); assert(r->faces);
    r->faces[0]=doc.rooms[1].faces[0]; r->faces[0].room=2; r->faces[0].id=doc.nextfaceid++;
    r->faces[0].layer=BG_GEOMETRY_SECONDARY; r->facecount=r->facecapacity=1; doc.facecount++;
    r->layers[1].groups=calloc(1,sizeof(*r->layers[1].groups)); assert(r->layers[1].groups);
    r->layers[1].groupcount=r->layers[1].groupcapacity=1; r->layers[1].sourcepresent=TRUE;
    for (int i=0; i<3; i++)
    { r->vertices[i].room=2; r->vertices[i].id=doc.nextvertexid++; r->vertices[i].usecount=1; }
    assert(BgDocumentClone(&doc,&before,&why));
    BgDocumentEdgeRef edges[2]={Edge(&doc.rooms[1].faces[0],0),Edge(&r->faces[0],0)}, out[2]; DWORD count;
    double offset[3]={0,-100,0};
    /* Every allocation failure must leave every room and ID counter untouched. */
    for (int budget=0; budget<6; budget++)
    {
        BgDocument original=doc; BgDocumentRoom rooms[3]; memcpy(rooms,doc.rooms,sizeof(rooms));
        allocations=budget; assert(!BgDocumentExtrudeEdges(&doc,edges,2,offset,out,&count,&why)); allocations=-1;
        assert(count==0 && !memcmp(&original,&doc,sizeof(doc)) && !memcmp(rooms,doc.rooms,sizeof(rooms)));
        VerifySource(&before,&doc);
    }
    const double invalid[][3]={{100,0,0},{0,1e10,0},{0,NAN,0},{0,-20000,0}};
    for (unsigned i=0; i<sizeof(invalid)/sizeof(*invalid); i++)
    { assert(!BgDocumentExtrudeEdges(&doc,edges,2,invalid[i],out,&count,&why) && !count); VerifySource(&before,&doc); }
    BgDocumentEdgeRef bad[2]={edges[0],edges[0]};
    assert(!BgDocumentExtrudeEdges(&doc,bad,2,offset,out,&count,&why));
    bad[1]=edges[1]; bad[1].corner=9;
    assert(!BgDocumentExtrudeEdges(&doc,bad,2,offset,out,&count,&why));
    bad[1]=edges[1]; bad[1].face.faceid=123456;
    assert(!BgDocumentExtrudeEdges(&doc,bad,2,offset,out,&count,&why));
    assert(BgDocumentExtrudeEdges(&doc,edges,2,(double[3]){0,.01,0},out,&count,&why) && !count && !doc.dirty);
    assert(BgDocumentExtrudeEdges(&doc,edges,2,offset,out,&count,&why) && count==2);
    assert(out[0].face.room==1 && out[1].face.room==2 && out[1].face.layer==1);
    CheckCounts(&doc); RoundTrip(&doc,&source,argv[1]);
    BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
    puts("PASS: multi-room/layer inheritance, coordinate and UV limits, degenerate/duplicate/stale edges, no-op drags and atomic allocation failures.");
    /* Unmapped UV fallback creates a seam without changing existing geometry. */
    source=Fixture(); assert(BgDocumentLoad(source.data,source.size,1,&doc,&why)); MakeStrip(&doc);
    for (DWORD i=0; i<doc.rooms[1].vertexcount; i++) { doc.rooms[1].vertices[i].s=doc.rooms[1].vertices[i].t=0; }
    assert(BgDocumentClone(&doc,&before,&why)); edges[0]=Edge(&doc.rooms[1].faces[0],0);
    assert(BgDocumentExtrudeEdges(&doc,edges,1,offset,out,&count,&why));
    VerifySource(&before,&doc); CheckCounts(&doc); RoundTrip(&doc,&source,argv[1]);
    const BgDocumentFace *face=BgDocumentFindFace(&doc,&out[0].face,NULL);
    const BgDocumentVertex *a=&doc.rooms[1].vertices[face->vertexindices[1]], *b=&doc.rooms[1].vertices[face->vertexindices[2]];
    assert(b->s-a->s==3200 && a->t==-3200 && b->t==-3200);
    BgDocumentFree(&doc); BgDocumentFree(&before); BgFileFree(&source);
    puts("PASS: unmapped-UV fallback leaves source UVs intact and produces a usable new surface.");
    return 0;
}
