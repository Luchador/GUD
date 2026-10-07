#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"
typedef void *HWND;
typedef struct MSG MSG;
#include "portalproperties.h"

BOOL SetupFileCompact(SetupFile *s, const char **why) { abort(); }
void SetupFileFree(SetupFile *s) { abort(); }
void StanFileFree(StanFile *s) { abort(); }
#include "fixture.inc"

/* Exercise the real frame handler, including stale selection and rollback. */
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
static HWND g_Viewport;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static DWORD selected;
static BOOL failcommit;
static unsigned refreshed, errors;
static BOOL ViewportGetSelectedPortal(HWND hwnd,DWORD *index) { *index=selected;return selected!=BG_PORTAL_INDEX_NONE; }
static void ViewportCancelTransform(HWND hwnd) { }
static void ViewportSetPortals(HWND hwnd,const BgPortalFile *p) { refreshed++; }
static void GEditorRefreshHistoryMenu(HWND hwnd) { }
static int MessageBox(HWND hwnd,const char *text,const char *title,int flags) { errors++;return 0; }
static BOOL Commit(EditHistory *h,BgDocument *bg,SetupFile *s,StanFile *st,EditHistoryTransaction *tx,const char **why)
{
    if(failcommit){*why="injected failure";return FALSE;}
    return EditHistoryCommitEdit(h,bg,s,st,tx,why);
}
#define EditHistoryCommitEdit Commit
#include "controller.inc"
#undef EditHistoryCommitEdit

static void RoundTrip(const BgFile *source,const char *dir,BgFile *compiled)
{
    const char *why="";BgFile saved={0};BgDocument loaded={0};char path[MAX_PATH];
    const BgDocument *doc=&g_CurrentBgDocument;
    assert(BgDocumentCompile(doc,source,compiled,&why));
    assert(BgFileValidateVertexBatches(compiled,&why));
    for(DWORD i=152;i<508;i++)if(i!=166)assert(compiled->data[i]==source->data[i]);
    assert(compiled->data[166]==doc->portals.portals[0].controlbytes1);
    snprintf(path,sizeof(path),"%s/bg",dir);CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,compiled,&why));
    assert(BgLoadProjectFile(dir,compiled->name,&saved,&why));
    assert(BgDocumentLoad(saved.data,saved.size,doc->levelscale,&loaded,&why));
    assert(loaded.portals.portalcount==doc->portals.portalcount);
    for(DWORD i=0;i<loaded.portals.portalcount;i++) {
        BgPortal expected=doc->portals.portals[i];
        expected.geometryoffset=loaded.portals.portals[i].geometryoffset;
        assert(!memcmp(&expected,&loaded.portals.portals[i],sizeof(expected)));
    }
    assert(loaded.facecount==doc->facecount&&loaded.roomcount==doc->roomcount);
    BgDocumentFree(&loaded);BgFileFree(&saved);
}
int main(int argc,char **argv)
{
    assert(argc==2);const char *why="";BOOL changed;
    BgFile source=Fixture(),compiled={0},restored={0};
    BgDocument *doc=&g_CurrentBgDocument;
    assert(BgDocumentLoad(source.data,source.size,.5f,doc,&why));
    BgPortal original[3];memcpy(original,doc->portals.portals,sizeof(original));
    assert(!(original[0].controlbytes1&PORTALFLAG_FORCE_SIDE_CULL));
    assert(BgDocumentSetPortalSideCulling(doc,0,FALSE,&changed,&why)&&!changed&&!doc->dirty);
    assert(!BgDocumentSetPortalSideCulling(doc,3,TRUE,&changed,&why)&&!changed);
    assert(!BgDocumentSetPortalSideCulling(NULL,0,TRUE,&changed,&why)&&!changed);
    EditHistoryReset(&g_EditHistory,doc,&g_CurrentSetup,&g_CurrentStan);
    PortalPropertiesEdit edit={.portal=0,.sidecullingonly=TRUE,.sideculling=TRUE};
    selected=1;assert(!GEditorSetPortalProperty(NULL,&edit));
    selected=0;assert(GEditorSetPortalProperty(NULL,&edit));
    assert(refreshed==1&&doc->dirty);
    BgPortal expected=original[0];expected.controlbytes1|=PORTALFLAG_FORCE_SIDE_CULL;
    assert(!memcmp(&expected,&doc->portals.portals[0],sizeof(expected)));
    assert(!memcmp(original+1,doc->portals.portals+1,2*sizeof(BgPortal)));
    assert(GEditorSetPortalProperty(NULL,&edit)&&refreshed==1); /* No-op history. */
    RoundTrip(&source,argv[1],&compiled);EditHistoryMarkBgSaved(&g_EditHistory,doc);
    assert(GEditorSetPortalProperty(NULL,&edit)&&!doc->dirty);
    EditHistoryAsset asset;
    assert(EditHistoryUndo(&g_EditHistory,doc,&g_CurrentSetup,&g_CurrentStan,&asset,&why));
    assert(!memcmp(original,doc->portals.portals,sizeof(original)));
    RoundTrip(&compiled,argv[1],&restored);BgFileFree(&restored);
    assert(EditHistoryRedo(&g_EditHistory,doc,&g_CurrentSetup,&g_CurrentStan,&asset,&why));
    assert(!memcmp(&expected,&doc->portals.portals[0],sizeof(expected)));
    edit.sideculling=FALSE;failcommit=TRUE;
    assert(!GEditorSetPortalProperty(NULL,&edit)&&errors==1);
    assert(!memcmp(&expected,&doc->portals.portals[0],sizeof(expected)));
    failcommit=FALSE;assert(GEditorSetPortalProperty(NULL,&edit));
    assert(!memcmp(original,doc->portals.portals,sizeof(original)));
    EditHistoryFree(&g_EditHistory);BgDocumentFree(doc);BgFileFree(&source);BgFileFree(&compiled);
    puts("PASS: side-culling bit isolation, shared polygons, native save/reload, stale selection, no-op, undo/redo, rollback and disabling.");
}
