#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "roomstats.h"

typedef intptr_t LPARAM;
#define CALLBACK
static RoomStats g_RoomStats;
static int g_RoomSortColumn;
static BOOL g_RoomSortDescending;
#include "sort.inc"

static const char *why;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); abort(); } }
static long failafter = -1;
void *__real_malloc(size_t size);
void *__real_calloc(size_t count, size_t size);
void *__wrap_malloc(size_t size)
{ if (!failafter) { return NULL; } if (failafter > 0) { failafter--; } return __real_malloc(size); }
void *__wrap_calloc(size_t count, size_t size)
{ if (!failafter) { return NULL; } if (failafter > 0) { failafter--; } return __real_calloc(count, size); }
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static StanTile Tile(DWORD id, int room, float x, float y)
{
    StanTile t={.id=id,.room=room,.pointcount=3,.extreme={0,1,2}};
    t.points[0]=(StanPoint){x,y,0,0}; t.points[1]=(StanPoint){x,y,100,0}; t.points[2]=(StanPoint){x+100,y,0,0};
    return t;
}
static void Counts(const RoomStats *s,DWORD room,DWORD primary,DWORD secondary,DWORD stan,DWORD objects,DWORD characters)
{
    DWORD expected[]={primary,secondary,stan,objects,characters};
    assert(room<=s->roomcount && !memcmp(s->rooms[room].values,expected,sizeof(expected)));
}
static void Sum(const RoomStats *s)
{
    for(int column=0;column<ROOM_COUNT_COLUMNS;column++)
    {
        DWORD total=0;for(DWORD room=0;room<=s->roomcount;room++)total+=s->rooms[room].values[column];
        assert(total==s->total.values[column]);
    }
}
static void Synthetic(void)
{
    BgDocumentFace a[3]={{.layer=0},{.layer=0},{.layer=1}}, b[3]={{.layer=0},{.layer=1},{.layer=1}};
    BgDocumentRoom rooms[4]={[1]={.faces=a,.facecount=3},[2]={.faces=b,.facecount=3}};
    BgDocument bg={.rooms=rooms,.roomcount=3,.facecount=6,.levelscale=.5f};
    StanTile tiles[]={Tile(0x100,1,0,0),Tile(0x200,2,0,100),Tile(0x300,0,200,0),Tile(0x400,99,400,0)};
    StanFile stan={.tiles=tiles,.tilecount=4,.levelscale=.5f};
    SetupPad pads[6]={0};SetupBoundPad bounds[2]={0};
    for(int i=0;i<6;i++) { pads[i].pos[0]=pads[i].pos[2]=5;strcpy(pads[i].stanname,"p1a"); }
    pads[1].pos[1]=50;strcpy(pads[1].stanname,"p2a");
    pads[2]=pads[1];strcpy(pads[2].stanname,"obsolete"); /* geometric fallback keeps the correct floor */
    pads[3].pos[0]=10000; pads[4].deleted=TRUE; pads[5].pos[0]=NAN;
    bounds[0].pad=pads[1];bounds[1].pad=pads[0];
    SetupObject objects[10]={
        {.type=PROPDEF_PROP,.pad=0,.sourceoffset=40},
        {.type=PROPDEF_DOOR,.pad=0,.flags=PROPFLAG_ASSIGNEDTOCHR},
        {.type=PROPDEF_PROP,.pad=10001}, {.type=PROPDEF_PROP,.pad=0,.deleted=TRUE},
        {.type=PROPDEF_COLLECTABLE,.pad=1,.flags=PROPFLAG_ASSIGNEDTOCHR},
        {.type=PROPDEF_PROP,.pad=0,.flags=PROPFLAG_INSIDEANOTHEROBJ},
        {.type=PROPDEF_PROP,.pad=127}, {.type=PROPDEF_PROP,.pad=4}, {.type=PROPDEF_PROP,.pad=5},
        {.type=PROPDEF_MONITOR,.pad=-1,.sourceoffset=168}};
    SetupCharacter chars[4]={{.pad=1},{.pad=2},{.pad=0,.deleted=TRUE},{.pad=3}};
    unsigned char raw[428]={0};Put(raw+12,40);raw[43]=PROPDEF_PROP;raw[171]=PROPDEF_MONITOR;
    Put(raw+168+244,(DWORD)-1);raw[427]=48;
    SetupFile setup={.data=raw,.size=sizeof(raw),.pads=pads,.padcount=6,.boundpads=bounds,.boundpadcount=2,
        .objects=objects,.objectcount=10,.characters=chars,.charactercount=4};
    RoomStats s={0};
    SetupPad beforepads[6];StanTile beforetiles[4];unsigned char beforeraw[428];
    memcpy(beforepads,pads,sizeof(pads));memcpy(beforetiles,tiles,sizeof(tiles));memcpy(beforeraw,raw,sizeof(raw));
    Require(RoomStatsBuild(&bg,&setup,&stan,&s,&why));
    Counts(&s,1,2,1,1,3,0);Counts(&s,2,1,2,1,1,2);Counts(&s,3,0,0,0,0,0);Counts(&s,0,0,0,2,3,1);
    g_RoomStats=s;
    assert(LevelManagerCompareRooms(1,2,0)<0 && LevelManagerCompareRooms(0,1,0)>0);
    g_RoomSortColumn=ROOM_OBJECTS+1;g_RoomSortDescending=TRUE;
    assert(LevelManagerCompareRooms(1,2,0)<0 && LevelManagerCompareRooms(0,1,0)>0);
    g_RoomSortDescending=FALSE;assert(LevelManagerCompareRooms(1,2,0)>0);
    g_RoomSortColumn=ROOM_STAN_TILES+1;g_RoomSortDescending=TRUE;
    assert(LevelManagerCompareRooms(1,2,0)<0); /* Numeric room ID breaks equal-count ties. */
    g_RoomStats=(RoomStats){0};g_RoomSortColumn=0;g_RoomSortDescending=FALSE;
    Sum(&s);assert(s.total.values[ROOM_OBJECTS]==7 && s.total.values[ROOM_CHARACTERS]==3);
    assert(!memcmp(beforepads,pads,sizeof(pads)) && !memcmp(beforetiles,tiles,sizeof(tiles)) && !memcmp(beforeraw,raw,sizeof(raw)));
    assert(!bg.dirty && !setup.dirty && !stan.dirty); RoomStatsFree(&s);
    /* Current documents drive counts: changed layers, room assignments and pads. */
    a[0].layer=1;tiles[1].room=3;objects[0].pad=1;
    Require(RoomStatsBuild(&bg,&setup,&stan,&s,&why));
    Counts(&s,1,1,2,1,1,0);Counts(&s,2,1,2,0,0,0);Counts(&s,3,0,0,1,3,2);Sum(&s);RoomStatsFree(&s);
    a[0].layer=0;tiles[1].room=2;objects[0].pad=0;
    Put(raw+168+244,0); /* Cyclic monitor attachment cannot loop or select a random room. */
    Require(RoomStatsBuild(&bg,&setup,&stan,&s,&why));Counts(&s,1,2,1,1,2,0);Counts(&s,0,0,0,2,4,1);RoomStatsFree(&s);
    Put(raw+168+244,(DWORD)-1);
    StanFile none={0};SetupFile emptysetup={0};BgDocument emptybg={0};
    Require(RoomStatsBuild(&bg,&setup,&none,&s,&why));Counts(&s,0,0,0,0,7,3);Sum(&s);RoomStatsFree(&s);
    Require(RoomStatsBuild(&bg,&emptysetup,&stan,&s,&why));Counts(&s,1,2,1,1,0,0);Sum(&s);RoomStatsFree(&s);
    Require(RoomStatsBuild(&emptybg,&setup,&stan,&s,&why));Counts(&s,0,0,0,4,7,3);RoomStatsFree(&s);
    bg.levelscale=NAN;Require(RoomStatsBuild(&bg,&setup,&stan,&s,&why));Counts(&s,0,0,0,2,7,3);RoomStatsFree(&s);bg.levelscale=.5f;
    for(int failure=0;failure<3;failure++)
    {
        failafter=failure;assert(!RoomStatsBuild(&bg,&setup,&stan,&s,&why));
        assert(!s.rooms && !s.roomcount && why[0]);failafter=-1;
    }
    rooms[1].faces=NULL;assert(!RoomStatsBuild(&bg,&setup,&stan,&s,&why) && !s.rooms);rooms[1].faces=a;
    puts("PASS: all rooms and layers, scaled/stacked floors, stale references, shared and bound pads, doors, monitor attachments/cycles, excluded/deleted records, Unassigned, totals, live edits, read-only counts and allocation failure.");
}
static void Bounds(void)
{
    BgDocumentVertex vertices[5]={{.x=-10},{.x=10},{.y=20},{.z=50},{.x=30000}};
    BgDocumentFace faces[2]={{.vertexindices={0,1,2},.layer=0},{.vertexindices={0,2,3},.layer=1}};
    BgDocumentRoom rooms[4]={[1]={.origin={100,200,300},.vertices=vertices,.vertexcount=5,.faces=faces,.facecount=2},
        [2]={.origin={1000,0,0}},[3]={.origin={1000,2000,3000}}};
    BgDocument bg={.rooms=rooms,.roomcount=3,.levelscale=.5f};
    StanTile tile=Tile(0x100,2,-200,5);StanFile stan={.tiles=&tile,.tilecount=1,.levelscale=.5f};
    double min[3],max[3];
    Require(RoomStatsGetBounds(&bg,&stan,1,min,max));
    assert(min[0]==180 && max[0]==220 && min[1]==400 && max[1]==440 && min[2]==600 && max[2]==700);
    /* Secondary faces contribute; the unused outlier vertex never does. */
    rooms[1].facecount=1;Require(RoomStatsGetBounds(&bg,&stan,1,min,max));assert(max[2]==600);rooms[1].facecount=2;
    Require(RoomStatsGetBounds(&bg,&stan,2,min,max));
    assert(min[0]==-200 && max[0]==-100 && min[1]==5 && max[1]==5 && min[2]==0 && max[2]==100);
    Require(RoomStatsGetBounds(&bg,&stan,3,min,max));
    assert(min[0]==2000 && min[1]==4000 && min[2]==6000 && !memcmp(min,max,sizeof(min)));
    assert(!RoomStatsGetBounds(&bg,&stan,0,min,max) && !RoomStatsGetBounds(&bg,&stan,4,min,max));
    faces[0].vertexindices[0]=5;assert(!RoomStatsGetBounds(&bg,&stan,1,min,max));faces[0].vertexindices[0]=0;
    bg.levelscale=0;assert(!RoomStatsGetBounds(&bg,&stan,1,min,max));bg.levelscale=.5f;
    rooms[3].origin[0]=NAN;assert(!RoomStatsGetBounds(&bg,&stan,3,min,max));
    puts("PASS: room bounds use live primary/secondary corners, room origin and level scale; STAN/empty-room fallbacks and invalid IDs are handled.");
}

