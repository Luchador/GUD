#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
#include "uvprojection.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations=-1;
static BOOL Fail(void) { if (!allocations) { return TRUE; } if (allocations>0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return Fail() ? NULL : malloc(n); }
static void *TestCalloc(size_t n,size_t s) { return Fail() ? NULL : calloc(n,s); }
static void *TestRealloc(void *p,size_t n) { return Fail() ? NULL : realloc(p,n); }
#define malloc TestMalloc
#define calloc TestCalloc
#define realloc TestRealloc
#include "bgdocument.c"
#include "bguv.c"
#undef malloc
#undef calloc
#undef realloc
#include "fixture.inc"
#include "common.inc"

static BgDocumentFaceUVEdit Edit(const BgDocument *doc,BgFaceRef ref,int s,int t)
{
    const BgDocumentRoom *room; const BgDocumentFace *face=BgDocumentFindFace(doc,&ref,&room);
    BgDocumentFaceUVEdit edit={.face=ref}; assert(face);
    for (int c=0;c<3;c++)
    { edit.vertexids[c]=room->vertices[face->vertexindices[c]].id; edit.s[c]=s; edit.t[c]=t; }
    return edit;
}
static void CheckUVs(const BgDocument *doc,const BgDocumentFaceUVEdit *edits,DWORD count)
{
    for (DWORD i=0;i<count;i++)
    {
        const BgDocumentRoom *room; const BgDocumentFace *face=BgDocumentFindFace(doc,&edits[i].face,&room); assert(face);
        for (int c=0;c<3;c++)
        { const BgDocumentVertex *v=&room->vertices[face->vertexindices[c]]; assert(v->s==edits[i].s[c] && v->t==edits[i].t[c]); }
    }
    UseCounts(doc);
}
static void Geometry(const char *dir)
{
    BgFile source=Fixture(); BgDocument original={0},doc={0}; BgFaceRef refs[20]; const char *why=""; DWORD changed;
    assert(BgDocumentLoad(source.data,source.size,1,&original,&why)); Refs(&original,refs);
    for (DWORD r=1;r<=original.roomcount;r++) for (DWORD i=0;i<original.rooms[r].vertexcount;i++)
    { BgDocumentVertex *v=&original.rooms[r].vertices[i]; v->flag=71; v->r=i*70; v->g=i*19; v->b=i*37; v->a=155+i; }
    BgDocumentFaceUVEdit edits[]={Edit(&original,refs[0],128,-256),Edit(&original,refs[1],128,-256),
        Edit(&original,refs[6],-32768,32767),Edit(&original,refs[11],500,600)};
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why) && changed==12);
    CheckUVs(&doc,edits,4);
    /* Two equal-UV faces share a new copy; another UV variant gets its own.
       Unselected primary and secondary faces retain the original vertices. */
    assert(doc.rooms[1].vertexcount==original.rooms[1].vertexcount+6);
    assert(doc.rooms[2].vertexcount==original.rooms[2].vertexcount+3);
    for (DWORD i=0;i<20;i++)
    {
        const BgDocumentRoom *ar,*br; const BgDocumentFace *a=BgDocumentFindFace(&original,&refs[i],&ar),*b=BgDocumentFindFace(&doc,&refs[i],&br);
        assert(a->id==b->id && a->drawgroup==b->drawgroup && BgMaterialEqual(&a->material,&b->material));
        for (int c=0;c<3;c++)
        {
            const BgDocumentVertex *x=&ar->vertices[a->vertexindices[c]],*y=&br->vertices[b->vertexindices[c]];
            assert(x->x==y->x && x->y==y->y && x->z==y->z && x->flag==y->flag && !memcmp(&x->r,&y->r,4));
            if (i!=0 && i!=1 && i!=6 && i!=11) { assert(x->id==y->id && x->s==y->s && x->t==y->t); }
        }
    }
    RoundTrip(&doc,&source,dir);
    /* Refresh source identities just as rebuilding the UV canvas does. */
    for (int i=0;i<4;i++) { edits[i]=Edit(&doc,edits[i].face,edits[i].s[0],edits[i].t[0]); }
    DWORD vertices=doc.nextvertexid;
    assert(BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why) && !changed && vertices==doc.nextvertexid);
    BgDocumentFree(&doc);
    for (int i=0;i<4;i++) { edits[i]=Edit(&original,edits[i].face,edits[i].s[0],edits[i].t[0]); }
    BOOL succeeded=FALSE;
    for (int budget=0;budget<300 && !succeeded;budget++)
    {
        assert(BgDocumentClone(&original,&doc,&why)); allocations=budget;
        BOOL ok=BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why); allocations=-1;
        if (!ok) { assert(why[0] && !changed); Same(&doc,&original); }
        else { succeeded=TRUE; CheckUVs(&doc,edits,4); }
        BgDocumentFree(&doc);
    }
    assert(succeeded && BgDocumentClone(&original,&doc,&why));
    edits[3].t[2]=32768; assert(!BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why)); Same(&doc,&original);
    edits[3].t[2]=600; edits[3].vertexids[2]++; assert(!BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why)); Same(&doc,&original);
    edits[1]=edits[0]; assert(!BgDocumentSetFaceUVs(&doc,edits,4,&changed,&why)); Same(&doc,&original);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: UV variants share seam vertices, unselected faces and colors stay intact, native save/reload, no-op remapping and atomic validation/allocation failures.");
}

