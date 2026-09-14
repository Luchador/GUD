#include <assert.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
#include "edittool.h"
void SetupFileFree(SetupFile *setup) { abort(); }
void StanFileFree(StanFile *stan) { abort(); }
static BOOL failallocation;
static void *TestRealloc(void *p, size_t size) { return failallocation ? NULL : realloc(p,size); }
#define realloc TestRealloc
#include "bgportal.c"
#undef realloc
#include "fixture.inc"
#include "options.inc"

static void SamePortals(const BgDocument *a, const BgDocument *b, DWORD oldcount)
{
    assert(a->portals.portalcount == b->portals.portalcount);
    for (DWORD i = 0; i < a->portals.portalcount; i++)
    {
        const BgPortal *p=&a->portals.portals[i], *q=&b->portals.portals[i];
        if (i < oldcount) { assert(!memcmp(p,q,sizeof(*p))); }
        else
        {
            assert(p->connectedroom1==q->connectedroom1 && p->connectedroom2==q->connectedroom2);
            assert(p->pointcount==4 && q->pointcount==4 && !q->controlbytes1 && !q->controlbytes2);
            for (int c=0;c<4;c++)
            {
                assert(fabs(p->points[c].x-q->points[c].x)<.001);
                assert(fabs(p->points[c].y-q->points[c].y)<.001);
                assert(fabs(p->points[c].z-q->points[c].z)<.001);
            }
        }
    }
}
static BgFile SaveReload(const BgDocument *doc, const BgFile *source, const char *dir, DWORD originals)
{
    BgFile compiled={0}, saved={0}; BgDocument reloaded={0}; const char *why="";
    assert(BgDocumentCompile(doc,source,&compiled,&why));
    assert(BgFileValidateVertexBatches(&compiled,&why)); /* Create ROM's BG gate. */
    assert(BgSaveProjectFile(dir,&compiled,&why));
    assert(BgLoadProjectFile(dir,compiled.name,&saved,&why));
    assert(saved.size==compiled.size && !memcmp(saved.data,compiled.data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&reloaded,&why));
    assert(!reloaded.portalwarning && reloaded.facecount==doc->facecount);
    SamePortals(doc,&reloaded,originals);
    DWORD table=Get(compiled.data+8)&0xffffff;
    assert(Get(compiled.data+table+doc->portals.portalcount*8)==0);
    for(DWORD i=0;i<originals;i++)
    {
        DWORD oldtable=Get(source->data+8)&0xffffff;
        assert(Get(compiled.data+table+i*8)==Get(source->data+oldtable+i*8));
    }
    BgDocumentFree(&reloaded);BgFileFree(&saved);return compiled;
}
static void Geometry(void)
{
    BgFile source=Fixture();BgDocument doc={0};const char *why="";DWORD index;
    assert(BgDocumentLoad(source.data,source.size,.5f,&doc,&why));
    BgPortalPlacement p={1,3,{15,-25,35},200,300,BG_PORTAL_XY};
    BgPortal originals[3];memcpy(originals,doc.portals.portals,sizeof(originals));
    for(int plane=0;plane<3;plane++)
    {
        p.plane=(BgPortalPlane)plane;assert(BgDocumentAddPortal(&doc,&p,&index,&why));
        assert(index==(DWORD)(plane+3) && doc.portals.portals[index].geometryoffset==(BG_PORTAL_NEW_GEOMETRY|index));
        for(DWORD i=3;i<=index;i++) assert(doc.portals.portals[i].geometryoffset!=doc.portals.portals[i-1].geometryoffset);
        BgPortal *q=&doc.portals.portals[index];
        double x=0,y=0,z=0;
        for(int c=0;c<4;c++) { x+=q->points[c].x;y+=q->points[c].y;z+=q->points[c].z; }
        assert(x/4==15 && y/4==-25 && z/4==35);
        if(plane==BG_PORTAL_XY) assert(q->points[0].z==35 && q->points[2].x-q->points[0].x==200 && q->points[2].y-q->points[0].y==300);
        if(plane==BG_PORTAL_YZ) assert(q->points[0].x==15 && q->points[2].z-q->points[0].z==200 && q->points[2].y-q->points[0].y==300);
        if(plane==BG_PORTAL_XZ) assert(q->points[0].y==-25 && q->points[2].x-q->points[0].x==200 && q->points[2].z-q->points[0].z==300);
    }
    assert(!memcmp(originals,doc.portals.portals,sizeof(originals)) && doc.dirty);
    DWORD count=doc.portals.portalcount;BgPortal *before=doc.portals.portals;
    failallocation=TRUE;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));failallocation=FALSE;
    assert(doc.portals.portalcount==count && doc.portals.portals==before && index==BG_PORTAL_INDEX_NONE);
    p.room2=1;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.room2=0;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));
    p.room2=4;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.room2=256;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.room2=3;
    p.width=0;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.width=NAN;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));
    p.width=200;p.center[0]=INFINITY;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));
    p.center[0]=1e30;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.center[0]=15;
    p.width=p.height=1e25;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.width=200;p.height=300;
    p.plane=(BgPortalPlane)3;assert(!BgDocumentAddPortal(&doc,&p,&index,&why));p.plane=BG_PORTAL_XY;
    doc.portalwarning="bad table";assert(!BgDocumentAddPortal(&doc,&p,&index,&why));doc.portalwarning=NULL;
    assert(doc.portals.portalcount==count && !memcmp(originals,doc.portals.portals,sizeof(originals)));
    BgDocumentFree(&doc);BgFileFree(&source);
    double value=123;
    assert(PortalOptionsNumber(" 2.5 ",TRUE,&value) && value==250);
    assert(PortalOptionsNumber("-7.5",FALSE,&value) && value==-7.5);
    assert(!PortalOptionsNumber("nan",FALSE,&value) && !PortalOptionsNumber("inf",FALSE,&value));
    assert(!PortalOptionsNumber("0",TRUE,&value) && !PortalOptionsNumber("1m",TRUE,&value));
    puts("PASS: rectangular planes, room IDs, exact centers/sizes, unique identities, float limits and allocation failure.");
}
static void Persistence(const char *dir)
{
    BgFile source=Fixture(),saved={0},again={0},undone={0},redone={0};
    BgDocument doc={0},after={0};EditHistory h={0};EditHistoryTransaction tx={0};SetupFile setup={0};StanFile stan={0};
    const char *why="";DWORD index;BOOL changed;char path[MAX_PATH];
    snprintf(path,sizeof(path),"%s/bg",dir);assert(CreateDirectory(path,NULL));
    assert(BgDocumentLoad(source.data,source.size,.5f,&doc,&why));
    /* A real visibility-stream pointer to an original polygon must stay valid. */
    Put(source.data+12,0x0f0001a0);Put(source.data+416,0x1f020000);Put(source.data+420,0);
    Put(source.data+424,0x04010000);Put(source.data+428,0x0f0000c0);Put(source.data+432,0x00010000);Put(source.data+436,0);
    EditHistoryReset(&h,&doc,&setup,&stan);
    BgPortalPlacement p={1,3,{15,-25,35},200,300,BG_PORTAL_XY};
    assert(EditHistoryBeginBgEdit(&h,&doc,"Add Portal",&tx,&why));
    assert(BgDocumentAddPortal(&doc,&p,&index,&why) && index==3);
    assert(EditHistoryCommitEdit(&h,&doc,&setup,&stan,&tx,&why));
    saved=SaveReload(&doc,&source,dir,3);EditHistoryMarkBgSaved(&h,&doc);
    /* Existing portal metadata, polygons, opaque bytes and visibility commands. */
    assert(!memcmp(source.data+152,saved.data+152,508-152));
    assert(Get(saved.data+12)==Get(source.data+12));
    again=SaveReload(&doc,&saved,dir,3);assert(again.size==saved.size && !memcmp(again.data,saved.data,saved.size));BgFileFree(&again);
    assert(EditHistoryUndo(&h,&doc,&setup,&stan,NULL,&why));assert(doc.portals.portalcount==3 && doc.dirty);
    undone=SaveReload(&doc,&saved,dir,3);EditHistoryMarkBgSaved(&h,&doc);
    assert(EditHistoryRedo(&h,&doc,&setup,&stan,NULL,&why));assert(doc.portals.portalcount==4 && doc.dirty);
    redone=SaveReload(&doc,&undone,dir,3);
    assert(BgDocumentSetPortalRooms(&doc,3,2,1,&changed,&why)&&changed);
    again=SaveReload(&doc,&redone,dir,3);assert(again.size==redone.size);
    /* A fresh load replaces temporary identities with real native offsets. */
    assert(BgDocumentLoad(again.data,again.size,.5f,&after,&why));
    assert(!(after.portals.portals[3].geometryoffset & BG_PORTAL_NEW_GEOMETRY));
    p.room1=2;p.room2=3;p.center[0]=150;p.plane=BG_PORTAL_XZ;
    assert(BgDocumentAddPortal(&after,&p,&index,&why) && index==4);
    BgFile fifth=SaveReload(&after,&again,dir,4);BgFileFree(&fifth);BgDocumentFree(&after);
    /* Undo and branch into a different rectangle at the same appended index. */
    assert(EditHistoryUndo(&h,&doc,&setup,&stan,NULL,&why));
    assert(BgDocumentAddPortal(&doc,&p,&index,&why) && index==3);
    BgFile branch=SaveReload(&doc,&again,dir,3);assert(branch.size==again.size);BgFileFree(&branch);
    EditHistoryFree(&h);BgDocumentFree(&doc);BgFileFree(&saved);BgFileFree(&again);BgFileFree(&undone);BgFileFree(&redone);
    /* Empty tables can grow, and every slot up to the native limit can save. */
    Put(source.data+8,0x0e000090); /* An existing eight-byte zero sentinel. */
    assert(BgDocumentLoad(source.data,source.size,.3f,&doc,&why));assert(!doc.portalwarning && !doc.portals.portalcount);
    for(DWORD i=0;i<BG_MAX_PORTALS-1;i++) { assert(BgDocumentAddPortal(&doc,&p,&index,&why) && index==i); }
    assert(!BgDocumentAddPortal(&doc,&p,&index,&why) && doc.portals.portalcount==199);
    saved=SaveReload(&doc,&source,dir,0);again=SaveReload(&doc,&saved,dir,0);
    assert(saved.size==again.size && !memcmp(saved.data,again.data,saved.size));
    BgFileFree(&again);BgFileFree(&saved);BgDocumentFree(&doc);BgFileFree(&source);
    puts("PASS: native compile/save/reload, stable existing pointers/scripts, repeat-save size, undo-save-redo, branch replacement, fresh-load additions and 199-portal limit.");
}

