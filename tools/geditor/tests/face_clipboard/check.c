#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static int allocations = -1;
static BOOL Fail(void) { if (!allocations) { return TRUE; } if (allocations > 0) { allocations--; } return FALSE; }
static void *TestMalloc(size_t n) { return Fail() ? NULL : malloc(n); }
static void *TestCalloc(size_t n, size_t s) { return Fail() ? NULL : calloc(n,s); }
static void *TestRealloc(void *p, size_t n) { return Fail() ? NULL : realloc(p,n); }
#define malloc TestMalloc
#define calloc TestCalloc
#define realloc TestRealloc
#include "bgdocument.c"
#include "bgroom.c"
#undef malloc
#undef calloc
#undef realloc
/* Opaque texture batching may reorder native faces (notably Depot room 50).
 * Compare all geometry/material output, retaining the order of secondary,
 * decal, alpha-tested and blended faces and the opaque runs between them. */
typedef struct SavedFace {
    DWORD room, surfacepolicy, surfacebasemode;
    unsigned short tag;
    BgRenderFlags flags;
    BgVertex vertices[3];
} SavedFace;
static int CompareSavedFaces(const void *a,const void *b) { return memcmp(a,b,sizeof(SavedFace)); }
static BOOL SavedFaceOpaque(const SavedFace *f)
{
    return !BG_TRI_IS_SECONDARY(f->tag)
        && (f->flags & (BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE|BG_RENDER_DECAL|BG_RENDER_BLEND|BG_RENDER_ALPHA_TEST))
            == (BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE);
}
static SavedFace *SavedFaces(const BgDocument *doc)
{
    BgDocumentRenderMesh mesh={0}; const char *why="";
    assert(BgDocumentBuildRenderMesh(doc,&mesh,&why));
    SavedFace *out=calloc(mesh.facecount,sizeof(*out)); assert(out);
    for(DWORD i=0;i<mesh.facecount;i++)
    {
        BgRenderState state;
        assert(BgDocumentGetFaceRenderStates(doc,&mesh.facerefs[i],1,&state));
        out[i].room=mesh.facerefs[i].room; out[i].tag=mesh.tags[i]; out[i].flags=mesh.renderflags[i];
        out[i].surfacepolicy=state.surfacepolicy; out[i].surfacebasemode=state.surfacebasemode;
        memcpy(out[i].vertices,mesh.vertices+i*3,sizeof(out[i].vertices));
    }
    for(DWORD start=0,end;start<mesh.facecount;start=end)
    {
        end=start+1;
        if(!SavedFaceOpaque(&out[start])) continue;
        while(end<mesh.facecount && out[end].room==out[start].room && SavedFaceOpaque(&out[end])) end++;
        qsort(out+start,end-start,sizeof(*out),CompareSavedFaces);
    }
    BgDocumentRenderMeshFree(&mesh);
    return out;
}
static void Equivalent(const BgDocument *a,const BgDocument *b)
{
    assert(a->facecount==b->facecount);
    SavedFace *x=SavedFaces(a), *y=SavedFaces(b);
    assert(!memcmp(x,y,a->facecount*sizeof(*x)));
    free(x);free(y);
}
#include "fixture.inc"
#include "common.inc"