/* Actual editor transaction with only presentation calls stubbed. */
typedef void *HWND;
typedef struct { const BgDocumentFaceUVEdit *faces; DWORD count; const char *action; } UVCanvasFaceEdit;
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 16
static HWND g_Viewport=(HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL failrebuild;
static int errors;
static EditorTool ViewportGetTool(HWND hwnd) { return EDITOR_TOOL_FACE_SELECT; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if (failrebuild) { failrebuild=FALSE; *why="test rebuild failure"; return FALSE; } return TRUE; }
static void UVEditorRefreshSelection(HWND hwnd,const BgDocument *doc) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]); errors++; }
#include "editor.inc"
static void Cylinder(const char *dir)
{
    BgFile source=Fixture(); const char *why=""; BgFaceRef refs[BG_PRIMITIVE_MAX_FACES]; DWORD count;
    assert(BgDocumentLoad(source.data,source.size,1,&g_CurrentBgDocument,&why));
    assert(BgDocumentAddRoundPrimitive(&g_CurrentBgDocument,TRUE,1,(double[]){1000,1000,1000},100,350,12,refs,&count,&why));
    /* Reload the fixture so primary/secondary faces start in native order,
       just like opening a level that already contains the cylinder. */
    BgFile compiled={0};
    assert(BgDocumentCompile(&g_CurrentBgDocument,&source,&compiled,&why));
    BgDocumentFree(&g_CurrentBgDocument); BgFileFree(&source); source=compiled;
    assert(BgDocumentLoad(source.data,source.size,1,&g_CurrentBgDocument,&why));
    count=0;
    for (DWORD i=0;i<g_CurrentBgDocument.rooms[1].facecount;i++)
    {
        const BgDocumentFace *face=&g_CurrentBgDocument.rooms[1].faces[i];
        if (face->textureid==BG_TEX_NONE) { refs[count++]=(BgFaceRef){.room=1,.faceid=face->id,.layer=face->layer}; }
    }
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    BgDocument before={0},after={0}; assert(BgDocumentClone(&g_CurrentBgDocument,&before,&why));
    UVProjectionVertex vertices[72]={0}; UVProjectionFace faces[24]; double uv[24][3][2]; BgDocumentFaceUVEdit edits[24];
    assert(count==24);
    for (DWORD i=0;i<count;i++)
    {
        const BgDocumentRoom *room; const BgDocumentFace *face=BgDocumentFindFace(&before,&refs[i],&room);
        for (int c=0;c<3;c++)
        {
            const BgDocumentVertex *v=&room->vertices[face->vertexindices[c]]; int n=i*3+c;
            vertices[n].position[0]=v->x; vertices[n].position[1]=v->y; vertices[n].position[2]=v->z;
            faces[i].vertices[c]=n;
        }
    }
    assert(UVProjectionCylinder(vertices,72,faces,24,uv,&why));
    for (DWORD i=0;i<count;i++)
    {
        edits[i]=Edit(&before,refs[i],0,0);
        for (int c=0;c<3;c++) { edits[i].s[c]=(int)round(uv[i][c][0]*32*128); edits[i].t[c]=(int)round(uv[i][c][1]*32*32); }
    }
    UVCanvasFaceEdit request={edits,count,"Cylindrical UV Mapping"};
    failrebuild=TRUE; assert(!GEditorApplyUVFaceEdit((HWND)1,&request)); Same(&g_CurrentBgDocument,&before);
    ULONGLONG revision=g_EditHistory.nextrevision; g_EditHistory.nextrevision=0;
    assert(!GEditorApplyUVFaceEdit((HWND)1,&request)); g_EditHistory.nextrevision=revision; Same(&g_CurrentBgDocument,&before);
    assert(GEditorApplyUVFaceEdit((HWND)1,&request)); CheckUVs(&g_CurrentBgDocument,edits,count);
    assert(g_CurrentBgDocument.rooms[1].vertexcount==before.rooms[1].vertexcount+2); /* Only the seam column splits. */
    assert(g_EditHistory.undocount==1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),request.action));
    assert(BgDocumentClone(&g_CurrentBgDocument,&after,&why)); RoundTrip(&after,&source,dir);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&before);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&after);
    for (DWORD i=0;i<count;i++)
    {
        BgDocumentFaceUVEdit current=Edit(&after,refs[i],0,0);
        memcpy(edits[i].vertexids,current.vertexids,sizeof(current.vertexids));
    }
    assert(GEditorApplyUVFaceEdit((HWND)1,&request) && g_EditHistory.undocount==1); Same(&g_CurrentBgDocument,&after);
    assert(errors==2);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&before); BgDocumentFree(&after); BgFileFree(&source);
    puts("PASS: cylinder mapping through the real editor transaction, two seam vertices, undo/redo, repeated mapping, save/reload and rebuild/history failure rollback.");
}
int main(int argc,char **argv) { setbuf(stdout,NULL); assert(argc==2); Geometry(argv[1]); Cylinder(argv[1]); }
