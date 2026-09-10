#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"
#include "bghistory.h"

/* This suite uses only the BG history branch. */
void SetupFileFree(SetupFile *setup) { (void)setup; abort(); }
void StanFileFree(StanFile *stan) { (void)stan; abort(); }
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Float(unsigned char *p,float value) { DWORD bits;memcpy(&bits,&value,4);Put(p,bits); }
static void SamePortals(const BgDocument *a,const BgDocument *b)
{
    assert(a->portals.portalcount==b->portals.portalcount);
    assert(!memcmp(a->portals.portals,b->portals.portals,a->portals.portalcount*sizeof(BgPortal)));
}
static BgFile Fixture(void)
{
    BgFile bg={0};unsigned char *d;
    bg.size=672;bg.data=calloc(bg.size,1);assert(bg.data);d=bg.data;strcpy(bg.name,"bg/bg_portal_test.seg");
    Put(d+4,0x0e000020);Put(d+8,0x0e0000a0);
    Put(d+56,0x0e000200);Put(d+60,0x0e000240); /* room 1 */
    Put(d+84,0x0e000270);Put(d+108,0x0e000280); /* empty rooms 2/3 */
    for(int i=0;i<3;i++)
    {
        unsigned char *p=d+160+i*8;
        Put(p,i==2 ? 0x0e000100 : 0x0e0000c0);
        p[4]=i==2 ? 2 : 1;p[5]=i==1 ? 3 : 2;
        p[6]=0x82+i;p[7]=0x32+i; /* opaque flags and differing margins */
    }
    /* Preserve opaque header/visibility bytes near the geometry streams. */
    for(int i=416;i<508;i++) { d[i]=(unsigned char)i; }
    for(int i=0;i<2;i++)
    {
        const float points[4][2]={{-5,-5},{5,-5},{5,5},{-5,5}};
        unsigned char *p=d+192+i*64;p[0]=4;
        for(int v=0;v<4;v++) { Float(p+4+v*12,points[v][0]);Float(p+8+v*12,points[v][1]);Float(p+12+v*12,10+i*10); }
    }
    Put(d+508,48);Put(d+572,24);
    d[512+16+1]=10;d[512+32+3]=10;
    for(int v=0;v<3;v++) { memset(d+512+v*16+12,255,4); }
    Put(d+576,0x04200030);Put(d+580,0);
    Put(d+584,0xbf000000);Put(d+588,0x00000a14);Put(d+592,0xb8000000);
    Put(d+620,8);Put(d+624,0xb8000000);Put(d+636,8);Put(d+640,0xb8000000);
    return bg;
}
static void CompileAndSave(const BgDocument *doc,const BgFile *source,const char *dir,BgFile *compiled)
{
    const char *why="";BgFile saved={0};BgDocument reloaded={0};char path[MAX_PATH];
    assert(BgDocumentCompile(doc,source,compiled,&why));
    assert(BgFileValidateVertexBatches(compiled,&why));
    for(DWORD i=152;i<508;i++)
    {
        /* The first portal's two room bytes are the only metadata edits. */
        if(i!=164 && i!=165) { assert(compiled->data[i]==source->data[i]); }
    }
    assert(compiled->data[164]==doc->portals.portals[0].connectedroom1);
    assert(compiled->data[165]==doc->portals.portals[0].connectedroom2);
    snprintf(path,sizeof(path),"%s/bg",dir);CreateDirectory(path,NULL);
    assert(BgSaveProjectFile(dir,compiled,&why));
    assert(BgLoadProjectFile(dir,compiled->name,&saved,&why));
    assert(saved.size==compiled->size && !memcmp(saved.data,compiled->data,saved.size));
    assert(BgDocumentLoad(saved.data,saved.size,.5f,&reloaded,&why));SamePortals(doc,&reloaded);
    assert(reloaded.facecount==doc->facecount && reloaded.roomcount==3);
    BgDocumentFree(&reloaded);BgFileFree(&saved);
}
int main(int argc,char **argv)
{
    BgFile source=Fixture(),compiled={0},restored={0};BgDocument doc={0},original={0},after={0};
    EditHistory history={0};EditHistoryTransaction transaction={0};EditHistoryAsset asset;
    SetupFile setup={0};StanFile stan={0};const char *why="";BOOL changed;
    assert(argc==2);assert(BgDocumentLoad(source.data,source.size,.5f,&doc,&why));
    assert(!doc.portalwarning && doc.portals.portalcount==3 && doc.roomcount==3 && doc.facecount==1);
    assert(doc.portals.portals[0].points[0].z==20);
    assert(BgDocumentClone(&doc,&original,&why));assert(doc.portals.portals!=original.portals.portals);
    EditHistoryReset(&history,&doc,&setup,&stan);
    assert(BgDocumentSetPortalRooms(&doc,0,1,2,&changed,&why)&&!changed&&!doc.dirty);
    assert(!BgDocumentSetPortalRooms(&doc,0,2,2,&changed,&why)&&!changed&&why[0]);
    assert(!BgDocumentSetPortalRooms(&doc,0,0,2,&changed,&why));
    assert(!BgDocumentSetPortalRooms(&doc,0,1,4,&changed,&why));
    assert(!BgDocumentSetPortalRooms(&doc,0,256,2,&changed,&why));
    assert(!BgDocumentSetPortalRooms(&doc,3,1,2,&changed,&why));SamePortals(&doc,&original);assert(!doc.dirty);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Change Portal Rooms",&transaction,&why));
    assert(BgDocumentSetPortalRooms(&doc,0,3,1,&changed,&why)&&changed&&doc.dirty);
    assert(!memcmp(&doc.portals.portals[1],&original.portals.portals[1],2*sizeof(BgPortal)));
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    assert(BgDocumentClone(&doc,&after,&why));CompileAndSave(&doc,&source,argv[1],&compiled);
    EditHistoryMarkBgSaved(&history,&doc);assert(!doc.dirty);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why));assert(asset==EDIT_HISTORY_ASSET_BG && doc.dirty);
    SamePortals(&doc,&original);
    /* Undo must override the now-saved source, not revive its edited rooms. */
    CompileAndSave(&doc,&compiled,argv[1],&restored);BgFileFree(&restored);
    assert(EditHistoryRedo(&history,&doc,&setup,&stan,&asset,&why));SamePortals(&doc,&after);assert(!doc.dirty);
    assert(EditHistoryBeginBgEdit(&history,&doc,"Swap Portal Rooms",&transaction,&why));
    assert(BgDocumentSetPortalRooms(&doc,0,1,3,&changed,&why)&&changed);
    EditHistoryRollbackEdit(&transaction,&doc,&setup,&stan);SamePortals(&doc,&after);assert(!doc.dirty);
    /* Geometry edits/clones must retain prior connection edits. */
    assert(EditHistoryBeginBgEdit(&history,&doc,"Delete Face",&transaction,&why));
    { BgFaceRef face={doc.rooms[1].faces[0].id,1,0,0};DWORD deleted;
      assert(BgDocumentDeleteFaces(&doc,&face,1,&deleted,&why)&&deleted==1); }
    assert(EditHistoryCommitEdit(&history,&doc,&setup,&stan,&transaction,&why));
    CompileAndSave(&doc,&compiled,argv[1],&restored);BgFileFree(&restored);
    assert(EditHistoryUndo(&history,&doc,&setup,&stan,&asset,&why));SamePortals(&doc,&after);
    assert(doc.facecount==1);EditHistoryFree(&history);
    BgFileFree(&compiled);BgDocumentFree(&doc);BgDocumentFree(&after);BgDocumentFree(&original);
    /* Corrupt portals keep their original nonfatal warning/opaque-save behavior. */
    Put(source.data+160,0x0e7fffff);
    assert(BgDocumentLoad(source.data,source.size,.5f,&doc,&why)&&doc.portalwarning);
    assert(!doc.portals.portalcount && !BgDocumentSetPortalRooms(&doc,0,1,2,&changed,&why));
    assert(BgDocumentCompile(&doc,&source,&compiled,&why));assert(Get(compiled.data+160)==0x0e7fffff);
    BgFileFree(&compiled);BgDocumentFree(&doc);BgFileFree(&source);
    puts("PASS: native portal rooms, validation, shared geometry, exact metadata preservation, save/reload, undo after save, rollback and mixed BG edits.");
    return 0;
}
