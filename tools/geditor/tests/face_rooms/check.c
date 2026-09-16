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
#include "fixture.inc"
#include "common.inc"

/* Face identity is stable during editing; draw order/room ownership may change. */
static void Appearance(const BgDocument *before, const BgDocument *after)
{
    assert(before->facecount == after->facecount && before->roomcount == after->roomcount);
    for (DWORD r = 1; r <= before->roomcount; r++)
    for (DWORD f = 0; f < before->rooms[r].facecount; f++)
    {
        const BgDocumentRoom *a = &before->rooms[r], *b = NULL;
        const BgDocumentFace *x = &a->faces[f], *y = NULL;
        for (DWORD s = 1; s <= after->roomcount && !y; s++)
        for (DWORD g = 0; g < after->rooms[s].facecount; g++)
        {
            if (after->rooms[s].faces[g].id == x->id) { b = &after->rooms[s]; y = &b->faces[g]; break; }
        }
        assert(y && x->layer == y->layer && x->cullbackfaces == y->cullbackfaces
            && x->textureid == y->textureid && BgMaterialEqual(&x->material, &y->material));
        for (int c = 0; c < 3; c++)
        {
            const BgDocumentVertex *v = &a->vertices[x->vertexindices[c]], *w = &b->vertices[y->vertexindices[c]];
            assert(v->x + (double)a->origin[0] == w->x + (double)b->origin[0]);
            assert(v->y + (double)a->origin[1] == w->y + (double)b->origin[1]);
            assert(v->z + (double)a->origin[2] == w->z + (double)b->origin[2]);
            assert(v->s == w->s && v->t == w->t && v->flag == w->flag && !memcmp(&v->r, &w->r, 4));
        }
        BgFaceRef ra = {x->id,x->room,x->layer,0}, rb = {y->id,y->room,y->layer,0};
        BgRenderState sa, sb;
        assert(BgDocumentGetFaceRenderStates(before,&ra,1,&sa) && BgDocumentGetFaceRenderStates(after,&rb,1,&sb));
        assert(sa.othermode == sb.othermode && sa.othermodehigh == sb.othermodehigh
            && !((sa.geometrymode ^ sb.geometrymode) & ~0x2000u)
            && sa.environmentalpha == sb.environmentalpha && sa.primitivealpha == sb.primitivealpha
            && sa.surfacepolicy == sb.surfacepolicy && sa.surfacebasemode == sb.surfacebasemode);
    }
}

static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled = {0}, saved = {0}, again = {0}; BgDocument loaded = {0}; const char *why = "";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why));
    char path[MAX_PATH]; snprintf(path,sizeof(path),"%s/bg",dir); CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,&compiled,&why) && BgLoadProjectFile(dir,compiled.name,&saved,&why));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    assert(loaded.roomcount == doc->roomcount);
    Equivalent(doc,&loaded); UseCounts(&loaded);
    for (DWORD r = 1; r <= doc->roomcount; r++)
    {
        assert(doc->rooms[r].facecount == loaded.rooms[r].facecount);
        assert(!memcmp(doc->rooms[r].origin,loaded.rooms[r].origin,sizeof(doc->rooms[r].origin)));
        /* Empty rooms retain their bounds; populated rooms drop orphans. */
        if (!doc->rooms[r].facecount)
        { assert(loaded.rooms[r].vertexcount == doc->rooms[r].vertexcount); }
    }
    assert(BgDocumentCompile(&loaded,&saved,&again,&why));
    /* Reload/recompile may reclaim the cleared space left by cleanup. */
    assert(again.size <= saved.size && BgFileValidateVertexBatches(&again,&why));
    BgDocumentFree(&loaded);
    assert(BgDocumentLoad(again.data,again.size,doc->levelscale,&loaded,&why));
    Equivalent(doc,&loaded); UseCounts(&loaded);
    BgDocumentFree(&loaded); BgFileFree(&saved); BgFileFree(&compiled); BgFileFree(&again);
}

