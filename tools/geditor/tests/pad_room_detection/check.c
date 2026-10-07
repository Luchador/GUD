#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupstan.h"
#include "bghistory.h"
#include "setupmeta.h"

static const char *why;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); abort(); } }
void BgDocumentFree(BgDocument *bg) { assert(!bg->rooms); }
static void Put16(unsigned char *p, unsigned v) { p[0]=v>>8; p[1]=v; }
static StanFile Floors(float scale)
{
    unsigned char bytes[160] = {0}; StanFile s = {0};
    SetupMetaWrite32(bytes + 4, 12);
    for (DWORD i = 0; i < 3; i++)
    {
        DWORD at = 12 + i * 32;
        SetupMetaWrite32(bytes + at, ((i ? i : 3) << 16) | (i == 1 ? 42 : 9));
        bytes[at + 6] = 0x30; bytes[at + 7] = 0x12;
        for (DWORD p = 0; p < 3; p++)
        {
            DWORD point = at + 8 + 8 * p;
            Put16(bytes + point, (p == 2 ? 100 : 0) + (i ? 0 : 500));
            Put16(bytes + point + 2, i == 2 ? 100 : 0);
            Put16(bytes + point + 4, p == 1 ? 100 : 0);
        }
    }
    memcpy(bytes + 116, "unstric", 8);
    Require(StanLoadNative(bytes, sizeof(bytes), scale, &s, &why));
    return s;
}
static void SamePadPose(const SetupPad *a, const SetupPad *b)
{
    assert(!memcmp(a->pos,b->pos,sizeof(a->pos)));
    assert(!memcmp(a->up,b->up,sizeof(a->up)));
    assert(!memcmp(a->look,b->look,sizeof(a->look)));
    assert(a->deleted==b->deleted && a->occluder==b->occluder);
}
static void Pads(const char *dir, float scale, BOOL bound)
{
    SetupFile setup={0}, loaded={0}; StanFile stan=Floors(scale), saved={0};
    BgDocument bg={0}; EditHistory history={0}; EditHistoryTransaction tx={0}; EditHistoryAsset asset;
    SetupPadRef ref={0,bound}; BOOL changed; DWORD count; unsigned char *bytes; DWORD size;
    Require(SetupLoadProjectFile(dir,"UsetuppadsZ",&setup,&why));
    SetupPad before=bound ? setup.boundpads[0].pad : setup.pads[0];
    SetupPad other=setup.pads[1];
    /* Splitting BG rooms requires corresponding collision tiles to be assigned.
     * Native names stay fixed while this tile moves from old room 42 to new 60. */
    Require(StanSetTileRooms(&stan,(DWORD[]){1},1,60,60,&count,&why)); assert(count==1);
    float pos[3];for(int i=0;i<3;i++)pos[i]=before.pos[i]/scale;
    assert(stan.tiles[StanResolvePadTile(&stan,before.stanname,pos)].room==9);
    EditHistoryReset(&history,NULL,&setup,NULL);
    Require(EditHistoryBeginSetupEdit(&history,&setup,"Detect Pad Room",&tx,&why));
    Require(SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why)); assert(changed);
    SetupPad *pad=bound ? &setup.boundpads[0].pad : &setup.pads[0];
    assert(!strcmp(pad->stanname,"p1a")); SamePadPose(&before,pad);
    assert(!memcmp(&other,&setup.pads[1],sizeof(other)));
    assert(stan.tiles[StanResolvePadTile(&stan,pad->stanname,pos)].room==60);
    Require(EditHistoryCommitEdit(&history,&bg,&setup,&stan,&tx,&why));
    Require(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));
    assert(!strcmp((bound ? setup.boundpads[0].pad : setup.pads[0]).stanname,"p2a"));
    Require(EditHistoryRedo(&history,&bg,&setup,&stan,&asset,&why));
    Require(SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why)); assert(!changed);
    /* This is the actual save path, including STAN regrouping and setup refresh. */
    SetupStanRefresh stats;
    Require(SetupSaveProjectFileWithStan(dir,&setup,&stan,&stats,&why)); assert(!stats.unresolved);
    Require(SetupLoadProjectFile(dir,"UsetuppadsZ",&loaded,&why));
    Require(StanPrepareSave(&stan,&bytes,&size,&why));
    Require(StanLoadNative(bytes,size,scale,&saved,&why)); free(bytes);
    pad=bound ? &loaded.boundpads[0].pad : &loaded.pads[0]; SamePadPose(&before,pad);
    assert(saved.tiles[StanResolvePadTile(&saved,pad->stanname,pos)].room==60);
    if(bound) assert(!memcmp(&setup.boundpads[0].xmin,&loaded.boundpads[0].xmin,6*sizeof(float)));
    /* Invalid/absent coverage cannot silently bind a disconnected nearby room. */
    unsigned char *snapshot=malloc(setup.size); memcpy(snapshot,setup.data,setup.size);DWORD oldsize=setup.size;
    pad=bound ? &setup.boundpads[0].pad : &setup.pads[0]; pad->pos[0]=1000;
    assert(!SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why) && !changed);
    assert(setup.size==oldsize&&!memcmp(snapshot,setup.data,oldsize));pad->pos[0]=before.pos[0];
    pad->pos[0]=NAN; assert(!SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why));pad->pos[0]=before.pos[0];
    pad->deleted=TRUE;assert(!SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why));pad->deleted=FALSE;
    if(bound){pad->occluder=TRUE;assert(!SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why));pad->occluder=FALSE;}
    /* Duplicate IDs that change first-match order during room grouping fail
     * atomically, even if the live named-tile fast path could use the floor. */
    stan.tiles[2].id=stan.tiles[1].id;
    SetupMetaWrite32(stan.data+stan.tiles[2].sourceoffset,(stan.tiles[2].id<<8)|stan.tiles[2].room);
    assert(!SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why));
    assert(setup.size==oldsize&&!memcmp(snapshot,setup.data,oldsize));free(snapshot);
    /* Restore original disk setup for independent ordinary/bound/scale cases. */
    Require(EditHistoryUndo(&history,&bg,&setup,&stan,&asset,&why));
    Require(SetupSaveProjectFile(dir,&setup,&why));
    EditHistoryFree(&history); SetupFileFree(&setup); SetupFileFree(&loaded);
    StanFileFree(&stan);StanFileFree(&saved);
}
static void Frigate(const char *dir)
{
    char path[1024];snprintf(path,sizeof(path),"%s/frigate.stan",dir);
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);long size=ftell(f);rewind(f);
    unsigned char *bytes=malloc(size);assert(fread(bytes,1,size,f)==(size_t)size);fclose(f);
    StanFile stan={0};SetupFile setup={0};SetupPadRef ref={145,FALSE};BOOL changed;
    Require(StanLoadNative(bytes,size,1,&stan,&why));free(bytes);
    Require(SetupLoadProjectFile(dir,"UsetupdestZ",&setup,&why));
    assert(!strcmp(setup.pads[145].stanname,"p2919h1"));
    Require(SetupDetectPadRoom(&setup,&stan,&ref,&changed,&why));assert(!changed);
    DWORD tile=StanResolvePadTile(&stan,setup.pads[145].stanname,setup.pads[145].pos);
    assert(tile<stan.tilecount&&stan.tiles[tile].room==55);
    SetupFileFree(&setup);StanFileFree(&stan);
    puts("PASS: last uploaded Frigate pad 145 remains p2919h1, room 55.");
}
static void DisconnectedNearestFloor(void)
{
    StanTile tiles[2] = {
        {.id=0x100,.room=60,.sourceoffset=12,.pointcount=3,.extreme={0,1,2},
         .points={{0,0,0,0},{0,0,1000,0},{1000,0,0,0}}},
        {.id=0x200,.room=42,.sourceoffset=44,.pointcount=3,.extreme={0,1,2},
         .points={{15,200,0,0},{15,200,100,0},{115,200,0,0}}}
    };
    StanFile stan={.tiles=tiles,.tilecount=2,.levelscale=1};
    const float pos[3]={10,200,10};char name[16];
    assert(StanResolvePadTile(&stan,"missing",pos)==STAN_TILE_NONE);
    assert(StanDetectPadRoomName(&stan,pos,name)&&!strcmp(name,"p1a"));
    assert(stan.tiles[StanResolvePadTile(&stan,name,pos)].room==60);
}
int main(int argc,char **argv)
{
    assert(argc>=2); DisconnectedNearestFloor();
    Pads(argv[1],1,FALSE);Pads(argv[1],1,TRUE);Pads(argv[1],.25f,FALSE);Pads(argv[1],.25f,TRUE);
    if(argc>2)Frigate(argv[1]);
    puts("PASS: new room 60, stacked floors, ordinary/bound pads, native scale, save/reload, undo/redo, missing coverage and ambiguous names.");
}
