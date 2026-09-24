#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "roomedit.h"
#include "bghistory.h"
#include "doorshadow.h"
#include "setupmeta.h"
#define OK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s (%s)\n",__FILE__,__LINE__,#x,why); abort(); } } while (0)
static const char *why = "";
#include "fixture.inc"
static void Put16(unsigned char *p, unsigned short n) { p[0]=n>>8; p[1]=n; }

static StanFile Floor(void)
{
    /* The second room shares endpoints and reciprocal links; it must not move.
     * The third floor is directly above room 1, testing authored floor names. */
    unsigned char data[12 + 3 * 40 + 32] = {0};
    Put(data + 4, 0x0e00000c);
    for (int t = 0; t < 3; t++)
    {
        unsigned char *tile = data + 12 + t * 40;
        Put(tile, ((100u+t)<<16) | (t+1)); Put16(tile+4,0xfff); Put16(tile+6,0x4012);
        const short p[4][2] = {{0,0},{0,100},{100,100},{100,0}};
        for (int i = 0; i < 4; i++)
        {
            Put16(tile+8+i*8,p[i][0]+(t==1 ? 100 : 0));
            Put16(tile+10+i*8,t==2 ? 200 : 0);
            Put16(tile+12+i*8,p[i][1]);
        }
        if (t==0) Put16(tile+8+2*8+6,0x15);
        if (t==1) Put16(tile+8+0*8+6,0x10);
    }
    memcpy(data + 12 + 120 + 8, "unstric", 8);
    StanFile stan = {0}; OK(StanLoadNative(data,sizeof(data),.5f,&stan,&why));
    strcpy(stan.name,"Tbg_room_test_stanZ"); return stan;
}
static void Close(float a, double b) { assert(fabs(a-b)<.001); }


#include "viewport.c"

