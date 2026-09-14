#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"

/* BG history is real; setup and stan edits are outside this command. */
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
#include "fixture.inc"

/* Compare rendered geometry after each save. The compiler can split native
   draw groups when adjacent faces have different culling states, so command
   bytes need not be identical between the first and second compilation. */
static void RoundTrip(const BgDocument *doc, const BgFile *source, const char *dir)
{
    BgFile compiled = {0}, saved = {0}, again = {0};
    BgDocument loaded = {0}, reloaded = {0};
    const char *why = "";
    char path[MAX_PATH];
    assert(BgDocumentCompile(doc, source, &compiled, &why));
    assert(BgFileValidateVertexBatches(&compiled, &why));
    snprintf(path, sizeof(path), "%s/bg", dir); CreateDirectory(path, NULL);
    assert(BgSaveProjectFile(dir, &compiled, &why));
    assert(BgLoadProjectFile(dir, compiled.name, &saved, &why));
    assert(saved.size == compiled.size && !memcmp(saved.data, compiled.data, saved.size));
    assert(BgDocumentLoad(saved.data, saved.size, doc->levelscale, &loaded, &why));
    Equivalent(doc, &loaded);
    assert(BgDocumentCompile(&loaded, &saved, &again, &why));
    assert(BgFileValidateVertexBatches(&again, &why));
    assert(BgDocumentLoad(again.data, again.size, doc->levelscale, &reloaded, &why));
    Equivalent(doc, &reloaded);
    BgDocumentFree(&loaded); BgDocumentFree(&reloaded);
    BgFileFree(&compiled); BgFileFree(&saved); BgFileFree(&again);
}

static void SameGeometry(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount == b->facecount && a->roomcount == b->roomcount);
    assert(a->nextfaceid == b->nextfaceid && a->nextvertexid == b->nextvertexid);
    for (DWORD r = 1; r <= a->roomcount; r++)
    {
        const BgDocumentRoom *x = &a->rooms[r], *y = &b->rooms[r];
        assert(x->facecount == y->facecount && x->vertexcount == y->vertexcount);
        assert(!memcmp(x->faces, y->faces, x->facecount * sizeof(*x->faces)));
        assert(!memcmp(x->vertices, y->vertices, x->vertexcount * sizeof(*x->vertices)));
    }
    Equivalent(a, b);
}

static void Geometry(const char *dir)
{
    BgFile source = Fixture();
    BgDocument doc = {0}, original = {0}, expected = {0};
    BgFaceRef refs[20], selected[4], invalid[2];
    BgDocumentRenderMesh before = {0}, after = {0};
    const char *why = "";
    assert(BgDocumentLoad(source.data, source.size, 1.0f, &doc, &why));
    assert(Refs(&doc, refs) == 20);
    /* Distinct attributes expose accidental UV/color edits or vertex swaps. */
    for (DWORD r = 1; r <= doc.roomcount; r++)
    {
        BgDocumentRoom *room = &doc.rooms[r];
        for (DWORD i = 0; i < room->vertexcount; i++)
        {
            BgDocumentVertex *v = &room->vertices[i];
            v->s = (short)(i * 128 - 64); v->t = (short)(320 - i * 96);
            v->r = (unsigned char)(i * 70); v->g = 128; v->b = (unsigned char)(255 - i * 40);
            v->a = (unsigned char)(192 + i * 20);
        }
        for (DWORD i = 0; i < room->facecount; i++) { room->faces[i].cullbackfaces = i % 2; }
    }
    assert(BgDocumentClone(&doc, &original, &why));
    assert(BgDocumentClone(&doc, &expected, &why));
    assert(BgDocumentBuildRenderMesh(&doc, &before, &why));
    /* One face in each room/layer, all sharing vertices with unselected faces. */
    for (DWORD i = 0; i < 4; i++)
    {
        selected[i] = refs[i * 5 + 1];
        BgDocumentFace *face = (BgDocumentFace *)BgDocumentFindFace(&expected, &selected[i], NULL);
        DWORD v = face->vertexindices[1]; face->vertexindices[1] = face->vertexindices[2]; face->vertexindices[2] = v;
    }
    assert(BgDocumentFlipFaces(&doc, selected, 4, &why) && doc.dirty);
    SameGeometry(&doc, &expected);
    assert(BgDocumentBuildRenderMesh(&doc, &after, &why));
    for (DWORD i = 0; i < before.facecount; i++)
    {
        BOOL flipped = i % 5 == 1;
        assert(before.tags[i] == after.tags[i] && before.renderflags[i] == after.renderflags[i]);
        assert(!memcmp(&before.facerefs[i], &after.facerefs[i], sizeof(BgFaceRef)));
        for (int c = 0; c < 3; c++)
        {
            int old = flipped && c ? 3 - c : c;
            const BgVertex *v = &before.vertices[i * 3 + old], *w = &after.vertices[i * 3 + c];
            assert(v->x == w->x && v->y == w->y && v->z == w->z && v->s == w->s && v->t == w->t);
            assert(v->r == w->r && v->g == w->g && v->b == w->b && v->a == w->a);
            assert(!memcmp(&before.vertexrefs[i * 3 + old], &after.vertexrefs[i * 3 + c], sizeof(BgDocumentVertexRef)));
        }
    }
    RoundTrip(&doc, &source, dir);
    assert(BgDocumentFlipFaces(&doc, selected, 4, NULL));
    SameGeometry(&doc, &original);
    doc.dirty = FALSE;
    invalid[0] = refs[0]; invalid[1] = refs[1]; invalid[1].faceid = BG_FACE_ID_NONE;
    assert(!BgDocumentFlipFaces(&doc, invalid, 2, &why) && why[0]);
    invalid[1] = invalid[0];
    assert(!BgDocumentFlipFaces(&doc, invalid, 2, &why));
    invalid[1] = refs[1]; invalid[1].room = (unsigned short)(doc.roomcount + 1);
    assert(!BgDocumentFlipFaces(&doc, invalid, 2, &why));
    invalid[1] = refs[1]; invalid[1].layer = 255;
    assert(!BgDocumentFlipFaces(&doc, invalid, 2, &why));
    assert(!BgDocumentFlipFaces(&doc, refs, 0, NULL));
    assert(!BgDocumentFlipFaces(&doc, NULL, 1, NULL));
    assert(!BgDocumentFlipFaces(NULL, refs, 1, NULL));
    assert(!doc.dirty);
    SameGeometry(&doc, &original);
    BgDocumentRenderMeshFree(&before); BgDocumentRenderMeshFree(&after);
    BgDocumentFree(&doc); BgDocumentFree(&original); BgDocumentFree(&expected); BgFileFree(&source);
    puts("PASS: winding, shared vertices, UV/colors, culling, native save/reload and atomic invalid selections.");
}

