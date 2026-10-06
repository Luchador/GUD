#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include <src/doorshadowformat.h>
#include "roommanage.h"
#include "roomedit.h"
#include "bgcommands.h"
#include "bghistory.h"
#include "doorshadow.h"
#include "setupmeta.h"
#define OK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s; %s)\n",__LINE__,#x,why,reason); abort(); } } while (0)
static const char *why="";
static char reason[512];
static DWORD packedsize;
#include "fixture.inc"

static void Globals(SetupFile *s,BOOL reference)
{
    unsigned char data[128]={0}; RomFile rom={0};
    rom.data=data; rom.size=sizeof(data); rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,128,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x4149474c,16,32,0};
    Put(data+16,0x80000030);Put(data+20,1);Put(data+24,8);Put(data+28,1);
    Put(data+48,0x80000050);Put(data+52,2);
    if (reference) { data[80]=0x44;data[84]=4; } else data[80]=4;
    OK(SetupFileSetGlobalReferences(s,&rom,&why)); OK(s->globalrefs);
}
static void Script(SetupFile *s,const unsigned char *bytes,DWORD count)
{
    DWORD at=(s->size+3)&~3u;unsigned char *data=realloc(s->data,at+16+count);OK(data);
    memset(data+s->size,0,at-s->size);
    s->data=data;s->size=at+16+count;memset(data+at,0,16);
    Put(data+20,at);Put(data+at,at+16);Put(data+at+4,0x401);memcpy(data+at+16,bytes,count);
}
static void Vis(BgDocument *bg,unsigned opcode,DWORD a,DWORD b)
{
    free(bg->viscommands);bg->viscommandsloaded=TRUE;
    DWORD args=BgVisArgumentType(opcode)==BG_VIS_ROOM_RANGE?2:1;
    bg->viscommandssize=(args+2)*8;bg->viscommands=calloc(1,bg->viscommandssize);OK(bg->viscommands);
    bg->viscommands[0]=opcode;bg->viscommands[1]=args+1;
    bg->viscommands[8]=0x65;Put(bg->viscommands+12,a);
    if (args==2) { bg->viscommands[16]=0x65;Put(bg->viscommands+20,b); }
}
static void TileRoom(StanFile *stan,DWORD index,DWORD room)
{
    DWORD changed;
    OK(StanSetTileRooms(stan,&index,1,room,3,&changed,&why));
}
static void Blocked(BgDocument *bg,BgFile *source,SetupFile *s,StanFile *stan,DWORD room,const char *message)
{
    DWORD count=bg->roomcount,faces=bg->facecount;BOOL dirty=bg->dirty;
    OK(!RoomManageRemove(bg,source,s,stan,room,reason,sizeof(reason)));
    if (!strstr(reason,message)) { fprintf(stderr,"Expected %s: %s\n",message,reason);abort(); }
    OK(count==bg->roomcount && faces==bg->facecount && dirty==bg->dirty);
}
static BgFile RoundTrip(const BgDocument *bg,const BgFile *source,BOOL project)
{
    BgFile compiled={0},packed={0};BgDocument loaded={0};BgVisProgram a={0},b={0};
    OK(project?BgDocumentCompileProject(bg,source,&compiled,&why):BgDocumentCompile(bg,source,&compiled,&why));
    OK(BgFileCompact(&compiled,&packed,&why));OK(BgFileValidateVertexBatches(&packed,&why));
    OK(BgDocumentLoad(packed.data,packed.size,bg->levelscale,&loaded,&why));
    OK(loaded.roomcount==bg->roomcount && loaded.facecount==bg->facecount);
    for (DWORD r=1;r<=bg->roomcount;r++) {
        OK(loaded.rooms[r].facecount==bg->rooms[r].facecount);
        OK(!memcmp(loaded.rooms[r].origin,bg->rooms[r].origin,sizeof(bg->rooms[r].origin)));
        if (!bg->rooms[r].facecount && bg->rooms[r].vertexcount) OK(loaded.rooms[r].vertexcount);
        for (DWORD f=0;f<loaded.rooms[r].facecount;f++) OK(loaded.rooms[r].faces[f].room==r);
    }
    OK(loaded.portals.portalcount==bg->portals.portalcount);
    for (DWORD p=0;p<bg->portals.portalcount;p++) {
        OK(loaded.portals.portals[p].connectedroom1==bg->portals.portals[p].connectedroom1);
        OK(loaded.portals.portals[p].connectedroom2==bg->portals.portals[p].connectedroom2);
        OK(!memcmp(loaded.portals.portals[p].nativepoints,bg->portals.portals[p].nativepoints,sizeof(BgPortalPoint)*bg->portals.portals[p].pointcount));
    }
    OK(BgVisDecodeDocument(source,bg,&a,&why));OK(BgVisDecodeDocument(&packed,&loaded,&b,&why));
    OK(a.complete && b.complete && !b.warnings && a.count==b.count);
    for (DWORD i=0;i<a.count;i++) {
        OK(a.instructions[i].opcode==b.instructions[i].opcode);
        if (a.instructions[i].argument==BG_VIS_ONE_PORTAL) OK(a.instructions[i].portal==b.instructions[i].portal);
        else OK(!memcmp(a.instructions[i].arg,b.instructions[i].arg,sizeof(a.instructions[i].arg)));
    }
    /* The editor retains the un-compacted source for live portal identities
     * and history; only the disk/ROM copy gets relocated by compaction. */
    packedsize=packed.size;
    BgVisFree(&a);BgVisFree(&b);BgDocumentFree(&loaded);BgFileFree(&packed);return compiled;
}
static void Lifecycle(const char *dir)
{
    BgFile source=Fixture(),saved={0};BgDocument bg={0};SetupFile s={0};StanFile stan=Floor();
    DWORD created,shadow;BOOL changed;EditHistory h={0};EditHistoryTransaction tx={0};EditHistoryAsset asset;
    OK(BgDocumentLoad(source.data,source.size,.5f,&bg,&why));
    OK(SetupLoadProjectFile(dir,"UsetuptestZ",&s,&why));Globals(&s,FALSE);
    OK(RoomManageAdd(&bg,&created,&why));OK(created==2);
    OK(RoomManageAdd(&bg,&created,&why));OK(created==3);
    saved=RoundTrip(&bg,&source,FALSE);BgFileFree(&saved);
    /* A real shadow and background geometry will be renumbered together. */
    BgFaceRef refs[2]={{.room=1,.faceid=bg.rooms[1].faces[0].id},{.room=1,.faceid=bg.rooms[1].faces[1].id}};
    OK(DoorShadowCreate(&bg,&s,refs,&shadow,&why));
    bg.rooms[1].facecount=1;bg.facecount=1;
    refs[0].faceid=bg.rooms[1].faces[0].id;
    OK(BgDocumentMoveFacesToRoom(&bg,refs,1,3,&changed,&why));OK(changed);
    bg.portals.portalcount=1;bg.portals.portals=calloc(1,sizeof(BgPortal));OK(bg.portals.portals);
    BgPortal *portal=bg.portals.portals;
    portal->geometryoffset=BG_PORTAL_NEW_GEOMETRY|1;portal->pointcount=3;
    portal->connectedroom1=1;portal->connectedroom2=3;
    for (int v=0;v<3;v++) {
        portal->nativepoints[v]=(BgPortalPoint){v*10,0,v==2?10:0};
        portal->points[v]=(BgPortalPoint){v*20,0,v==2?20:0};
    }
    Blocked(&bg,&source,&s,&stan,0,"valid room");
    Blocked(&bg,&source,&s,&stan,4,"valid room");
    const double place[3]={250,5,50};SetupPadRef pad,bound;
    OK(SetupFileAddPad(&s,.5f,place,"p101a",&pad,&why));OK(pad.index==0);
    Blocked(&bg,&source,&s,&stan,2,"ordinary pad 0");
    DWORD door;const double facing[3]={0,0,1};
    OK(SetupFileAddDoor(&s,1,.5f,place,facing,&door,&why));OK(SetupFileGetModelPad(&s,door,&bound));
    OK(SetupFileSetPadStanName(&s,&bound,"p101a",&why));
    s.pads[pad.index].deleted=TRUE;
    Blocked(&bg,&source,&s,&stan,2,"bound pad");
    const unsigned char roomscript[]={0x44,0,0,0,4},aimscript[]={0x14,0,8,0,0,0,4};
    Script(&s,roomscript,sizeof(roomscript));Blocked(&bg,&source,&s,&stan,2,"Action Block 0x0401");
    Script(&s,aimscript,sizeof(aimscript));Blocked(&bg,&source,&s,&stan,2,"Action Block 0x0401");
    Put(s.data+20,0);Globals(&s,TRUE);
    Blocked(&bg,&source,&s,&stan,2,"shared Action Block");Globals(&s,FALSE);
    s.pads[pad.index].deleted=FALSE;
    /* Same tile identity, new room: pads continue following their STAN tile. */
    TileRoom(&stan,1,3);
    Vis(&bg,0x20,2,0);Blocked(&bg,&source,&s,&stan,2,"visibility command");
    for (DWORD first=1;first<=2;first++) for (DWORD last=2;last<=3;last++) {
        Vis(&bg,0x25,first,last);Blocked(&bg,&source,&s,&stan,2,"visibility command");
    }
    bg.viscommands[0]=0xfe;Blocked(&bg,&source,&s,&stan,2,"warnings");
    Vis(&bg,0x20,3,0);
    portal->connectedroom2=2;Blocked(&bg,&source,&s,&stan,2,"portal 0");portal->connectedroom2=3;
    unsigned char *shadowroom=s.data+s.objects[shadow].sourceoffset+DOOR_SHADOW_ROOM;
    Put(shadowroom,2);Blocked(&bg,&source,&s,&stan,2,"door shadow");Put(shadowroom,3);
    TileRoom(&stan,1,2);Blocked(&bg,&source,&s,&stan,2,"pad");
    s.pads[pad.index].deleted=s.boundpads[bound.index].pad.deleted=TRUE;
    Blocked(&bg,&source,&s,&stan,2,"STAN tiles");
    TileRoom(&stan,1,3);s.pads[pad.index].deleted=s.boundpads[bound.index].pad.deleted=FALSE;
    /* Remove the empty middle room, then save all three assets and undo. */
    EditHistoryReset(&h,&bg,&s,&stan);
    OK(EditHistoryBeginRoomEdit(&h,&bg,&s,&stan,"Remove Room",&tx,&why));
    OK(RoomManageRemove(&bg,&source,&s,&stan,2,reason,sizeof(reason)));
    OK(EditHistoryCommitEdit(&h,&bg,&s,&stan,&tx,&why));
    OK(bg.roomcount==2 && bg.rooms[2].facecount==1 && bg.rooms[2].faces[0].room==2);
    OK(bg.rooms[2].vertices[0].room==2 && bg.portals.portals[0].connectedroom2==2);
    OK(stan.tiles[1].room==2 && stan.tiles[2].room==2);
    OK(RoomEditPadRoom(&stan,&s.pads[pad.index],.5f)==2);
    OK(SetupMetaRead32(s.data+s.objects[shadow].sourceoffset+DOOR_SHADOW_ROOM)==2);
    OK(SetupMetaRead32(bg.viscommands+12)==2);
    saved=RoundTrip(&bg,&source,TRUE);
    unsigned char *bytes=NULL;DWORD size;StanFile reloaded={0};
    OK(StanPrepareSave(&stan,&bytes,&size,&why));OK(StanLoadNative(bytes,size,.5f,&reloaded,&why));
    OK(RoomEditPadRoom(&reloaded,&s.pads[pad.index],.5f)==2);free(bytes);StanFileFree(&reloaded);
    OK(SetupSaveProjectFile(dir,&s,&why));
    SetupFile reloadsetup={0};OK(SetupLoadProjectFile(dir,s.name,&reloadsetup,&why));
    OK(SetupMetaRead32(reloadsetup.data+reloadsetup.objects[shadow].sourceoffset+DOOR_SHADOW_ROOM)==2);
    SetupFileFree(&reloadsetup);
    EditHistoryMarkBgSaved(&h,&bg);EditHistoryMarkSetupSaved(&h,&s);EditHistoryMarkStanSaved(&h,&stan);
    OK(EditHistoryUndo(&h,&bg,&s,&stan,&asset,&why));OK(bg.roomcount==3 && stan.tiles[2].room==3);
    BgFile undo=RoundTrip(&bg,&saved,TRUE);BgFileFree(&undo);
    OK(EditHistoryRedo(&h,&bg,&s,&stan,&asset,&why));OK(bg.roomcount==2 && !bg.dirty && !s.dirty && !stan.dirty);
    Blocked(&bg,&saved,&s,&stan,2,"pad");
    EditHistoryFree(&h);BgFileFree(&saved);BgFileFree(&source);BgDocumentFree(&bg);SetupFileFree(&s);StanFileFree(&stan);
    puts("PASS room reference guards, middle removal, room-ID remapping, native BG/setup/STAN persistence and combined undo/redo after save");
}
static void LimitsAndRanges(const char *dir)
{
    BgFile source=Fixture();BgDocument bg={0};SetupFile s={0};StanFile stan={0};DWORD room;
    OK(BgDocumentLoad(source.data,source.size,1,&bg,&why));
    /* Load an empty setup again; the lifecycle test saved placements to its copy. */
    s.size=40;s.data=calloc(s.size,1);Globals(&s,FALSE);
    Blocked(&bg,&source,&s,&stan,1,"at least one");
    while(bg.roomcount<BG_MAX_ROOM) OK(RoomManageAdd(&bg,&room,&why));
    OK(!RoomManageAdd(&bg,&room,&why));
    BgFile packed=RoundTrip(&bg,&source,FALSE);BgFileFree(&packed);
    Vis(&bg,0x25,100,138);
    OK(RoomManageRemove(&bg,&source,&s,&stan,2,reason,sizeof(reason)));
    OK(SetupMetaRead32(bg.viscommands+12)==99 && SetupMetaRead32(bg.viscommands+20)==137);
    packed=RoundTrip(&bg,&source,FALSE);BgFileFree(&packed);
    Blocked(&bg,&source,&s,&stan,1,"background faces");
    BgDocumentFree(&bg);BgFileFree(&source);SetupFileFree(&s);
    puts("PASS reserved/last room guards, 138-room limit and inclusive visibility range remapping");
}
static void Corpus(const char *path)
{
    FILE *f=fopen(path,"rb");OK(f);OK(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    BgFile source={0};source.size=(DWORD)size;source.data=malloc(size);OK(source.data);
    OK(fread(source.data,1,size,f)==(size_t)size);fclose(f);
    BgDocument bg={0};OK(BgDocumentLoad(source.data,source.size,1,&bg,&why));
    DWORD original=bg.roomcount,faces=bg.facecount,room;BOOL changed;
    OK(RoomManageAdd(&bg,&room,&why));OK(room==original+1);
    DWORD from=1;while(from<original && !bg.rooms[from].facecount)from++;
    memcpy(bg.rooms[room].origin,bg.rooms[from].origin,sizeof(bg.rooms[room].origin));
    BgFaceRef ref={.room=from,.faceid=bg.rooms[from].faces[0].id,.layer=bg.rooms[from].faces[0].layer};
    OK(BgDocumentMoveFacesToRoom(&bg,&ref,1,room,&changed,&why));OK(changed && bg.facecount==faces);
    BgFile saved=RoundTrip(&bg,&source,TRUE);BgFileFree(&source);source=saved;
    SetupFile s={0};StanFile stan={0};s.size=40;s.data=calloc(1,s.size);Globals(&s,FALSE);
    Blocked(&bg,&source,&s,&stan,room,"background faces");
    ref.room=room;OK(BgDocumentMoveFacesToRoom(&bg,&ref,1,from,&changed,&why));OK(changed);
    OK(RoomManageRemove(&bg,&source,&s,&stan,room,reason,sizeof(reason)));
    saved=RoundTrip(&bg,&source,FALSE);BgFileFree(&source);source=saved;
    DWORD stable=packedsize;
    for (int repeat=0;repeat<3;repeat++) {
        OK(RoomManageAdd(&bg,&room,&why));saved=RoundTrip(&bg,&source,TRUE);BgFileFree(&source);source=saved;
        OK(RoomManageRemove(&bg,&source,&s,&stan,room,reason,sizeof(reason)));
        saved=RoundTrip(&bg,&source,FALSE);BgFileFree(&source);source=saved;
        OK(packedsize==stable);
    }
    OK(bg.roomcount==original && bg.facecount==faces);
    printf("PASS %s: create room %lu, move geometry, save/reload, remove and repeat without file growth\n",path,(unsigned long)room);
    BgDocumentFree(&bg);BgFileFree(&source);SetupFileFree(&s);
}
int main(int argc,char **argv)
{
    setbuf(stdout,NULL);
    assert(argc>=2);Lifecycle(argv[1]);LimitsAndRanges(argv[1]);
    for (int i=2;i<argc;i++) { Corpus(argv[i]); }
    return 0;
}
