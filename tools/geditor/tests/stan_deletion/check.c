#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stanload.h"
#include "bghistory.h"
#include "edittool.h"
#include "project.h"
#include "setupmeta.h"

void BgDocumentFree(BgDocument *doc) { abort(); }
BOOL SetupFileCompact(SetupFile *setup, const char **why) { (void)setup; (void)why; abort(); }
void SetupFileFree(SetupFile *setup) { abort(); }
static int failafter=-1;
void *__real_malloc(size_t); void *__real_calloc(size_t,size_t);
static BOOL Fail(void) { if(failafter<0)return FALSE; if(!failafter)return TRUE; failafter--;return FALSE; }
void *__wrap_malloc(size_t n) { return Fail() ? NULL : __real_malloc(n); }
void *__wrap_calloc(size_t n,size_t s) { return Fail() ? NULL : __real_calloc(n,s); }
static char g_RomExportError[256];
#include "export.inc"
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Put16(unsigned char *p,unsigned short v) { p[0]=v>>8;p[1]=v; }
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static StanFile Fixture(const char *dir)
{
    StanFile raw={0},stan={0}; const char *why="";
    const DWORD sizes[4]={32,40,32,48}; DWORD offsets[4],end=20;
    for(int i=0;i<4;i++) { offsets[i]=end;end+=sizes[i]; }
    raw.size=end+32;raw.data=calloc(raw.size,1);assert(raw.data);strcpy(raw.name,"Tbg_delete_test_stanZ");
    Put(raw.data,0x12345678); /* Opaque first word. */
    Put(raw.data+4,0x0e000000|offsets[0]); Put(raw.data+8,0x0e000000|offsets[2]); Put(raw.data+12,0x0e000000|offsets[3]);
    for(int i=0;i<4;i++) {
        unsigned char *p=raw.data+offsets[i];DWORD count=sizes[i]/8-1;
        Put(p,((100u+i)<<16)|(i<2 ? 1 : 2));Put16(p+4,(i<<12)|0x345);Put16(p+6,(count<<12)|0x012);
        const short points[5][3]={{0,0,0},{20,0,0},{20,0,20},{0,0,20},{-10,0,10}};
        for(DWORD k=0;k<count;k++) {
            unsigned char *v=p+8+k*8;
            Put16(v,points[k][0]+i*30);Put16(v+2,points[k][1]+10);Put16(v+4,points[k][2]);
            unsigned short link=k==2 ? 0xf : 0x10+(offsets[k==0 ? (i+1)%4 : k==1 ? (i+3)%4 : 0]-20)/8;
            Put16(v+6,link);
        }
    }
    memcpy(raw.data+end+8,"unstric",8); memset(raw.data+end+16,0x5a,16);
    assert(StanSaveProjectFile(dir,&raw,&why));assert(StanLoadProjectFile(dir,raw.name,.25f,&stan,&why));free(raw.data);
    return stan;
}
static void Same(const StanFile *a,const StanFile *b)
{
    assert(a->size==b->size && a->tilecount==b->tilecount && a->dirty==b->dirty && a->levelscale==b->levelscale);
    assert(!memcmp(a->data,b->data,a->size));
    /* Loading a saved file starts a new visibility session. Native content
     * must round-trip exactly; temporary editor identities need not. */
    for(DWORD t=0;t<a->tilecount;t++) {
        StanTile tile=b->tiles[t];tile.editorid=a->tiles[t].editorid;
        assert(!memcmp(&a->tiles[t],&tile,sizeof(tile)));
    }
}
static void CheckLinks(const StanFile *before,const StanFile *after,const DWORD *selected,DWORD count)
{
    unsigned char *removed=calloc(before->tilecount,1);DWORD *map=malloc(before->tilecount*sizeof(*map));assert(removed&&map);
    for(DWORD i=0;i<count;i++)removed[selected[i]]=1;
    DWORD next=0,bytes=0;
    for(DWORD i=0;i<before->tilecount;i++) {
        map[i]=removed[i] ? STAN_TILE_NONE : next++;
        if(removed[i])bytes+=8+before->tiles[i].pointcount*8;
    }
    assert(after->tilecount==next && after->size==before->size-bytes);
    for(DWORD i=0;i<before->tilecount;i++) {
        if(removed[i])continue;
        const StanTile *old=&before->tiles[i],*tile=&after->tiles[map[i]];
        StanTile expected=*old;expected.sourceoffset=tile->sourceoffset;
        for(DWORD p=0;p<tile->pointcount;p++) {
            unsigned short link=old->points[p].link;
            if(link>=0x10) {
                DWORD target=StanLinkedTile(before,link);assert(target!=STAN_TILE_NONE);
                assert(StanLinkedTile(after,tile->points[p].link)==map[target]);
                if(removed[target])assert(!tile->points[p].link);
                expected.points[p].link=tile->points[p].link;
            } else assert(link==tile->points[p].link);
            assert(!memcmp(before->data+old->sourceoffset+8+p*8,after->data+tile->sourceoffset+8+p*8,6));
        }
        assert(!memcmp(&expected,tile,sizeof(expected)));
        assert(!memcmp(before->data+old->sourceoffset,after->data+tile->sourceoffset,8));
    }
    DWORD oldend=before->tiles[before->tilecount-1].sourceoffset+8+before->tiles[before->tilecount-1].pointcount*8;
    DWORD newend=after->tiles[after->tilecount-1].sourceoffset+8+after->tiles[after->tilecount-1].pointcount*8;
    assert(!memcmp(before->data+oldend,after->data+newend,before->size-oldend));
    assert(Get(after->data)==Get(before->data));
    DWORD first=after->tiles[0].sourceoffset;assert((Get(after->data+4)&0xffffff)==first);
    for(DWORD p=4;p<first-4;p+=4) {
        DWORD pointer=Get(after->data+p),found=STAN_TILE_NONE;
        assert((pointer&0xff000000)==(Get(before->data+p)&0xff000000));
        for(DWORD t=0;t<after->tilecount;t++)if(after->tiles[t].sourceoffset==(pointer&0xffffff))found=t;
        assert(found!=STAN_TILE_NONE);
    }
    assert(!Get(after->data+first-4));free(map);free(removed);
}
static void Persist(const char *dir,const StanFile *stan)
{
    const char *why="";StanFile loaded={0};GEditorProject project={0};char path[MAX_PATH];DWORD size;
    assert(StanSaveProjectFile(dir,stan,&why));assert(StanLoadProjectFile(dir,stan->name,stan->levelscale,&loaded,&why));
    loaded.dirty=stan->dirty;Same(stan,&loaded);StanFileFree(&loaded);
    strcpy(project.dir,dir);assert(RomExportProjectResourcePath(&project,stan->name,path,sizeof(path))==1);
    unsigned char *resource=RomExportReadResource(path,stan->name,&size,&why);
    assert(resource && size==stan->size && !memcmp(resource,stan->data,size));free(resource);
}