int main(int argc, char **argv)
{
    assert(argc==2);
    BgFile source=Fixture(), compiled={0}; BgDocument bg={0}, savedbg={0};
    SetupFile setup={0}, savedsetup={0}; StanFile stan=Floor(), savedstan={0};
    EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    DWORD prop, guard, door, other, shadow; BOOL changed;
    const double place[3]={50,5,50}, facing[3]={0,0,1}, upstairs[3]={50,405,50};
    OK(BgDocumentLoad(source.data,source.size,.5f,&bg,&why));
    OK(SetupLoadProjectFile(argv[1],"UsetuptestZ",&setup,&why));
    OK(SetupFileAddModel(&setup,FALSE,1,.5f,place,&prop,&why));
    OK(SetupFileAddModel(&setup,TRUE,1,.5f,place,&guard,&why));
    OK(SetupFileAddDoor(&setup,1,.5f,place,facing,&door,&why));
    OK(SetupFileAddModel(&setup,FALSE,1,.5f,upstairs,&other,&why));
    SetupPadRef guardpad, proppad, doorpad, otherpad, patrol;
    OK(SetupFileGetModelPad(&setup,prop,&proppad)); OK(SetupFileGetModelPad(&setup,guard,&guardpad));
    OK(SetupFileGetModelPad(&setup,door,&doorpad)); OK(SetupFileGetModelPad(&setup,other,&otherpad));
    OK(SetupFileSetPadStanName(&setup,&proppad,"p100a",&why));
    OK(SetupFileSetPadStanName(&setup,&guardpad,"p100a",&why));
    OK(SetupFileSetPadStanName(&setup,&doorpad,"p100a",&why));
    OK(SetupFileSetPadStanName(&setup,&otherpad,"p102a",&why));
    OK(SetupFileAddPad(&setup,.5f,place,"p100a",&patrol,&why));
    assert(RoomEditObjectRoom(&setup,&stan,.5f,prop)==1);
    assert(RoomEditObjectRoom(&setup,&stan,.5f,guard)==1);
    assert(RoomEditObjectRoom(&setup,&stan,.5f,door)==1);
    assert(RoomEditObjectRoom(&setup,&stan,.5f,other)==3);
    /* Two props share a placement pad. No new pads or placement flags may be introduced. */
    DWORD shared;
    OK(SetupFileAddModel(&setup,FALSE,1,.5f,place,&shared,&why));
    unsigned char *record=setup.data+setup.objects[shared].sourceoffset;
    Put16(record+6,(unsigned short)setup.objects[prop].pad);
    setup.objects[shared].pad=setup.objects[prop].pad;
    BgFaceRef refs[2]={{.faceid=bg.rooms[1].faces[0].id,.room=1}, {.faceid=bg.rooms[1].faces[1].id,.room=1}};
    OK(DoorShadowCreate(&bg,&setup,refs,&shadow,&why));
    /* Keep a BG triangle as well as the converted shadow. */
    bg.rooms[1].facecount=1; bg.facecount=1;
    bg.rooms[1].faces[0].id=123;
    bg.portals.portalcount=3; bg.portals.portals=calloc(3,sizeof(BgPortal)); assert(bg.portals.portals);
    for (int i=0;i<3;i++)
    {
        BgPortal *p=&bg.portals.portals[i]; p->geometryoffset=BG_PORTAL_NEW_GEOMETRY | (1+(i==2)); p->pointcount=3;
        p->connectedroom1=i==0 ? 1 : 2; p->connectedroom2=3;
        for (int v=0;v<3;v++) p->nativepoints[v]=p->points[v]=(BgPortalPoint){v*10,0,v==2 ? 10 : 0};
    }
    /* Native portal geometry is at half world scale. */
    for (int i=0;i<3;i++) for (int v=0;v<3;v++) {
        bg.portals.portals[i].nativepoints[v].x *= .5f; bg.portals.portals[i].nativepoints[v].z *= .5f;
    }
    DWORD padcount=setup.padcount, boundcount=setup.boundpadcount;
    BgDocumentVertex original=bg.rooms[1].vertices[0];
    StanTile neighbor=stan.tiles[1], upper=stan.tiles[2];
    unsigned char flags[8]; memcpy(flags,setup.data+setup.objects[prop].sourceoffset+8,8);
    EditHistoryReset(&history,&bg,&setup,&stan);
    OK(EditHistoryBeginRoomEdit(&history,&bg,&setup,&stan,"Move Room",&tx,&why));
    const double offset[3]={0,21,0}; /* 10.5 native -> 11, so all assets move 22 world units. */
    OK(RoomEditTranslate(&bg,&setup,&stan,1,offset,&changed,&why)); assert(changed);
    OK(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    assert(bg.dirty && setup.dirty && stan.dirty && history.undocount==1);
    Close(bg.rooms[1].origin[1],11); assert(!memcmp(&original,bg.rooms[1].vertices,sizeof(original)));
    Close(stan.tiles[0].points[0].y,22);
    assert(!memcmp(&neighbor,&stan.tiles[1],sizeof(neighbor)) && !memcmp(&upper,&stan.tiles[2],sizeof(upper)));
    Close(setup.pads[proppad.index].pos[1],13.5); Close(setup.pads[guardpad.index].pos[1],13.5);
    Close(setup.pads[otherpad.index].pos[1],202.5); assert(setup.padcount==padcount && setup.boundpadcount==boundcount);
    assert(!strcmp(setup.pads[proppad.index].stanname,"p100a"));
    assert(!memcmp(flags,setup.data+setup.objects[prop].sourceoffset+8,8));
    Close(bg.portals.portals[0].points[0].y,22); Close(bg.portals.portals[1].points[0].y,22);
    Close(bg.portals.portals[2].points[0].y,0);
    BgVertex vertices[18]; unsigned short tag; BgRenderFlags render;
    OK(DoorShadowBuildPreview(&setup,shadow,.5f,vertices,&tag,&render,&why)); Close(vertices[0].y,22);
    ViewportTest(&setup,&stan,&bg,prop,other);
    OK(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); assert(asset==EDIT_HISTORY_ASSET_ROOM);
    Close(bg.rooms[1].origin[1],0); Close(stan.tiles[0].points[0].y,0); Close(setup.pads[proppad.index].pos[1],2.5);
    assert(!bg.dirty && !setup.dirty && !stan.dirty);
    OK(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)); Close(bg.rooms[1].origin[1],11);
    Close(stan.tiles[0].points[0].y,22); Close(setup.pads[proppad.index].pos[1],13.5);
    /* Failure after a partial mutation is rolled back as one operation. */
    OK(EditHistoryBeginRoomEdit(&history,&bg,&setup,&stan,"Bad Move",&tx,&why));
    double bad[3]={0,65534,0};
    assert(!RoomEditTranslate(&bg,&setup,&stan,1,bad,&changed,&why));
    EditHistoryRollbackEdit(&tx,&bg,&setup,&stan);
    Close(bg.rooms[1].origin[1],11); Close(stan.tiles[0].points[0].y,22); Close(setup.pads[proppad.index].pos[1],13.5);
    /* A late commit failure restores all assets, including pads already moved. */
    OK(EditHistoryBeginRoomEdit(&history,&bg,&setup,&stan,"Late Failure",&tx,&why));
    const double sideways[3]={6,0,-4};
    OK(RoomEditTranslate(&bg,&setup,&stan,1,sideways,&changed,&why));
    Close(bg.rooms[1].origin[0],3); Close(setup.pads[proppad.index].pos[0],28);
    ULONGLONG revision=history.nextrevision; history.nextrevision=0;
    assert(!EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why)); history.nextrevision=revision;
    EditHistoryRollbackEdit(&tx,&bg,&setup,&stan);
    Close(bg.rooms[1].origin[0],0); Close(setup.pads[proppad.index].pos[0],25);
    /* Every file's saved state and subsequent Undo/Redo revision stays independent. */
    OK(SetupSaveProjectFile(argv[1],&setup,&why));
    OK(SetupLoadProjectFile(argv[1],setup.name,&savedsetup,&why));
    Close(savedsetup.pads[proppad.index].pos[1],13.5);
    OK(StanSaveProjectFile(argv[1],&stan,&why));
    OK(StanLoadProjectFile(argv[1],stan.name,.5f,&savedstan,&why)); Close(savedstan.tiles[0].points[0].y,22);
    OK(BgDocumentCompile(&bg,&source,&compiled,&why));
    OK(BgDocumentLoad(compiled.data,compiled.size,.5f,&savedbg,&why)); Close(savedbg.rooms[1].origin[1],11);
    EditHistoryMarkBgSaved(&history,&bg); EditHistoryMarkSetupSaved(&history,&setup); EditHistoryMarkStanSaved(&history,&stan);
    OK(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why)); assert(bg.dirty && setup.dirty && stan.dirty);
    OK(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why)); assert(!bg.dirty && !setup.dirty && !stan.dirty);
    EditHistoryFree(&history); BgDocumentFree(&bg); BgDocumentFree(&savedbg);
    SetupFileFree(&setup); SetupFileFree(&savedsetup); StanFileFree(&stan); StanFileFree(&savedstan);
    BgFileFree(&source); BgFileFree(&compiled);
    puts("PASS room move: native rounding, linked/stacked rooms, shared pads, door shadow, portals, save/reload, Undo/Redo and rollback");
    return 0;
}