static void Copies(const BgDocument *original, const BgFaceRef *refs,
    const BgDocument *doc, const BgFaceRef *copies, DWORD count, const int offset[3])
{
    for (DWORD i = 0; i < count; i++)
    {
        const BgDocumentRoom *a, *b;
        const BgDocumentFace *x = BgDocumentFindFace(original,&refs[i],&a);
        const BgDocumentFace *y = BgDocumentFindFace(doc,&copies[i],&b);
        assert(x && y && x->id != y->id && x->layer == y->layer && x->room == y->room);
        assert(x->textureid == y->textureid && x->cullbackfaces == y->cullbackfaces && BgMaterialEqual(&x->material,&y->material));
        for (int c = 0; c < 3; c++)
        {
            const BgDocumentVertex *v = &a->vertices[x->vertexindices[c]], *w = &b->vertices[y->vertexindices[c]];
            assert(v->id != w->id && y->vertexindices[c] >= a->vertexcount);
            assert(v->x + offset[0] == w->x && v->y + offset[1] == w->y && v->z + offset[2] == w->z);
            assert(v->s == w->s && v->t == w->t && v->flag == w->flag && !memcmp(&v->r,&w->r,4));
        }
        BgRenderState s,t;
        assert(BgDocumentGetFaceRenderStates(original,&refs[i],1,&s) && BgDocumentGetFaceRenderStates(doc,&copies[i],1,&t));
        assert(s.othermode == t.othermode && s.othermodehigh == t.othermodehigh
            && s.geometrymode == t.geometrymode && s.primitivealpha == t.primitivealpha
            && s.environmentalpha == t.environmentalpha && s.surfacepolicy == t.surfacepolicy
            && s.surfacebasemode == t.surfacebasemode);
    }
}

static void Geometry(const char *dir)
{
    BgFile source = Fixture(); BgDocument original = {0}, doc = {0}, clip = {0}, clipbefore = {0};
    BgFaceRef refs[20], *pasted = NULL; DWORD count; const char *why = "";
    const double offset[3] = {0,10,0}; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,0.5f,&original,&why)); Refs(&original,refs);
    for (DWORD r = 1; r <= original.roomcount; r++) for (DWORD v = 0; v < original.rooms[r].vertexcount; v++)
    {
        BgDocumentVertex *p = &original.rooms[r].vertices[v];
        p->s = (short)(v*231-200); p->t = (short)(v*152-73); p->flag = 37;
        p->r = (unsigned char)(v*80); p->g = (unsigned char)(v*35); p->a = (unsigned char)(120+v);
    }
    BgFacePropertiesEdit edit = {.fields=BG_FACE_PROPERTY_TRANSPARENCY,.transparency=BG_TRANSPARENCY_CUTOUT};
    assert(BgDocumentSetFaceProperties(&original,&refs[1],1,&edit,&changed,&why));
    assert(BgDocumentSetFaceTexture(&original,&refs[4],1,BG_TEX_NONE,&changed,&why));
    assert(BgDocumentClone(&original,&doc,&why));
    BgFaceRef selection[] = {refs[1],refs[3],refs[4],refs[6],refs[11],refs[1]};
    assert(BgDocumentCopyFaces(&doc,selection,6,&clip,&why) && clip.facecount == 5);
    Same(&doc,&original); UseCounts(&clip);
    assert(BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why) && count == 5);
    Copies(&original,selection,&doc,pasted,count,(int[]){0,5,0}); UseCounts(&doc);
    assert(doc.rooms[1].vertexcount == original.rooms[1].vertexcount + 3); /* Sharing within the copied set. */
    for (DWORD r = 1; r <= original.roomcount; r++)
    { assert(!memcmp(doc.rooms[r].vertices,original.rooms[r].vertices,original.rooms[r].vertexcount*sizeof(BgDocumentVertex))); }
    RoundTrip(&doc,&source,dir);
    free(pasted); BgDocumentFree(&doc);
    /* Clipboard is a snapshot: deletion, UV/color/material edits and reordering do not change it. */
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentSetFaceTexture(&doc,selection,1,99,&changed,&why));
    edit.transparency = BG_TRANSPARENCY_BLEND;
    assert(BgDocumentSetFaceProperties(&doc,selection,1,&edit,&changed,&why));
    DWORD deleted; assert(BgDocumentDeleteFaces(&doc,selection,5,&deleted,&why) && deleted == 5);
    assert(BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why));
    Copies(&original,selection,&doc,pasted,count,(int[]){0,5,0}); RoundTrip(&doc,&source,dir);
    free(pasted);
    assert(BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why));
    Copies(&original,selection,&doc,pasted,count,(int[]){0,5,0}); UseCounts(&doc); free(pasted); BgDocumentFree(&doc);
    /* Every allocation failure is atomic, including a partly prepared later room. */
    assert(BgDocumentClone(&clip,&clipbefore,&why));
    BOOL succeeded = FALSE;
    for (int budget = 0; budget < 300 && !succeeded; budget++)
    {
        allocations = budget;
        BOOL ok = BgDocumentCopyFaces(&original,refs,20,&clip,&why);
        allocations = -1;
        if (!ok) { assert(why[0]); Same(&clip,&clipbefore); }
        else { succeeded = TRUE; }
    }
    assert(succeeded);
    succeeded = FALSE;
    for (int budget = 0; budget < 350 && !succeeded; budget++)
    {
        assert(BgDocumentClone(&original,&doc,&why)); allocations = budget;
        BOOL ok = BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why);
        allocations = -1;
        if (!ok) { assert(why[0] && !pasted && !count); Same(&doc,&original); }
        else { succeeded = TRUE; UseCounts(&doc); free(pasted); }
        BgDocumentFree(&doc);
    }
    assert(succeeded);
    assert(BgDocumentClone(&original,&doc,&why));
    clip.rooms[2].vertices[0].y = 32767;
    assert(!BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why)); Same(&doc,&original);
    clip.rooms[2].vertices[0].y = original.rooms[2].vertices[0].y;
    doc.nextfaceid = (DWORD)-1;
    assert(!BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why));
    doc.nextfaceid = original.nextfaceid; Same(&doc,&original);
    BgFaceRef bad = {.room=1,.faceid=0};
    assert(!BgDocumentCopyFaces(&doc,&bad,1,&clip,&why));
    BgDocumentFree(&doc); BgDocumentFree(&clip); BgDocumentFree(&clipbefore); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: snapshot copy, fresh identities, shared pasted vertices, +10 world-unit offset, colors/UVs/materials/rooms/layers, repeated paste, save/reload and atomic failures.");
}