static unsigned char *Read(const char *dir,const char *name,DWORD *size)
{
    char path[1024];snprintf(path,sizeof(path),"%s/%s",dir,name);FILE *f=fopen(path,"rb");assert(f);
    assert(!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);unsigned char *b=malloc(*size);assert(b&&fread(b,1,*size,f)==*size);fclose(f);return b;
}
static void Depot(const char *dir)
{
    SetupFile setup={0};StanFile stan={0};BgDocument bg={0};RoomStats counts={0};DWORD size;unsigned char *data;
    const float scale=.21847887337207794f;
    Require(SetupLoadProjectFile(dir,"UsetupdepoZ",&setup,&why));
    data=Read(dir,"rom-depot.stan",&size);Require(StanLoadNative(data,size,scale,&stan,&why));free(data);
    data=Read(dir,"rom-depot.seg",&size);Require(BgDocumentLoad(data,size,scale,&bg,&why));free(data);
    Require(RoomStatsBuild(&bg,&setup,&stan,&counts,&why));Sum(&counts);
    assert(counts.roomcount==bg.roomcount && counts.total.values[ROOM_STAN_TILES]==stan.tilecount);
    assert(counts.total.values[ROOM_PRIMARY_TRIS]+counts.total.values[ROOM_SECONDARY_TRIS]==bg.facecount);
    DWORD objects=0,characters=0;
    for(DWORD i=0;i<setup.objectcount;i++)
    {const SetupObject *o=&setup.objects[i];objects+=!o->deleted && (o->type==PROPDEF_DOOR || !(o->flags&(PROPFLAG_ASSIGNEDTOCHR|PROPFLAG_INSIDEANOTHEROBJ)));}
    for(DWORD i=0;i<setup.charactercount;i++)characters+=!setup.characters[i].deleted;
    assert(counts.total.values[ROOM_OBJECTS]==objects && counts.total.values[ROOM_CHARACTERS]==characters);
    printf("PASS: Depot: %lu rooms; %lu primary / %lu secondary tris; %lu STAN tiles; %lu objects; %lu characters. Unassigned: %lu objects / %lu characters.\n",
        (unsigned long)counts.roomcount,(unsigned long)counts.total.values[0],(unsigned long)counts.total.values[1],
        (unsigned long)counts.total.values[2],(unsigned long)counts.total.values[3],(unsigned long)counts.total.values[4],
        (unsigned long)counts.rooms[0].values[3],(unsigned long)counts.rooms[0].values[4]);
    RoomStatsFree(&counts);BgDocumentFree(&bg);StanFileFree(&stan);SetupFileFree(&setup);
}
#include "navigation.c"
int main(int argc,char **argv) { Synthetic();Bounds();Navigation();if(argc==2)Depot(argv[1]);return 0; }
