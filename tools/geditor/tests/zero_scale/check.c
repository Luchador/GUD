#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
#include "objectload.h"
#include "fixture.inc"

BOOL SetupFileCompact(SetupFile *setup, const char **why) { abort(); }
BOOL SetupFileClone(const SetupFile *source, SetupFile *out, const char **why) { abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
typedef void *HWND;
typedef intptr_t LPARAM;
typedef struct ViewportRotation { Rotation rotation; double pivot[3]; } ViewportRotation;
typedef struct RightPanelPosition { double position[3]; unsigned int axismask; } RightPanelPosition;
static HWND g_Viewport = (HWND)1;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static SetupObjectGeometry g_CurrentObjects;
static EditorTool tool = EDITOR_TOOL_FACE_SELECT;
static BOOL selectingstan, selectingobject, selectingpad, failrebuild;
static Scaling currentScale;
static int prompts, errors, refreshes, confirm = 1;
static DWORD bgcount = 6, stancount = 8;
enum { MB_OKCANCEL=1, MB_ICONWARNING=16, MB_ICONERROR=32, IDOK=1,
       EM_SETSEL=99, RIGHTPANEL_WM_SET_SCALE=100, RIGHTPANEL_WM_SET_ROTATION=101, RIGHTPANEL_WM_SET_POSITION=102 };
#define GEDITOR_TITLE "GEditor"
static int Never(void) { abort(); }
#define ViewportGetSelectedModelCount(...) 0
#define ViewportKnifeActive(...) FALSE
#define ViewportGetPortalSelectionCount(...) 0
#define ViewportGetSelectedMarker(...) FALSE
#define GEditorTransformModels(...) Never()
#define ViewportTransformKnife(...) Never()
#define GEditorTransformPortals(...) Never()
#define GEditorTransformMarker(...) Never()
static BOOL ViewportGetSelectedPad(HWND hwnd, SetupPadRef *out) { *out=(SetupPadRef){0}; return selectingpad; }
#define ViewportGetStanSelectionCount(...) (selectingstan ? 2 : 0)
static BOOL ViewportGetSelectedObject(HWND hwnd, DWORD *index) { *index=0; return selectingobject; }
#define GEditorCanMoveSetupModel(...) TRUE
#define ViewportGetTool(...) tool
#define SetupFileScalePad(...) Never()
#define SetupFileRotatePad(...) Never()
#define ObjectLoadSetupGeometry(...) Never()
#define ObjectScaleSetupModel(...) Never()
#define ObjectRotateSetupModel(...) Never()
#define ObjectGeometryFree(...) ((void)0)
#define ViewportSelectPad(...) ((void)0)
static BgDocumentVertexRef *ViewportGetMoveVertices(HWND hwnd, DWORD *count)
{
    *count=bgcount; BgDocumentVertexRef *p=malloc(bgcount*sizeof(*p)); assert(p);
    for (DWORD i=0; i<bgcount; i++) { p[i]=(BgDocumentVertexRef){i/3+1,i%3}; }
    return p;
}
static StanPointRef *ViewportGetMoveStanPoints(HWND hwnd, DWORD *count)
{
    *count=stancount; StanPointRef *p=malloc(stancount*sizeof(*p)); assert(p);
    for (DWORD i=0; i<stancount; i++) { p[i]=(StanPointRef){i/4,i%4}; }
    return p;
}
static BOOL GEditorRebuildCurrentViewport(const char **why)
{ if (failrebuild) { failrebuild=FALSE; *why="test rebuild failure"; return FALSE; } return TRUE; }
#define GEditorReloadCurrentObjectsAndViewport GEditorRebuildCurrentViewport
#define GEditorRebuildCurrentViewportWithObjects(objects, why) GEditorRebuildCurrentViewport(why)
#define GEditorRestoreHistorySelection(...) ((void)0)
#define GEditorRefreshSelectionDetails(...) ((void)0)
#define GEditorRefreshHistoryMenu(...) ((void)0)
static int MessageBox(HWND hwnd, const char *message, const char *title, unsigned flags)
{ assert(message[0]); if (flags&MB_OKCANCEL) { prompts++; return confirm; } errors++; return 0; }
#include "editor.inc"
static BOOL ViewportGetScaling(HWND hwnd, Scaling *out) { *out=currentScale; return TRUE; }
static void GEditorRefreshTransformFields(void) { refreshes++; }
#include "dispatch.inc"

/* Same function invoked by Enter in the panel; real frame dispatch and edits. */
typedef struct RightPanelState {
    BOOL transformenabled, rotationmode, scalemode, scaleallowzero;
    unsigned int editedaxes;
    HWND positions[3];
} RightPanelState;
static BOOL dispatched, result;
static HWND GetParent(HWND hwnd) { return hwnd; }
static void GetWindowText(HWND hwnd, char *text, size_t size) { snprintf(text,size,"%s",(const char *)hwnd); }
static void SetFocus(HWND hwnd) {}
static intptr_t SendMessage(HWND hwnd, unsigned msg, uintptr_t wparam, LPARAM lparam)
{
    if (msg==EM_SETSEL) { return 0; }
    assert(msg==RIGHTPANEL_WM_SET_SCALE); dispatched=TRUE;
    result=DispatchScale(hwnd,lparam); return result;
}
#include "panel.inc"
static BOOL Enter(unsigned mask, const char *x, const char *y, const char *z, BOOL allowzero)
{
    RightPanelState p={.transformenabled=TRUE,.scalemode=TRUE,.scaleallowzero=allowzero,
        .editedaxes=mask,.positions={(HWND)x,(HWND)y,(HWND)z}};
    dispatched=result=FALSE; RightPanelSetPosition((HWND)2,&p); return dispatched && result;
}
static void SameBg(const BgDocument *a, const BgDocument *b)
{
    assert(a->facecount==b->facecount && a->dirty==b->dirty);
    for (DWORD r=1; r<=a->roomcount; r++)
    {
        const BgDocumentRoom *x=&a->rooms[r], *y=&b->rooms[r];
        assert(x->facecount==y->facecount && x->vertexcount==y->vertexcount);
        assert(!memcmp(x->vertices,y->vertices,x->vertexcount*sizeof(*x->vertices)));
        assert(!memcmp(x->faces,y->faces,x->facecount*sizeof(*x->faces)));
    }
}
static void ResetScale(double x, double y, double z)
{
    memset(&currentScale,0,sizeof(currentScale)); RotationAxis(&currentScale.axes,0,0);
    currentScale.factor[0]=currentScale.factor[1]=currentScale.factor[2]=1;
    currentScale.pivot[0]=x; currentScale.pivot[1]=y; currentScale.pivot[2]=z;
}
static void Background(void)
{
    BgFile source=Fixture(); BgDocument original={0}, flattened={0}; const char *why="";
    assert(BgDocumentLoad(source.data,source.size,.5f,&g_CurrentBgDocument,&why));
    const short coords[3][3]={{0,1,-3},{4,6,2},{5,8,7}};
    for (DWORD r=1; r<=2; r++)
    {
        BgDocumentRoom *room=&g_CurrentBgDocument.rooms[r];
        for (int a=0; a<3; a++) { room->origin[a]=r==1?0:11; }
        for (int v=0; v<3; v++)
        { room->vertices[v].x=coords[v][0]; room->vertices[v].y=coords[v][1]; room->vertices[v].z=coords[v][2]; }
    }
    assert(BgDocumentClone(&g_CurrentBgDocument,&original,&why));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    ResetScale(17,21,15); /* Average native position = (8.5,10.5,7.5). */
    for (tool=EDITOR_TOOL_VERTEX_SELECT; tool<=EDITOR_TOOL_FACE_SELECT; tool++) for (int axis=0; axis<3; axis++)
    {
        const char *fields[3]={"untouched","untouched","untouched"}; fields[axis]="0";
        assert(Enter(1u<<axis,fields[0],fields[1],fields[2],TRUE));
        assert(g_EditHistory.undocount==1 && g_CurrentBgDocument.facecount==20 && prompts==0);
        for (DWORD r=1; r<=2; r++) for (int v=0; v<3; v++)
        {
            const BgDocumentRoom *room=&g_CurrentBgDocument.rooms[r];
            const BgDocumentVertex *p=&room->vertices[v];
            short xyz[3]={p->x,p->y,p->z};
            for (int a=0; a<3; a++)
            { assert(a==axis ? xyz[a]+room->origin[a]==round(currentScale.pivot[a]*.5) : xyz[a]==coords[v][a]); }
        }
        BgFile compiled={0}; BgDocument loaded={0};
        assert(BgDocumentCompile(&g_CurrentBgDocument,&source,&compiled,&why));
        assert(BgDocumentLoad(compiled.data,compiled.size,.5f,&loaded,&why)); Equivalent(&g_CurrentBgDocument,&loaded);
        BgDocumentFree(&loaded); BgFileFree(&compiled);
        assert(BgDocumentClone(&g_CurrentBgDocument,&flattened,&why));
        assert(Enter(1u<<axis,fields[0],fields[1],fields[2],TRUE) && g_EditHistory.undocount==1);
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); SameBg(&g_CurrentBgDocument,&original);
        assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); SameBg(&g_CurrentBgDocument,&flattened);
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        BgDocumentFree(&flattened);
    }
    tool=EDITOR_TOOL_FACE_SELECT;
    /* Two zero axes collapse all faces: Cancel is atomic; OK + Undo retains topology. */
    confirm=0; assert(!Enter(3,"0","0","untouched",TRUE)); SameBg(&g_CurrentBgDocument,&original);
    assert(g_EditHistory.undocount==0 && prompts==1);
    confirm=IDOK; assert(Enter(3,"0","0","untouched",TRUE));
    assert(g_CurrentBgDocument.facecount==0 && g_EditHistory.undocount==1 && prompts==2);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why)); SameBg(&g_CurrentBgDocument,&original);
    failrebuild=TRUE; assert(!Enter(1,"0","unused","unused",TRUE)); SameBg(&g_CurrentBgDocument,&original);
    ULONGLONG revision=g_EditHistory.nextrevision; g_EditHistory.nextrevision=0;
    assert(!Enter(1,"0","unused","unused",TRUE)); g_EditHistory.nextrevision=revision; SameBg(&g_CurrentBgDocument,&original);
    /* Geometry-only validation is enforced by both the panel and the controller. */
    int before=errors;
    assert(!Enter(1,"0","unused","unused",FALSE) && !dispatched);
    assert(!Enter(1,"-1","unused","unused",TRUE) && !dispatched);
    assert(!Enter(1,"nan","unused","unused",TRUE) && !dispatched);
    assert(!Enter(1,"1000001","unused","unused",TRUE) && !dispatched);
    assert(errors==before+4);
    selectingobject=TRUE; assert(!Enter(1,"0","unused","unused",TRUE)); selectingobject=FALSE;
    selectingpad=TRUE; assert(!Enter(1,"0","unused","unused",TRUE)); selectingpad=FALSE;
    SameBg(&g_CurrentBgDocument,&original);
    EditHistoryFree(&g_EditHistory); BgDocumentFree(&original); BgDocumentFree(&g_CurrentBgDocument); BgFileFree(&source);
    puts("PASS: BG vertices/edges/faces, all axes, shared cross-room plane at half-unit pivots, native roundtrip, no-op history, undo/redo, collapse confirmation and failure rollback.");
}
static void Put16(unsigned char *p, unsigned value) { p[0]=value>>8; p[1]=value; }
static void Stans(void)
{
    unsigned char raw[124]={0}; const char *why=""; StanFile before={0};
    Put(raw+4,0x0e00000c);
    const short xyz[2][4][3]={{{0,0,0},{0,8,20},{20,14,20},{20,6,0}},
                             {{20,6,0},{20,14,20},{40,20,20},{40,12,0}}};
    for (DWORD t=0; t<2; t++)
    {
        unsigned char *p=raw+12+t*40; Put(p,((100+t)<<8)|(t+1)); Put16(p+4,0x0abc); Put16(p+6,0x4012);
        for (int v=0; v<4; v++) for (int a=0; a<3; a++) { Put16(p+8+v*8+a*2,xyz[t][v][a]); }
    }
    Put16(raw+12+8+2*8+6,0x15); Put16(raw+52+8+6,0x10); memcpy(raw+100,"unstric",8);
    assert(StanLoadNative(raw,sizeof(raw),.5f,&g_CurrentStan,&why));
    assert(StanFileClone(&g_CurrentStan,&before,&why)); selectingstan=TRUE;
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    ResetScale(40,20,20);
    for (tool=EDITOR_TOOL_VERTEX_SELECT; tool<=EDITOR_TOOL_FACE_SELECT; tool++)
    {
        assert(Enter(2,"unused","0","unused",TRUE));
        assert(g_CurrentStan.tilecount==2 && g_EditHistory.undocount==1);
        for (DWORD t=0; t<2; t++) for (int v=0; v<4; v++)
        {
            const StanPoint *p=&g_CurrentStan.tiles[t].points[v];
            assert(p->y==20 && p->x==xyz[t][v][0]*2 && p->z==xyz[t][v][2]*2);
            assert(p->link==before.tiles[t].points[v].link);
        }
        unsigned char *saved; DWORD size; StanFile loaded={0};
        assert(StanPrepareSave(&g_CurrentStan,&saved,&size,&why));
        assert(StanLoadNative(saved,size,.5f,&loaded,&why));
        for (DWORD t=0; t<loaded.tilecount; t++) for (int v=0; v<4; v++) { assert(loaded.tiles[t].points[v].y==20); }
        free(saved); StanFileFree(&loaded);
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        assert(g_CurrentStan.size==before.size && !memcmp(before.data,g_CurrentStan.data,before.size));
        assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
        assert(g_CurrentStan.tiles[1].points[2].y==20);
        assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    }
    /* Empty collision files remain forbidden; rejecting the edit is atomic. */
    tool=EDITOR_TOOL_FACE_SELECT; assert(!Enter(3,"0","0","unused",TRUE));
    assert(g_EditHistory.undocount==0 && g_CurrentStan.size==before.size);
    assert(!memcmp(before.data,g_CurrentStan.data,before.size));
    /* A collapsed tile is removed when other collision geometry survives. */
    stancount=4; assert(Enter(3,"0","0","unused",TRUE));
    assert(g_CurrentStan.tilecount==1 && g_EditHistory.undocount==1);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(!memcmp(before.data,g_CurrentStan.data,before.size));
    EditHistoryFree(&g_EditHistory); StanFileFree(&g_CurrentStan); StanFileFree(&before); selectingstan=FALSE;
    puts("PASS: linked stan vertices/edges/faces, native persistence, collapsed-tile cleanup, empty-level rejection and undo/redo.");
}
int main(void)
{
    ResetScale(0,0,0); assert(ScalingValid(&currentScale));
    currentScale.factor[0]=0; assert(ScalingGeometryValid(&currentScale) && !ScalingValid(&currentScale));
    currentScale.factor[0]=-1; assert(!ScalingGeometryValid(&currentScale));
    currentScale.factor[0]=INFINITY; assert(!ScalingGeometryValid(&currentScale));
    assert(!ScalingValid(NULL) && !ScalingGeometryValid(NULL));
    Background(); Stans(); assert(refreshes>0); return 0;
}