static void Native(const char *path, const char *dir)
{
    BgFile source = {0}; BgDocument original = {0}; const char *why = "";
    FILE *fp = fopen(path,"rb"); assert(fp); fseek(fp,0,SEEK_END); source.size = (DWORD)ftell(fp); rewind(fp);
    source.data = malloc(source.size); assert(source.data && fread(source.data,1,source.size,fp) == source.size); fclose(fp);
    strcpy(source.name,"bg/clipboard_native.seg");
    assert(BgDocumentLoad(source.data,source.size,0.2333333333f,&original,&why));
    const double offset[3] = {0,10,0}; DWORD checked = 0;
    for (DWORD r = 1; r <= original.roomcount; r++)
    {
        const BgDocumentRoom *room = &original.rooms[r];
        if (!room->facecount) { continue; }
        BgDocument clip = {0}, doc = {0}; BgFaceRef *refs = malloc(room->facecount*sizeof(*refs)), *pasted;
        DWORD count;
        assert(refs && BgDocumentClone(&original,&doc,&why));
        for (DWORD f = 0; f < room->facecount; f++)
        { const BgDocumentFace *p = &room->faces[f]; refs[f] = (BgFaceRef){p->id,p->room,p->layer,0}; }
        assert(BgDocumentCopyFaces(&doc,refs,room->facecount,&clip,&why));
        if (!BgDocumentPasteFaces(&doc,&clip,offset,&pasted,&count,&why))
        { fprintf(stderr,"%s room %lu: %s\n",path,(unsigned long)r,why); abort(); }
        assert(count == room->facecount); Copies(&original,refs,&doc,pasted,count,(int[]){0,2,0});
        if (r == 1) { RoundTrip(&doc,&source,dir); }
        checked += count; free(refs); free(pasted); BgDocumentFree(&clip); BgDocumentFree(&doc);
    }
    printf("PASS: native %s, %lu faces copied across all occupied rooms.\n",path,(unsigned long)checked);
    BgDocumentFree(&original); BgFileFree(&source);
}