/* Execute the actual editor command and hotkey handler with only window and
   viewport UI calls stubbed. Document edits and history are production code. */
typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct MSG { HWND hwnd; unsigned message; uintptr_t wParam; LPARAM lParam; } MSG;
enum { WM_KEYDOWN = 256, WM_SYSKEYDOWN = 260, WM_COMMAND = 273,
       VK_CONTROL = 17, VK_MENU = 18, VK_SHIFT = 16, ID_EDIT_FLIP_FACE = 100, MB_ICONERROR = 16 };
#define GEDITOR_TITLE "GEditor"
static HWND g_Viewport = (HWND)2;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BgFaceRef selection[4];
static int selectedcount = 1, rebuilds, errors;
static BOOL flying, transforming, control, alt = TRUE, shift, failrebuild, failselection;
static EditorTool tool = EDITOR_TOOL_FACE_SELECT;
static const char *classname = "GEditorViewport";
static unsigned sent;
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static EditorTool ViewportGetTool(HWND hwnd) { return tool; }
static int ViewportGetSelectedBgFaceCount(HWND hwnd) { return selectedcount; }
static BOOL ViewportGetSelectedBgFaces(HWND hwnd, BgFaceRef *out, int count)
{ assert(count == selectedcount); memcpy(out, selection, count * sizeof(*out)); return !failselection; }
static BOOL GEditorRebuildCurrentViewport(const char **why)
{
    rebuilds++;
    if (failrebuild) { failrebuild = FALSE; *why = "injected rebuild failure"; return FALSE; }
    return TRUE;
}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd, const char *why, const char *title, unsigned flags)
{ assert(why[0]); errors++; }
static BOOL IsChild(HWND parent, HWND child) { return parent == (HWND)1 && child == (HWND)2; }
static short GetKeyState(int key) { return (key == VK_CONTROL ? control : key == VK_MENU ? alt : shift) ? (short)0x8000 : 0; }
static void GetClassName(HWND hwnd, char *out, int size) { snprintf(out, size, "%s", classname); }
static BOOL GEditorFlipSelectedBgFaces(HWND hwnd);
static void SendMessage(HWND hwnd, unsigned message, uintptr_t wparam, LPARAM lparam)
{ assert(hwnd == (HWND)1 && message == WM_COMMAND && wparam == ID_EDIT_FLIP_FACE); sent++; GEditorFlipSelectedBgFaces(hwnd); }
#include "editor.inc"

