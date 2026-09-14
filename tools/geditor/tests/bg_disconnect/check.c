#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations=-1;
static BOOL FailAllocation(void)
{ if (!allocations) { return TRUE; } if (allocations>0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return FailAllocation() ? NULL : malloc(n); }
static void *TestCalloc(size_t n,size_t s) { return FailAllocation() ? NULL : calloc(n,s); }
#define malloc TestMalloc
#define calloc TestCalloc
#include "bgdisconnect.c"
#undef malloc
#undef calloc
#include "fixture.inc"

static void Same(const BgDocument *a,const BgDocument *b)
{
    assert(a->facecount==b->facecount && a->nextvertexid==b->nextvertexid && a->nextfaceid==b->nextfaceid);
    assert(a->dirty==b->dirty && a->roomcount==b->roomcount);
    for (DWORD r=1;r<=a->roomcount;r++)
    {
        const BgDocumentRoom *x=&a->rooms[r], *y=&b->rooms[r];
        assert(x->facecount==y->facecount && x->vertexcount==y->vertexcount);
        assert(!memcmp(x->faces,y->faces,x->facecount*sizeof(*x->faces)));
        assert(!memcmp(x->vertices,y->vertices,x->vertexcount*sizeof(*x->vertices)));
    }
}

static void UseCounts(const BgDocument *doc)
{
    for (DWORD r=1;r<=doc->roomcount;r++)
    {
        const BgDocumentRoom *room=&doc->rooms[r];
        for (DWORD v=0;v<room->vertexcount;v++)
        {
            DWORD uses=0;
            for (DWORD f=0;f<room->facecount;f++) for (int c=0;c<3;c++)
            { uses+=room->faces[f].vertexindices[c]==v; }
            assert(uses==room->vertices[v].usecount && room->vertices[v].room==r && room->vertices[v].id);
        }
    }
}

static void PrivateCorners(const BgDocument *doc,const BgFaceRef *ref,unsigned mask)
{
    const BgDocumentRoom *room;
    const BgDocumentFace *face=BgDocumentFindFace(doc,ref,&room); assert(face);
    for (int c=0;c<3;c++) if (mask&(1u<<c))
    {
        DWORD v=face->vertexindices[c];
        for (DWORD f=0;f<room->facecount;f++) if (&room->faces[f]!=face)
        { for (int k=0;k<3;k++) { assert(room->faces[f].vertexindices[k]!=v); } }
    }
}

/* Check independence even before attributes diverge: saving must not weld
   identical copies together. Repeated compilation may repack native batches. */
static void RoundTrip(const BgDocument *doc,const BgFile *source,const char *dir,
    const BgFaceRef *private,DWORD count,unsigned mask)
{
    BgFile compiled={0},saved={0}; BgDocument loaded={0}; const char *why="";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    assert(saved.size==compiled.size && !memcmp(saved.data,compiled.data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    Equivalent(doc,&loaded); UseCounts(&loaded);
    /* Face IDs are reassigned on load, but the native face order remains. */
    for (DWORD i=0;i<count;i++)
    {
        const BgDocumentRoom *room;
        const BgDocumentFace *face=BgDocumentFindFace(doc,&private[i],&room);
        DWORD index=(DWORD)(face-room->faces);
        const BgDocumentFace *f=&loaded.rooms[private[i].room].faces[index];
        BgFaceRef ref={f->id,f->room,f->layer,0}; PrivateCorners(&loaded,&ref,mask);
    }
    BgDocumentFree(&loaded); BgFileFree(&saved); BgFileFree(&compiled);
}

static void Geometry(const char *dir)
{
    BgFile source=Fixture(); BgDocument original={0},doc={0},before={0};
    BgFaceRef refs[20],selected[4]; const char *why=""; DWORD added;
    assert(BgDocumentLoad(source.data,source.size,1,&original,&why));
    assert(Refs(&original,refs)==20);
    for (DWORD r=1;r<=original.roomcount;r++) for (DWORD v=0;v<original.rooms[r].vertexcount;v++)
    {
        BgDocumentVertex *p=&original.rooms[r].vertices[v];
        p->s=(short)(v*96-64); p->t=(short)(128-v*32); p->r=(unsigned char)(v*60); p->a=(unsigned char)(255-v*20);
    }
    assert(BgDocumentClone(&original,&doc,&why));
    selected[0]=refs[1];selected[1]=refs[2];selected[2]=refs[6];selected[3]=refs[11];
    assert(BgDocumentDisconnectFaces(&doc,selected,4,&added,&why) && added==12);
    Equivalent(&original,&doc); UseCounts(&doc);
    for (DWORD i=0;i<4;i++) { PrivateCorners(&doc,&selected[i],7); }
    RoundTrip(&doc,&source,dir,selected,4,7);
    DWORD next=doc.nextvertexid;
    assert(BgDocumentDisconnectFaces(&doc,selected,4,&added,&why) && !added && doc.nextvertexid==next);
    /* UV edits and paint now affect exactly one face. */
    assert(BgDocumentClone(&doc,&before,&why));
    const BgDocumentFace *face=BgDocumentFindFace(&doc,&selected[0],NULL);
    DWORD vertex=face->vertexindices[0];
    BgDocumentUVEdit uv={.vertex={selected[0].room,vertex},.vertexid=doc.rooms[selected[0].room].vertices[vertex].id,.s=1234,.t=-456};
    assert(BgDocumentSetVertexUVs(&doc,&uv,1,&added,&why) && added==1);
    BOOL changed; unsigned char rgba[]={17,33,99,127};
    assert(BgDocumentPaintVertex(&doc,&selected[0],0,rgba,&changed,&why) && changed);
    for (DWORD r=1;r<=doc.roomcount;r++) for (DWORD v=0;v<doc.rooms[r].vertexcount;v++)
    {
        const BgDocumentVertex *p=&doc.rooms[r].vertices[v];
        if (r==selected[0].room && v==vertex) { assert(p->s==1234 && p->t==-456 && !memcmp(&p->r,rgba,4)); }
        else { assert(!memcmp(p,&before.rooms[r].vertices[v],sizeof(*p))); }
    }
    RoundTrip(&doc,&source,dir,selected,4,7); BgDocumentFree(&before); BgDocumentFree(&doc);

    assert(BgDocumentClone(&original,&doc,&why));
    BgDocumentEdgeRef edge={refs[0],0};
    DWORD third=BgDocumentFindFace(&doc,&edge.face,NULL)->vertexindices[2];
    /* All ten faces in room 1 share this edge, in primary and secondary layers. */
    assert(BgDocumentSplitEdge(&doc,&edge,&added,&why) && added==18);
    Equivalent(&original,&doc); UseCounts(&doc);
    for (DWORD i=0;i<10;i++)
    {
        PrivateCorners(&doc,&refs[i],3);
        assert(BgDocumentFindFace(&doc,&refs[i],NULL)->vertexindices[2]==third);
    }
    RoundTrip(&doc,&source,dir,refs,10,3);
    assert(BgDocumentSplitEdge(&doc,&edge,&added,&why) && !added);
    /* No incident faces or same-position vertices in another room were edited. */
    assert(!memcmp(doc.rooms[2].vertices,original.rooms[2].vertices,original.rooms[2].vertexcount*sizeof(BgDocumentVertex)));
    BgDocumentFree(&doc);

    /* Exhaust each staged allocation, including after an earlier room was processed. */
    for (int operation=0;operation<2;operation++)
    {
        BOOL succeeded=FALSE;
        for (int budget=0;budget<20 && !succeeded;budget++)
        {
            assert(BgDocumentClone(&original,&doc,&why)); allocations=budget;
            BOOL ok=operation ? BgDocumentSplitEdge(&doc,&edge,&added,&why)
                : BgDocumentDisconnectFaces(&doc,selected,4,&added,&why);
            allocations=-1;
            if (!ok) { assert(why[0] && !added); Same(&doc,&original); }
            else { succeeded=TRUE; assert(added); UseCounts(&doc); }
            BgDocumentFree(&doc);
        }
        assert(succeeded);
    }
    assert(BgDocumentClone(&original,&doc,&why));
    BgFaceRef bad[]={refs[0],refs[0]};
    assert(!BgDocumentDisconnectFaces(&doc,bad,2,&added,&why)); Same(&doc,&original);
    bad[1]=refs[11];bad[1].faceid=0;
    assert(!BgDocumentDisconnectFaces(&doc,bad,2,&added,&why)); Same(&doc,&original);
    assert(!BgDocumentDisconnectFaces(&doc,refs,0,&added,NULL));
    assert(!BgDocumentSplitEdge(&doc,NULL,&added,NULL));
    edge.corner=3; assert(!BgDocumentSplitEdge(&doc,&edge,&added,&why)); Same(&doc,&original);edge.corner=0;
    doc.nextvertexid=UINT32_MAX;
    assert(!BgDocumentDisconnectFaces(&doc,selected,4,&added,&why) && !added);
    doc.nextvertexid=original.nextvertexid; Same(&doc,&original);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: independent corners, UV/paint isolation, source identity, native round trips, no-ops, non-manifold edges and atomic failures.");
}

/* Run the real editor transaction command with only presentation calls stubbed. */
typedef void *HWND;
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 16
static HWND g_Viewport=(HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgFaceRef selection[4];
static int selectedcount=1,errors,rebuilds,restores,edgeselections;
static BOOL failrebuild,failselection,flying,transforming;
static EditorTool tool=EDITOR_TOOL_FACE_SELECT;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectedcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *out,int count)
{ assert(count==selectedcount);memcpy(out,selection,count*sizeof(*out));return TRUE; }
static BOOL ViewportSelectBgEdges(HWND hwnd,const BgDocumentEdgeRef *edge,DWORD count)
{ assert(count==1 && BgDocumentFindFace(&g_CurrentBgDocument,&edge->face,NULL));edgeselections++;return !failselection; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ rebuilds++; if(failrebuild) { failrebuild=FALSE;*why="test rebuild failure";return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]);errors++; }
#include "editor.inc"

static void Commands(void)
{
    BgFile source=Fixture(); BgDocument original={0},separated={0}; BgFaceRef refs[20];const char *why="";
    assert(BgDocumentLoad(source.data,source.size,1,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why)); Refs(&original,refs);
    selection[0]=refs[1];selection[1]=refs[6];selectedcount=2;
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(GEditorSeparateBgVertices((HWND)1,NULL));
    assert(g_EditHistory.undocount==1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Disconnect Faces"));
    assert(BgDocumentClone(&g_CurrentBgDocument,&separated,&why));
    assert(GEditorSeparateBgVertices((HWND)1,NULL) && g_EditHistory.undocount==1 && rebuilds==1);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&g_CurrentBgDocument,&original);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&g_CurrentBgDocument,&separated);
    selection[0]=refs[2]; selectedcount=1;
    failrebuild=TRUE; assert(!GEditorSeparateBgVertices((HWND)1,NULL));Same(&g_CurrentBgDocument,&separated);
    ULONGLONG revision=g_EditHistory.nextrevision;g_EditHistory.nextrevision=0;
    assert(!GEditorSeparateBgVertices((HWND)1,NULL));g_EditHistory.nextrevision=revision;Same(&g_CurrentBgDocument,&separated);
    assert(errors==2 && restores==2 && g_EditHistory.undocount==1);
    tool=EDITOR_TOOL_EDGE_SELECT; BgDocumentEdgeRef edge={refs[2],0};
    failselection=TRUE;assert(!GEditorSeparateBgVertices((HWND)1,&edge));failselection=FALSE;Same(&g_CurrentBgDocument,&separated);
    assert(GEditorSeparateBgVertices((HWND)1,&edge));
    assert(edgeselections==2 && g_EditHistory.undocount==2 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Split Edge"));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));Same(&g_CurrentBgDocument,&separated);
    flying=TRUE;assert(!GEditorSeparateBgVertices((HWND)1,&edge));flying=FALSE;
    transforming=TRUE;assert(!GEditorSeparateBgVertices((HWND)1,&edge));transforming=FALSE;
    tool=EDITOR_TOOL_VERTEX_SELECT;assert(!GEditorSeparateBgVertices((HWND)1,NULL));
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgDocumentFree(&original);BgDocumentFree(&separated);BgFileFree(&source);
    puts("PASS: editor commands, undo/redo, no-op history, rebuild/selection/commit rollback and edit guards.");
}
int main(int argc,char **argv) { assert(argc==2);Geometry(argv[1]);Commands();return 0; }