static void Geometry(const char *dir)
{
    BgFile source = Fixture(); BgDocument original = {0}, doc = {0}; BgFaceRef refs[20];
    const char *why = ""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,0.5f,&original,&why)); Refs(&original,refs);
    for (DWORD r = 1; r <= original.roomcount; r++) for (DWORD v = 0; v < original.rooms[r].vertexcount; v++)
    {
        BgDocumentVertex *p = &original.rooms[r].vertices[v];
        p->s = (short)(v * 123 - 170); p->t = (short)(v * 34); p->r = (unsigned char)(v * 50); p->a = 120 + v;
    }
    original.rooms[1].origin[0] = 613; original.rooms[1].origin[1] = 22; original.rooms[1].origin[2] = -122;
    original.rooms[2].origin[0] = 569; original.rooms[2].origin[1] = 29; original.rooms[2].origin[2] = -358;
    BgFacePropertiesEdit edit = {.fields = BG_FACE_PROPERTY_TRANSPARENCY, .transparency = BG_TRANSPARENCY_CUTOUT};
    assert(BgDocumentSetFaceProperties(&original,&refs[1],1,&edit,&changed,&why));
    assert(BgDocumentClone(&original,&doc,&why));
    BgFaceRef selected[] = {refs[1],refs[4],refs[6],refs[11],refs[1]};
    assert(BgDocumentMoveFacesToRoom(&doc,selected,5,2,&changed,&why) && changed);
    assert(doc.rooms[1].facecount == 7 && doc.rooms[2].facecount == 13 && doc.rooms[2].vertexcount == 6);
    assert(doc.rooms[1].vertexcount == original.rooms[1].vertexcount);
    Appearance(&original,&doc); UseCounts(&doc); RoundTrip(&doc,&source,dir);
    for (unsigned i = 0; i < 5; i++) { selected[i].room = 2; }
    assert(BgDocumentMoveFacesToRoom(&doc,selected,5,2,&changed,&why) && !changed);
    BgDocumentFree(&doc);
    BOOL succeeded = FALSE;
    for (int budget = 0; budget < 160 && !succeeded; budget++)
    {
        assert(BgDocumentClone(&original,&doc,&why)); allocations = budget;
        BOOL ok = BgDocumentMoveFacesToRoom(&doc,refs,10,2,&changed,&why);
        allocations = -1;
        if (!ok) { assert(why[0] && !changed); Same(&doc,&original); }
        else
        {
            succeeded = TRUE; assert(changed && !doc.rooms[1].facecount && doc.rooms[2].facecount == 20);
            Appearance(&original,&doc); UseCounts(&doc); RoundTrip(&doc,&source,dir);
        }
        BgDocumentFree(&doc);
    }
    assert(succeeded);
    assert(BgDocumentClone(&original,&doc,&why));
    assert(!BgDocumentMoveFacesToRoom(&doc,refs,10,0,&changed,&why)); Same(&doc,&original);
    assert(!BgDocumentMoveFacesToRoom(&doc,refs,10,3,&changed,&why)); Same(&doc,&original);
    BgFaceRef bad = refs[0]; bad.faceid = 0;
    assert(!BgDocumentMoveFacesToRoom(&doc,&bad,1,2,&changed,&why)); Same(&doc,&original);
    doc.rooms[2].origin[0] += 0.5f;
    assert(!BgDocumentMoveFacesToRoom(&doc,refs,10,2,&changed,&why) && !changed);
    doc.rooms[2].origin[0] -= 0.5f; Same(&doc,&original);
    doc.rooms[2].origin[0] += 65536;
    assert(!BgDocumentMoveFacesToRoom(&doc,refs,10,2,&changed,&why) && !changed);
    doc.rooms[2].origin[0] -= 65536; Same(&doc,&original);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: room transfers preserve positions/UVs/RGBA/materials/shared vertices; mixed selections, empty rooms, native save/reload, no-ops and atomic allocation/range failures.");
}