/* Run actual editor commands with presentation calls stubbed. */
typedef void *HWND;
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 16
static HWND g_Viewport = (HWND)2;
static BgDocument g_CurrentBgDocument, g_FaceClipboard;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgFaceRef selection[32];
static int selectedcount,errors,restores;
static BOOL failrebuild,failselection,flying,transforming;
static EditorTool tool = EDITOR_TOOL_FACE_SELECT;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectedcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *out,int count)
{ assert(count == selectedcount); memcpy(out,selection,count*sizeof(*out)); return TRUE; }
static BOOL ViewportSelectBgFaces(HWND hwnd,const BgFaceRef *refs,DWORD count)
{ if (failselection) { return FALSE; } assert(count <= 32); memcpy(selection,refs,count*sizeof(*refs)); selectedcount = count; return TRUE; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if (failrebuild) { failrebuild=FALSE; *why="test rebuild failure"; return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]); errors++; }
#include "editor.inc"
static void Commands(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, pasted = {0}; BgFaceRef refs[20]; const char *why = "";
    assert(BgDocumentLoad(source.data,source.size,1,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why)); Refs(&original,refs);
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(!GEditorPasteBgFaces((HWND)1));
    selection[0] = refs[1]; selection[1] = refs[6]; selectedcount = 2;
    assert(GEditorCopySelectedBgFaces((HWND)1) && !g_EditHistory.undocount); Same(&g_CurrentBgDocument,&original);
    assert(GEditorPasteBgFaces((HWND)1) && g_EditHistory.undocount == 1);
    assert(selectedcount == 2 && selection[0].faceid != refs[1].faceid);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Paste Faces"));
    assert(BgDocumentClone(&g_CurrentBgDocument,&pasted,&why));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&original);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&pasted);
    failrebuild = TRUE; assert(!GEditorPasteBgFaces((HWND)1)); Same(&g_CurrentBgDocument,&pasted);
    failselection = TRUE; assert(!GEditorPasteBgFaces((HWND)1)); failselection = FALSE; Same(&g_CurrentBgDocument,&pasted);
    ULONGLONG revision = g_EditHistory.nextrevision; g_EditHistory.nextrevision = 0;
    assert(!GEditorPasteBgFaces((HWND)1)); g_EditHistory.nextrevision = revision; Same(&g_CurrentBgDocument,&pasted);
    assert(errors == 3 && restores == 3 && g_EditHistory.undocount == 1);
    flying = TRUE; assert(!GEditorPasteBgFaces((HWND)1)); flying = FALSE;
    transforming = TRUE; assert(!GEditorCopySelectedBgFaces((HWND)1)); transforming = FALSE;
    selectedcount = 0; assert(!GEditorCopySelectedBgFaces((HWND)1) && GEditorPasteBgFaces((HWND)1));
    tool = EDITOR_TOOL_VERTEX_SELECT; assert(!GEditorPasteBgFaces((HWND)1));
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&g_FaceClipboard);
    BgDocumentFree(&original); BgDocumentFree(&pasted); BgFileFree(&source);
    puts("PASS: actual copy/paste commands, no copy history, pasted selection, undo/redo, empty clipboard, edit gates and failed paste rollback.");
}
static void DuplicateCommands(const char *dir)
{
    BgFile source = Fixture(); BgDocument original = {0}, copied = {0}, clipboard = {0};
    BgFaceRef refs[20], seeds[3]; const char *why = "";
    assert(BgDocumentLoad(source.data,source.size,.5f,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why)); Refs(&original,refs);
    tool = EDITOR_TOOL_FACE_SELECT;
    selection[0] = refs[9]; selection[1] = refs[19]; selectedcount = 2;
    assert(GEditorCopySelectedBgFaces((HWND)1));
    assert(BgDocumentClone(&g_FaceClipboard,&clipboard,&why));
    /* Different rooms/layers, one face and multiple faces, both directions. */
    seeds[0]=refs[1]; seeds[1]=refs[6]; seeds[2]=refs[11];
    for (int axis=0;axis<3;axis++)
    {
        double offset[3]={0}; int native[3]={0};
        offset[axis]=axis==1 ? -26 : 26; native[axis]=(int)(offset[axis]*.5);
        selectedcount=axis+1; memcpy(selection,seeds,selectedcount*sizeof(*seeds));
        EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
        assert(GEditorDuplicateBgFaces((HWND)1,offset));
        assert(g_EditHistory.undocount==1 && selectedcount==axis+1);
        assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory),axis ? "Duplicate Faces" : "Duplicate Face"));
        Copies(&original,seeds,&g_CurrentBgDocument,selection,selectedcount,native);
        UseCounts(&g_CurrentBgDocument); Same(&g_FaceClipboard,&clipboard);
        for (DWORD r=1;r<=original.roomcount;r++)
        {
            const BgDocumentRoom *a=&original.rooms[r], *b=&g_CurrentBgDocument.rooms[r];
            assert(!memcmp(a->vertices,b->vertices,a->vertexcount*sizeof(*a->vertices)));
            for (DWORD f=0;f<a->facecount;f++)
            {
                const BgDocumentFace *face=&a->faces[f];
                BgFaceRef ref={face->id,face->room,face->layer,0};
                const BgDocumentFace *after=BgDocumentFindFace(&g_CurrentBgDocument,&ref,NULL);
                assert(after && !memcmp(face->vertexindices,after->vertexindices,sizeof(face->vertexindices)));
            }
        }
        RoundTrip(&g_CurrentBgDocument,&source,dir);
        assert(BgDocumentClone(&g_CurrentBgDocument,&copied,&why));
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        Same(&g_CurrentBgDocument,&original);
        assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        Same(&g_CurrentBgDocument,&copied);
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        BgDocumentFree(&copied);
    }
    memcpy(selection,seeds,sizeof(seeds)); selectedcount=3;
    const double offset[3]={0,20,0};
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    errors=restores=0;
    assert(!GEditorDuplicateBgFaces((HWND)1,(double[]){0,0,0}));
    failrebuild=TRUE; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); Same(&g_CurrentBgDocument,&original);
    failselection=TRUE; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); failselection=FALSE; Same(&g_CurrentBgDocument,&original);
    ULONGLONG revision=g_EditHistory.nextrevision; g_EditHistory.nextrevision=0;
    assert(!GEditorDuplicateBgFaces((HWND)1,offset)); g_EditHistory.nextrevision=revision; Same(&g_CurrentBgDocument,&original);
    assert(errors==3 && restores==3 && !g_EditHistory.undocount);
    allocations=0; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); allocations=-1;
    assert(errors==4 && !g_EditHistory.undocount); Same(&g_CurrentBgDocument,&original);
    assert(!GEditorDuplicateBgFaces((HWND)1,(double[]){1e8,0,0})); Same(&g_CurrentBgDocument,&original);
    selectedcount=0; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); selectedcount=3;
    flying=TRUE; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); flying=FALSE;
    transforming=TRUE; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); transforming=FALSE;
    tool=EDITOR_TOOL_EDGE_SELECT; assert(!GEditorDuplicateBgFaces((HWND)1,offset)); tool=EDITOR_TOOL_FACE_SELECT;
    Same(&g_FaceClipboard,&clipboard);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&g_FaceClipboard);
    BgDocumentFree(&original); BgDocumentFree(&clipboard); BgFileFree(&source);
    puts("PASS: face drag duplication on XYZ, single/multiple selections, preserved source/clipboard/materials, native save/reload, one undo/redo step, no-op and failure rollback.");
}
int main(int argc,char **argv)
{ setvbuf(stdout,NULL,_IONBF,0); assert(argc == 4); Geometry(argv[1]); Commands(); DuplicateCommands(argv[1]); Native(argv[2],argv[1]); Native(argv[3],argv[1]); return 0; }