typedef void *HWND;
typedef struct { long x,y; } POINT;
#include "browser.h"
#include "portaloptions.h"
#define GEDITOR_NO_LEVEL ((DWORD)-1)
#define GEDITOR_TITLE "GEditor"
#define MB_ICONERROR 1
static HWND g_Viewport=(HWND)1,g_RightPanel=(HWND)2,g_ToolToolbar=(HWND)3;
static DWORD g_CurrentLevelIndex;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL outside, cancelprompt, faildisplay, failcommit, visible;
static DWORD overlaycount, selectedportal=BG_PORTAL_INDEX_NONE;
static unsigned prompts, errors, restores;
static EditorTool tool=EDITOR_TOOL_VERTEX_SELECT;
static HWND WindowFromPoint(POINT point) { return outside ? NULL : g_Viewport; }
static BOOL ViewportGetPrimitiveDrop(HWND hwnd, POINT point, double *position, double *right, DWORD *room)
{ position[0]=100;position[1]=200;position[2]=300;right[0]=1;right[1]=right[2]=0;*room=1;return TRUE; }
BOOL PortalOptionsPrompt(HWND hwnd, DWORD rooms, BgPortalPlacement *p)
{
    prompts++;assert(rooms==3 && p->room1==1 && !p->room2 && p->center[0]==100 && p->center[1]==200 && p->center[2]==300);
    assert(p->width==200 && p->height==300 && p->plane==BG_PORTAL_XY);
    if(cancelprompt) return FALSE;
    p->room2=3;return TRUE;
}
static void ViewportSetPortals(HWND hwnd,const BgPortalFile *p) { overlaycount=faildisplay?0:p->portalcount; }
static void RightPanelShowPortals(HWND hwnd) { visible=TRUE; }
static void ViewportSetTool(HWND hwnd,EditorTool t) { tool=t; }
static BOOL ViewportSelectPortal(HWND hwnd,DWORD index)
{ assert(visible && tool==EDITOR_TOOL_FACE_SELECT);if(index>=overlaycount)return FALSE;selectedportal=index;return TRUE; }
static void ToolToolbarSetTool(HWND hwnd,EditorTool t) { assert(t==tool); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void SetFocus(HWND hwnd) {}
static void GEditorRestoreHistorySelection(HWND hwnd) { restores++;selectedportal=BG_PORTAL_INDEX_NONE;tool=EDITOR_TOOL_VERTEX_SELECT; }
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags) { assert(text && text[0]);errors++;return 0; }
static BOOL Commit(EditHistory *h,BgDocument *b,SetupFile *s,StanFile *st,EditHistoryTransaction *tx,const char **why)
{ if(failcommit) { *why="commit failure";return FALSE; }return EditHistoryCommitEdit(h,b,s,st,tx,why); }
#define EditHistoryCommitEdit Commit
#include "editor.inc"
#undef EditHistoryCommitEdit
static void Commands(void)
{
    BgFile source=Fixture();const char *why="";BrowserObjectDrop drop={BROWSER_OBJECT_PORTAL,{100,200}};
    assert(BgDocumentLoad(source.data,source.size,.5f,&g_CurrentBgDocument,&why));
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    outside=TRUE;assert(!GEditorDropPortal(NULL,&drop)&&!prompts);outside=FALSE;
    g_CurrentLevelIndex=GEDITOR_NO_LEVEL;assert(!GEditorDropPortal(NULL,&drop)&&!prompts);g_CurrentLevelIndex=0;
    g_CurrentBgDocument.portalwarning="bad table";assert(!GEditorDropPortal(NULL,&drop)&&!prompts);g_CurrentBgDocument.portalwarning=NULL;
    cancelprompt=TRUE;assert(!GEditorDropPortal(NULL,&drop));cancelprompt=FALSE;
    assert(g_CurrentBgDocument.portals.portalcount==3&&!g_CurrentBgDocument.dirty&&!g_EditHistory.undocount&&!errors);
    failallocation=TRUE;assert(!GEditorDropPortal(NULL,&drop));failallocation=FALSE;
    assert(g_CurrentBgDocument.portals.portalcount==3&&!g_CurrentBgDocument.dirty&&!g_EditHistory.undocount);
    faildisplay=TRUE;assert(!GEditorDropPortal(NULL,&drop));faildisplay=FALSE;
    assert(g_CurrentBgDocument.portals.portalcount==3&&!g_CurrentBgDocument.dirty&&!g_EditHistory.undocount);
    failcommit=TRUE;assert(!GEditorDropPortal(NULL,&drop));failcommit=FALSE;
    assert(g_CurrentBgDocument.portals.portalcount==3&&!g_CurrentBgDocument.dirty&&!g_EditHistory.undocount);
    assert(errors==3&&restores==3);
    assert(GEditorDropPortal(NULL,&drop));
    assert(g_CurrentBgDocument.portals.portalcount==4&&g_CurrentBgDocument.dirty&&g_EditHistory.undocount==1);
    assert(selectedportal==3 && visible && tool==EDITOR_TOOL_FACE_SELECT && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Add Portal"));
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(g_CurrentBgDocument.portals.portalcount==3&&!g_CurrentBgDocument.dirty);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(g_CurrentBgDocument.portals.portalcount==4&&g_CurrentBgDocument.dirty);
    EditHistoryFree(&g_EditHistory);BgDocumentFree(&g_CurrentBgDocument);BgFileFree(&source);
    puts("PASS: actual drop controller, BG-only levels, Cancel, off-viewport drops, display/allocation/commit rollback, auto-selection and undo/redo.");
}
int main(int argc,char **argv) { assert(argc==2);Geometry();Persistence(argv[1]);Commands();return 0; }