static void CommandAndHotkey(const char *dir)
{
    BgFile source = Fixture();
    BgDocument original = {0}, flipped = {0};
    BgFaceRef refs[20];
    const char *why = "";
    HWND frame = (HWND)1;
    MSG msg = {.hwnd = g_Viewport, .message = WM_SYSKEYDOWN, .wParam = 'N'};
    assert(BgDocumentLoad(source.data, source.size, 1.0f, &g_CurrentBgDocument, &why));
    assert(BgDocumentClone(&g_CurrentBgDocument, &original, &why));
    Refs(&g_CurrentBgDocument, refs); selection[0] = refs[1];
    EditHistoryReset(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory, selection, sizeof(selection[0]), FALSE, &why));
    assert(GEditorCanFlipSelectedBgFaces());
    assert(GEditorHandleFlipFaceHotkey(frame, &msg) && sent == 1 && rebuilds == 1);
    assert(g_CurrentBgDocument.dirty && g_EditHistory.undocount == 1);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Flip Face"));
    assert(BgDocumentClone(&g_CurrentBgDocument, &flipped, &why));
    RoundTrip(&g_CurrentBgDocument, &source, dir);
    msg.lParam = (LPARAM)1 << 30;
    assert(GEditorHandleFlipFaceHotkey(frame, &msg) && sent == 1 && g_EditHistory.undocount == 1);
    msg.lParam = 0;
    assert(EditHistoryUndo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    SameGeometry(&g_CurrentBgDocument, &original); assert(!g_CurrentBgDocument.dirty);
    assert(g_EditHistory.selectionsize == sizeof(selection[0]));
    assert(!memcmp(g_EditHistory.selection, selection, sizeof(selection[0])));
    assert(EditHistoryRedo(&g_EditHistory, &g_CurrentBgDocument, &g_CurrentSetup, &g_CurrentStan, NULL, &why));
    SameGeometry(&g_CurrentBgDocument, &flipped); assert(g_CurrentBgDocument.dirty);
    EditHistoryMarkBgSaved(&g_EditHistory, &g_CurrentBgDocument);
    failrebuild = TRUE;
    assert(!GEditorFlipSelectedBgFaces(frame) && errors == 1 && rebuilds == 3);
    SameGeometry(&g_CurrentBgDocument, &flipped); assert(!g_CurrentBgDocument.dirty);
    ULONGLONG revision = g_EditHistory.nextrevision;
    g_EditHistory.nextrevision = 0; /* Commit must fail after the viewport rebuild. */
    assert(!GEditorFlipSelectedBgFaces(frame) && errors == 2 && rebuilds == 5);
    g_EditHistory.nextrevision = revision;
    SameGeometry(&g_CurrentBgDocument, &flipped);
    assert(!g_CurrentBgDocument.dirty && g_EditHistory.undocount == 1);
    failselection = TRUE; assert(!GEditorFlipSelectedBgFaces(frame) && errors == 3); failselection = FALSE;
    assert(rebuilds == 5 && g_EditHistory.undocount == 1);

    const char *inputs[] = {"Edit", "ComboBox", "ComboLBox"};
    for (unsigned i = 0; i < 3; i++) { classname = inputs[i]; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); }
    classname = "GEditorViewport";
    msg.hwnd = (HWND)3; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); msg.hwnd = g_Viewport;
    control = TRUE; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); control = FALSE;
    shift = TRUE; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); shift = FALSE;
    alt = FALSE; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); alt = TRUE;
    flying = TRUE; assert(!GEditorHandleFlipFaceHotkey(frame, &msg) && !GEditorCanFlipSelectedBgFaces()); flying = FALSE;
    transforming = TRUE; assert(!GEditorFlipSelectedBgFaces(frame)); transforming = FALSE;
    tool = EDITOR_TOOL_VERTEX_SELECT; assert(!GEditorFlipSelectedBgFaces(frame)); tool = EDITOR_TOOL_FACE_SELECT;
    selectedcount = 0; assert(!GEditorFlipSelectedBgFaces(frame)); selectedcount = 1;
    msg.wParam = 'M'; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); msg.wParam = 'N';
    msg.message = WM_COMMAND; assert(!GEditorHandleFlipFaceHotkey(frame, &msg)); msg.message = WM_KEYDOWN;
    assert(!GEditorHandleFlipFaceHotkey(frame, NULL));
    g_Viewport = NULL; assert(!GEditorHandleFlipFaceHotkey(frame, &msg) && !GEditorCanFlipSelectedBgFaces()); g_Viewport = (HWND)2;
    assert(sent == 1 && errors == 3 && g_EditHistory.undocount == 1);
    selection[1] = refs[6]; selectedcount = 2;
    assert(GEditorHandleFlipFaceHotkey(frame, &msg) && sent == 2);
    assert(!strcmp(EditHistoryGetUndoAction(&g_EditHistory), "Flip Faces") && g_EditHistory.undocount == 2);
    EditHistoryFree(&g_EditHistory);
    BgDocumentFree(&g_CurrentBgDocument); BgDocumentFree(&original); BgDocumentFree(&flipped); BgFileFree(&source);
    puts("PASS: editor command, undo/redo, saved state, rollback, Alt+N scoping and repeat suppression.");
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    Geometry(argv[1]); CommandAndHotkey(argv[1]);
    return 0;
}
