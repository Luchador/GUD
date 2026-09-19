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

static void InheritedShading(const char *dir)
{
    BgFile source = Fixture();
    BgDocument original = {0}, doc = {0};
    BgFaceRef refs[20];
    const char *why = "";
    BOOL changed;
    const DWORD shading = 0x00000204u; /* G_SHADE | G_SHADING_SMOOTH. */

    assert(BgDocumentLoad(source.data,source.size,1,&original,&why));
    Refs(&original,refs);
    /* Like Depot 29 -> 14: the source inherits runtime shading defaults;
     * the destination explicitly enables those same flags in its last group. */
    BgDocumentLayerData *inherited = &original.rooms[1].layers[0];
    for (DWORD g = 0; g < inherited->groupcount; g++)
    for (DWORD off = 0; off < inherited->groups[g].commandsize; off += 8)
    {
        unsigned char *cmd = inherited->groups[g].commands + off;
        if (cmd[0] == 0xb6 || cmd[0] == 0xb7)
        { Put(cmd+4,BgDocumentRead32(cmd+4) & ~shading); }
    }
    BgDocumentLayerData *layer = &original.rooms[2].layers[0];
    assert(layer->groupcount);
    assert(BgDocumentSurfaceCommand(&layer->groups[layer->groupcount-1],0xb7000000,shading));
    BgRenderState state;
    assert(BgDocumentGetFaceRenderStates(&original,&refs[0],1,&state));
    assert((state.geometrymode & shading) == shading && !(state.geometryknown & shading));
    assert(BgDocumentClone(&original,&doc,&why));
    assert(BgDocumentMoveFacesToRoom(&doc,refs,2,2,&changed,&why) && changed);
    Appearance(&original,&doc); UseCounts(&doc); RoundTrip(&doc,&source,dir);
    BgDocumentFree(&doc);

    /* A genuinely different shading state must still fail atomically. */
    assert(BgDocumentSurfaceCommand(&layer->groups[layer->groupcount-1],0xb6000000,shading));
    assert(BgDocumentClone(&original,&doc,&why));
    assert(!BgDocumentMoveFacesToRoom(&doc,refs,2,2,&changed,&why) && !changed);
    assert(strstr(why,"render state")); Same(&doc,&original);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgFileFree(&source);
    puts("PASS: inherited runtime shading matches explicit shading through room transfer and save/reload; real conflicts retain atomic rejection.");
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
static DWORD StateBytes(const BgDocument *doc)
{
    DWORD size = 0;
    for (DWORD r = 1; r <= doc->roomcount; r++) for (unsigned int l = 0; l < 2; l++)
    {
        const BgDocumentLayerData *layer = &doc->rooms[r].layers[l];
        assert(layer->groupcount <= doc->rooms[r].facecount + 1);
        for (DWORD g = 0; g < layer->groupcount; g++) { size += layer->groups[g].commandsize; }
    }
    return size;
}

static void RepeatedTransfers(const char *dir)
{
    BgFile source = Fixture(), saved = {0}; BgDocument doc = {0}, original = {0}, loaded = {0};
    BgFaceRef refs[20]; const char *why = ""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    assert(BgDocumentClone(&doc,&original,&why)); Refs(&doc,refs);
    DWORD limit = StateBytes(&doc) * 2 + 1024;
    for (unsigned int i = 0; i < 100; i++)
    {
        DWORD target = refs[1].room == 1 ? 2 : 1;
        assert(BgDocumentMoveFacesToRoom(&doc,&refs[1],1,target,&changed,&why) && changed);
        refs[1].room = (unsigned short)target;
        assert(StateBytes(&doc) < limit);
        Appearance(&original,&doc);
    }
    RoundTrip(&doc,&source,dir);
    assert(BgDocumentCompile(&doc,&source,&saved,&why));
    assert(BgDocumentLoad(saved.data,saved.size,1,&loaded,&why));
    assert(!loaded.dirty);
    BgDocumentFree(&loaded); BgDocumentFree(&doc); BgDocumentFree(&original);
    BgFileFree(&source); BgFileFree(&saved);
    puts("PASS: 100 alternating room transfers retain every face's appearance without accumulating render-state history.");
}

/* A loaded legacy room with repeated state must stay dirty across history
 * reset, selection edits and undo; otherwise Save Project skips its repair. */
static void LegacyRepair(const char *dir)
{
    BgFile source = Fixture(), bloated = {0}, saved = {0}; BgDocument doc = {0}, loaded = {0};
    BgDocument original = {0}; EditHistory history = {0}; EditHistoryTransaction tx = {0};
    SetupFile setup = {0}; StanFile stan = {0};
    const char *why = ""; BOOL changed;
    assert(BgDocumentLoad(source.data,source.size,1,&doc,&why));
    BgDocumentDrawGroup *group = &doc.rooms[1].layers[0].groups[0];
    /* Thousands of repeated sync, full RGB colours, and mixed/partial bit
     * writes before the first face reproduce the live Depot command bloat. */
    for (unsigned int i = 0; i < 1024; i++)
    {
        assert(BgDocumentSurfaceCommand(group,0xe7000000,0));
        assert(BgDocumentSurfaceCommand(group,0xb7000000,0x2000));
        assert(BgDocumentSurfaceCommand(group,0xfb000000,0x123456ff));
        assert(BgDocumentSurfaceCommand(group,0xfa008080,0xabcdefaa));
        assert(BgDocumentSurfaceCommand(group,0xba000c02,0x2000));
    }
    assert(BgDocumentClone(&doc,&original,&why));
    assert(BgDocumentCompile(&doc,&source,&bloated,&why));
    assert(BgDocumentLoad(bloated.data,bloated.size,1,&loaded,&why) && loaded.dirty);
    Appearance(&original,&loaded);
    EditHistoryReset(&history,&loaded,NULL,NULL); assert(loaded.dirty);
    assert(EditHistoryBeginBgEdit(&history,&loaded,"Test repair history",&tx,&why));
    BgFaceRef ref = {loaded.rooms[1].faces[0].id,1,loaded.rooms[1].faces[0].layer,0};
    assert(BgDocumentMoveFacesToRoom(&loaded,&ref,1,2,&changed,&why) && changed);
    assert(EditHistoryCommitEdit(&history,&loaded,NULL,NULL,&tx,&why));
    assert(EditHistoryUndo(&history,&loaded,&setup,&stan,NULL,&why) && loaded.dirty);
    assert(BgDocumentCompile(&loaded,&bloated,&saved,&why));
    assert(saved.size + 30000 < bloated.size);
    EditHistoryMarkBgSaved(&history,&loaded); assert(!loaded.dirty);
    RoundTrip(&loaded,&saved,dir);
    EditHistoryFree(&history); BgDocumentFree(&doc); BgDocumentFree(&original); BgDocumentFree(&loaded);
    BgFileFree(&source); BgFileFree(&bloated); BgFileFree(&saved);
    puts("PASS: old bloated files repair on load/save, retaining appearances and pending repair through history reset/undo.");
}

static void StateBarriers(void)
{
    BgDocumentDrawGroup group = {0};
    const DWORD input[][2] = {
        {0xe7000000,0}, {0xb9000008,0xaa}, {0xb9000004,5},
        {0xfb000000,0x11223344}, {0xfb000000,0x55667788},
        {0x03860010,0x0e000100}, /* Opaque DMA/light command: preserve both sides. */
        {0xef123456,0x98765432}, {0xba000008,0xab}, {0xb9000004,7},
        {0xfb000000,0xaabbccdd}, {0xfb000000,0xffffffff},
        {0xb6000000,0x3000}, {0xb7000000,0x2000},
        {0xe7000000,0}, {0xfa000080,0x01020304}, {0xe7000000,0},
        {BG_SURFACE_MARKER,BG_SURFACE_TAG_VALUE(BG_SURFACE_OPAQUE,0x2018)},
        {BG_SURFACE_MARKER,BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND,0x2038)}
    };
    const DWORD expected[][2] = {
        {0xe7000000,0}, {0xb9000008,0xaa}, {0xb9000004,5}, {0xfb000000,0x55667788},
        {0x03860010,0x0e000100},
        {0xef123456,0x98765432}, {0xba000008,0xab}, {0xb9000004,7},
        {0xfb000000,0xffffffff}, {0xb6000000,0x3000}, {0xb7000000,0x2000},
        {0xe7000000,0}, {0xfa000080,0x01020304},
        {BG_SURFACE_MARKER,BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND,0x2038)}
    };
    for (unsigned int i = 0; i < sizeof(input)/sizeof(input[0]); i++)
    { assert(BgDocumentSurfaceCommand(&group,input[i][0],input[i][1])); }
    BgDocumentCompactCommands(&group);
    assert(group.commandsize == sizeof(expected));
    for (unsigned int i = 0; i < sizeof(expected)/sizeof(expected[0]); i++)
    {
        assert(BgDocumentRead32(group.commands+i*8) == expected[i][0]);
        assert(BgDocumentRead32(group.commands+i*8+4) == expected[i][1]);
    }
    BgDocumentCompactCommands(&group); assert(group.commandsize == sizeof(expected)); free(group.commands);
    puts("PASS: partial register writes, full RGB/alpha, geometry masks, surface tags, leading sync and opaque-command barriers survive cleanup.");
}

int main(int argc, char **argv) { assert(argc == 3); Geometry(argv[1]); InheritedShading(argv[1]); RepeatedTransfers(argv[1]); LegacyRepair(argv[1]); StateBarriers(); Depot(argv[2],argv[1]); Commands(); return 0; }