static void Depot(const char *file, const char *dir)
{
    BgFile source = {0}; BgDocument original = {0}, doc = {0}; const char *why = ""; BOOL changed;
    FILE *fp = fopen(file,"rb"); assert(fp); fseek(fp,0,SEEK_END); source.size = (DWORD)ftell(fp); rewind(fp);
    source.data = malloc(source.size); assert(source.data && fread(source.data,1,source.size,fp) == source.size); fclose(fp);
    strcpy(source.name,"bg/depot_room_test.seg");
    assert(BgDocumentLoad(source.data,source.size,0.2333333333f,&original,&why));
    assert(BgDocumentClone(&original,&doc,&why));
    DWORD count = doc.rooms[22].facecount; assert(count);
    BgFaceRef *refs = malloc(count*sizeof(*refs)); assert(refs);
    for (DWORD f = 0; f < count; f++) { BgDocumentFace *p = &doc.rooms[22].faces[f]; refs[f] = (BgFaceRef){p->id,22,p->layer,0}; }
    if (!BgDocumentMoveFacesToRoom(&doc,refs,count,23,&changed,&why)) { fprintf(stderr,"Depot: %s\n",why); abort(); }
    assert(changed && !doc.rooms[22].facecount && doc.rooms[23].facecount == original.rooms[23].facecount + count);
    Appearance(&original,&doc); UseCounts(&doc); RoundTrip(&doc,&source,dir);
    printf("PASS: Depot room 22 -> 23, %lu faces; empty room 22 survives native compile/save/reload.\n",(unsigned long)count);
    free(refs); BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
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
static int selectedcount=1,errors,rebuilds,restores;
static BOOL failrebuild,failselection,flying,transforming;
static EditorTool tool=EDITOR_TOOL_FACE_SELECT;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectedcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd,BgFaceRef *out,int count)
{ assert(count==selectedcount);memcpy(out,selection,count*sizeof(*out));return TRUE; }
static BOOL ViewportSelectBgFaces(HWND hwnd,const BgFaceRef *refs,DWORD count)
{ if (failselection) { return FALSE; } memcpy(selection,refs,count*sizeof(*refs)); return TRUE; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ rebuilds++; if(failrebuild) { failrebuild=FALSE;*why="test rebuild failure";return FALSE; } return TRUE; }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++; }
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]);errors++; }
#include "editor.inc"

static void Commands(void)
{
    BgFile source = Fixture(); BgDocument original = {0}, moved = {0}; BgFaceRef refs[20]; const char *why = "";
    assert(BgDocumentLoad(source.data,source.size,1,&g_CurrentBgDocument,&why));
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why)); Refs(&original,refs);
    selection[0] = refs[1]; selection[1] = refs[6]; selectedcount = 2;
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(GEditorMoveSelectedFacesToRoom((HWND)1,2));
    assert(selection[0].room == 2 && selection[1].room == 2 && selection[0].faceid == refs[1].faceid);
    assert(g_EditHistory.undocount == 1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Change BG Room"));
    assert(BgDocumentClone(&g_CurrentBgDocument,&moved,&why));
    assert(GEditorMoveSelectedFacesToRoom((HWND)1,2) && g_EditHistory.undocount == 1 && rebuilds == 1);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&original);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); Same(&g_CurrentBgDocument,&moved);
    selection[0] = refs[2]; selectedcount = 1;
    failrebuild = TRUE; assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2)); Same(&g_CurrentBgDocument,&moved);
    failselection = TRUE; assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2)); failselection = FALSE; Same(&g_CurrentBgDocument,&moved);
    ULONGLONG revision = g_EditHistory.nextrevision; g_EditHistory.nextrevision = 0;
    assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2)); g_EditHistory.nextrevision = revision; Same(&g_CurrentBgDocument,&moved);
    assert(errors == 3 && restores == 3 && g_EditHistory.undocount == 1);
    flying = TRUE; assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2)); flying = FALSE;
    transforming = TRUE; assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2)); transforming = FALSE;
    tool = EDITOR_TOOL_VERTEX_SELECT; assert(!GEditorMoveSelectedFacesToRoom((HWND)1,2));
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original); BgDocumentFree(&moved); BgFileFree(&source);
    puts("PASS: editor selection follows moved faces; undo/redo, no-op history and rebuild/selection/commit rollback.");
}
int main(int argc, char **argv) { assert(argc == 3); Geometry(argv[1]); Depot(argv[2],argv[1]); Commands(); return 0; }