typedef void *HWND;
#define MB_ICONERROR 1
#define GEDITOR_TITLE "test"
typedef struct ViewportState {
    EditorTool tool; BOOL showstan; int stanopacity,stancomponentcount;
    StanFile stan; unsigned char *stanselected;
} ViewportState;
static ViewportState view;
static HWND g_Viewport=&view;
static BgDocument g_CurrentBgDocument;
static SetupFile g_CurrentSetup;
static StanFile g_CurrentStan;
static EditHistory g_EditHistory;
static BOOL flying,transforming,failrebuild;
static int errors,restores,rebuilds;
static ViewportState *ViewportGetState(HWND hwnd) { return hwnd; }
#include "selection.inc"
static EditorTool ViewportGetTool(HWND hwnd) { return view.tool; }
static BOOL ViewportIsFlying(HWND hwnd) { return flying; }
static BOOL ViewportIsTransforming(HWND hwnd) { return transforming; }
static BOOL GEditorReloadCurrentObjectsAndViewport(const char **why)
{
    rebuilds++; view.stan=g_CurrentStan; memset(view.stanselected,0,4);
    if(failrebuild) { failrebuild=FALSE;*why="Rebuild failed.";return FALSE; }
    return TRUE;
}
static void GEditorRestoreHistorySelection(HWND hwnd)
{ restores++;assert(g_EditHistory.selectionsize==4);memcpy(view.stanselected,g_EditHistory.selection,4); }
static void GEditorRefreshSelectionDetails(void) {}
static void GEditorRefreshHistoryMenu(HWND hwnd) {}
static void MessageBox(HWND hwnd,const char *why,const char *title,unsigned flags) { assert(why[0]);errors++; }
#include "controller.inc"
static void Controller(const StanFile *source,const char *dir)
{
    const char *why="";DWORD ids[4];unsigned char selected[4]={0,1,0,1};
    assert(StanFileClone(source,&g_CurrentStan,&why));
    view=(ViewportState){EDITOR_TOOL_FACE_SELECT,TRUE,44,0,g_CurrentStan,selected};
    assert(ViewportGetSelectedStanTiles(&view,ids,2) && ids[0]==1 && ids[1]==3);
    assert(!ViewportGetSelectedStanTiles(&view,ids,1));view.showstan=FALSE;
    assert(!ViewportGetSelectedStanTiles(&view,ids,2));view.showstan=TRUE;view.stanopacity=0;
    assert(!ViewportGetSelectedStanTiles(&view,ids,2));view.stanopacity=44;
    view.tool=EDITOR_TOOL_EDGE_SELECT;view.stancomponentcount=2;
    assert(!ViewportGetSelectedStanTiles(&view,ids,2));assert(!GEditorDeleteSelectedStanTiles(NULL));view.tool=EDITOR_TOOL_FACE_SELECT;
    flying=TRUE;assert(!GEditorDeleteSelectedStanTiles(NULL));flying=FALSE;
    transforming=TRUE;assert(!GEditorDeleteSelectedStanTiles(NULL));transforming=FALSE;
    EditHistoryReset(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan);
    assert(EditHistorySetSelection(&g_EditHistory,selected,4,FALSE,&why));
    for(int failure=0;failure<2;failure++) {
        ULONGLONG next=g_EditHistory.nextrevision;
        failrebuild=failure==0;if(failure==1)g_EditHistory.nextrevision=0;
        assert(!GEditorDeleteSelectedStanTiles(NULL));g_EditHistory.nextrevision=next;
        Same(source,&g_CurrentStan);assert(selected[1] && selected[3] && !g_EditHistory.undocount);
    }
    assert(errors==2 && restores==2);
    assert(GEditorDeleteSelectedStanTiles(NULL) && g_CurrentStan.tilecount==2);
    assert(!ViewportGetStanSelectionCount(&view,NULL));
    assert(EditHistorySetSelection(&g_EditHistory,selected,4,FALSE,&why));
    assert(g_EditHistory.undocount==1 && !strcmp(EditHistoryGetUndoAction(&g_EditHistory),"Delete Stan Tiles"));
    Persist(dir,&g_CurrentStan);EditHistoryMarkStanSaved(&g_EditHistory,&g_CurrentStan);
    assert(EditHistoryUndo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(g_CurrentStan.tilecount==4 && g_CurrentStan.dirty);
    GEditorReloadCurrentObjectsAndViewport(&why);GEditorRestoreHistorySelection(NULL);assert(selected[1] && selected[3]);
    assert(EditHistoryRedo(&g_EditHistory,&g_CurrentBgDocument,&g_CurrentSetup,&g_CurrentStan,NULL,&why));
    assert(g_CurrentStan.tilecount==2 && !g_CurrentStan.dirty);
    GEditorReloadCurrentObjectsAndViewport(&why);GEditorRestoreHistorySelection(NULL);assert(!ViewportGetStanSelectionCount(&view,NULL));
    EditHistoryFree(&g_EditHistory);StanFileFree(&g_CurrentStan);
}
int main(int argc,char **argv)
{
    assert(argc==2);const char *why="";StanFile original=Fixture(argv[1]),work={0};DWORD ids[4],deleted;
    for(unsigned mask=1;mask<15;mask++) {
        DWORD n=0;for(DWORD t=0;t<4;t++)if(mask&(1u<<t))ids[n++]=t;
        assert(StanFileClone(&original,&work,&why));
        assert(StanDeleteTiles(&work,ids,n,&deleted,&why) && deleted==n && work.dirty);
        CheckLinks(&original,&work,ids,n);Persist(argv[1],&work);StanFileFree(&work);
    }
    assert(StanFileClone(&original,&work,&why));
    DWORD all[4]={0,1,2,3},bad[2]={0,4},duplicate[2]={1,1};
    assert(!StanDeleteTiles(&work,all,4,&deleted,&why) && !deleted);Same(&original,&work);
    assert(!StanDeleteTiles(&work,bad,2,&deleted,&why));Same(&original,&work);
    assert(!StanDeleteTiles(&work,duplicate,2,&deleted,&why));Same(&original,&work);
    /* A malformed surviving link or header must leave the complete file alone. */
    work.tiles[0].points[0].link=0xffff;StanFile corrupt={0};assert(StanFileClone(&work,&corrupt,&why));ids[0]=1;
    assert(!StanDeleteTiles(&work,ids,1,&deleted,&why));Same(&work,&corrupt);StanFileFree(&work);StanFileFree(&corrupt);
    for(int attempt=0;attempt<10;attempt++) {
        assert(StanFileClone(&original,&work,&why));failafter=attempt;
        BOOL ok=StanDeleteTiles(&work,ids,1,&deleted,&why);failafter=-1;
        if(!ok) { assert(!deleted);Same(&original,&work); }
        StanFileFree(&work);if(ok) { assert(attempt==4);break; }
    }
    Controller(&original,argv[1]);StanFileFree(&original);
    /* Real Depot data: mixed first/interior/final removals retain every link target ID. */
    assert(StanLoadProjectFile(argv[1],"Tbg_depo_all_p_stanZ",.21847887f,&original,&why));assert(original.tilecount>1000);
    ids[0]=0;ids[1]=original.tilecount/2;ids[2]=original.tilecount-1;
    assert(StanFileClone(&original,&work,&why));
    assert(StanDeleteTiles(&work,ids,3,&deleted,&why));CheckLinks(&original,&work,ids,3);Persist(argv[1],&work);
    StanFileFree(&work);StanFileFree(&original);
    puts("PASS: multi-tile deletion, all survivor combinations, native links/header/footer, final-tile guard, allocation rollback, viewport selection, controller failures, undo/redo after save, ROM resource bytes and real Depot stan data.");
}
