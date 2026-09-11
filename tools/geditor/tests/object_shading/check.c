#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "objectload.c"
#include "characterload.c"

/* Compile the unchanged game function as the reference, with minimal types. */
typedef unsigned char u8;
typedef unsigned short u16;
typedef int s32;
typedef struct { struct { u16 half; } mid; } StandTile;
typedef struct { StandTile *stan; } PropRecord;
#include "reference.h"

static void Colors(void)
{
    StanTile tile={0}; StanFile stan={.tiles=&tile,.tilecount=1};
    StandTile native={0}; PropRecord prop={&native};
    for (unsigned rgb=0; rgb<4096; rgb++)
    {
        unsigned char expected[4]; ObjectShade character,object;
        native.mid.half=(u16)rgb;
        tile.red=((rgb>>8)&15)*17; tile.green=((rgb>>4)&15)*17; tile.blue=(rgb&15)*17;
        objSetColorFromTile(&prop,expected);
        ObjectShadeFromTile(&stan,0,TRUE,0,&character);
        assert(!memcmp(expected,character.rgba,4));
        ObjectShadeFromTile(&stan,0,FALSE,0,&object);
        for (int c=0;c<3;c++) { assert(object.rgba[c]==((expected[c]>>1)*255>>8)); }
        assert(object.rgba[3]==expected[3]);
        ObjectShadeFromTile(&stan,0,FALSE,PROPFLAG_ILLUMINATED,&object);
        assert(!memcmp(object.rgba,(u8[4]){0},4));
    }
    ObjectShade neutral;
    ObjectShadeFromTile(NULL,0,FALSE,0,&neutral); assert(!memcmp(neutral.rgba,(u8[4]){0},4));
    ObjectShadeFromTile(&stan,STAN_TILE_NONE,FALSE,0,&neutral); assert(!memcmp(neutral.rgba,(u8[4]){0},4));
    BgVertex original={.r=255,.g=128,.b=0,.a=37}, vertex=original;
    tile.red=tile.green=tile.blue=255;
    ObjectShadeFromTile(&stan,0,FALSE,0,&neutral); ObjectShadeVertices(&vertex,1,&neutral);
    assert(!memcmp(&original,&vertex,sizeof(vertex)));
    tile.red=tile.green=tile.blue=0;
    ObjectShadeFromTile(&stan,0,FALSE,0,&neutral); ObjectShadeVertices(&vertex,1,&neutral);
    assert(vertex.r==64 && vertex.g==32 && vertex.b==0 && vertex.a==37);
    puts("PASS: all 4096 tile colors match objSetColorFromTile; prop attenuation, illuminated/missing-tile neutrality, darkening and vertex alpha.");
}

/* Feed native fixture assets through the real model loaders and placement
 * pipeline; replace only project/ROM IO and the unrelated animation bank. */
static struct { const char *name; unsigned char *data; DWORD size,offset; } files[]={
    {.name="Pconsole1Z"}, {.name="Ptv1Z"}, {.name="CcamguardZ"}, {.name="CheadchrisZ"}, {.name="PchrwppkZ"}
};
static DWORD romsize;
static void Fixtures(const char *root)
{
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++)
    {
        char path[1024];
        snprintf(path,sizeof(path),"%s/assets/obseg/%s/%s.bin",root,files[i].name[0]=='C'?"chr":"prop",files[i].name);
        FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); files[i].size=ftell(f); rewind(f);
        files[i].data=malloc(files[i].size); assert(files[i].data);
        assert(fread(files[i].data,1,files[i].size,f)==files[i].size); fclose(f);
        files[i].offset=romsize; romsize+=files[i].size;
    }
}
BOOL RomLoad(const char *path, RomFile *rom, const char **reason)
{
    memset(rom,0,sizeof(*rom)); rom->data=malloc(romsize); rom->size=romsize; assert(rom->data);
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++)
    { memcpy(rom->data+files[i].offset,files[i].data,files[i].size); }
    *reason=""; return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); memset(rom,0,sizeof(*rom)); }
BOOL RomFindFile(const RomFile *rom, const char *name, DWORD *offset, DWORD *size, const char **reason)
{
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++) if (!strcmp(name,files[i].name))
    { *offset=files[i].offset; *size=files[i].size; return TRUE; }
    return FALSE;
}
const unsigned char *ModelEditsGetData(const char *dir, const char *name, DWORD *size, const char **reason)
{
    *reason="";
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++) if (!strcmp(name,files[i].name))
    { *size=files[i].size; return files[i].data; }
    return NULL;
}
BgVertex *GltfLoadModel(const char *path, const char *dir, DWORD *count, unsigned short **tags,
    BgRenderFlags **flags, const char **reason) { abort(); }
BOOL MonitorBankLoadRom(MonitorBank *bank, const RomFile *rom, const char **reason)
{ memset(bank,0,sizeof(*bank)); return TRUE; }
void MonitorGeometryFree(MonitorGeometry *geometry)
{ free(geometry->surfaces); memset(geometry,0,sizeof(*geometry)); }
static int PropModel(const char *name)
{
    const char *candidate;
    for (int i=0;ModelGetPropDefinition(i,&candidate,NULL);i++) if (!strcmp(candidate,name)) { return i; }
    abort();
}
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void Compare(const SetupObjectGeometry *source, const SetupObjectGeometry *shaded,
    const StanFile *stan, DWORD tileindex)
{
    assert(source->tricount==shaded->tricount && source->objectcount==shaded->objectcount);
    unsigned seen=0;
    for (DWORD i=0;i<source->tricount;i++)
    {
        DWORD owner=source->objectindices[i];
        BOOL character=(owner&SETUP_CHARACTER_SELECTION_BIT)!=0;
        ObjectShade shade;
        ObjectShadeFromTile(stan,tileindex,character,owner==1?PROPFLAG_ILLUMINATED:0,&shade);
        assert(source->objectindices[i]==shaded->objectindices[i] && source->tritags[i]==shaded->tritags[i]);
        assert(source->renderflags[i]==shaded->renderflags[i]);
        for (int c=0;c<3;c++)
        {
            BgVertex expected=source->tris[i*3+c];
            if (!(source->renderflags[i]&BG_RENDER_MONITOR)) { ObjectShadeVertices(&expected,1,&shade); }
            assert(!memcmp(&expected,&shaded->tris[i*3+c],sizeof(expected)));
        }
        seen|=character?8:1<<owner;
    }
    assert(seen==15); /* ordinary, illuminated, attached-monitor, and character meshes */
}
static void Placement(void)
{
    StanTile tiles[2]={0}; StanFile stan={.tiles=tiles,.tilecount=2,.levelscale=1};
    for (int i=0;i<2;i++)
    {
        float x=i?1000:-1000;
        tiles[i].id=i+1; tiles[i].room=i+1; tiles[i].pointcount=3;
        tiles[i].extreme[0]=0; tiles[i].extreme[1]=1; tiles[i].extreme[2]=2;
        tiles[i].points[0]=(StanPoint){x-400,0,-400,0};
        tiles[i].points[1]=(StanPoint){x,0,400,0};
        tiles[i].points[2]=(StanPoint){x+400,0,-400,0};
        tiles[i].red=tiles[i].green=tiles[i].blue=255;
    }
    SetupPad pads[2]={0}; SetupObject objects[4]={0}; SetupCharacter character={0};
    unsigned char raw[1024]={0}; SetupFile setup={.data=raw,.size=sizeof(raw),.objects=objects,.objectcount=4,
        .pads=pads,.padcount=2,.characters=&character,.charactercount=1};
    for (int i=0;i<2;i++)
    {
        pads[i].pos[0]=-1000; pads[i].up[1]=pads[i].look[2]=1;
        objects[i]=(SetupObject){.type=PROPDEF_PROP,.extrascale=256,.modelid=PropModel("Pconsole1Z"),
            .pad=i,.flags=PROPFLAG_INAIR|PROPFLAG_ABSOLUTEPOSITION,.sourceoffset=40+i*128};
        raw[objects[i].sourceoffset+3]=objects[i].type;
    }
    objects[1].flags|=PROPFLAG_ILLUMINATED;
    objects[2]=(SetupObject){.type=PROPDEF_MONITOR,.extrascale=256,.modelid=PropModel("Ptv1Z"),
        .pad=-1,.flags=PROPFLAG_FIXED_MONITOR|PROPFLAG_ILLUMINATED,.sourceoffset=296};
    Put(raw+12,40); raw[299]=PROPDEF_MONITOR; Put(raw+296+244,(DWORD)-2); /* Child of object 0. */
    raw[555]=48; /* end command */
    character.chrnum=10; character.bodyid=CharacterFindModel("CcamguardZ"); character.headid=CharacterFindModel("CheadchrisZ");
    objects[3]=(SetupObject){.type=PROPDEF_COLLECTABLE,.extrascale=256,.modelid=PropModel("PchrwppkZ"),
        .pad=10,.flags=PROPFLAG_ASSIGNEDTOCHR,.sourceoffset=600};
    const char *why=""; SetupObjectGeometry white={0},dark={0},again={0},missing={0},moved={0};
    assert(ObjectLoadSetupGeometry("test",&setup,&stan,1,&white,&why));
    assert(white.objectcount==4 && white.monitors.count==1);
    tiles[0].red=34; tiles[0].green=136; tiles[0].blue=17;
    assert(ObjectLoadSetupGeometry("test",&setup,&stan,1,&dark,&why)); Compare(&white,&dark,&stan,0);
    assert(ObjectLoadSetupGeometry("test",&setup,&stan,1,&again,&why));
    assert(again.tricount==dark.tricount && !memcmp(again.tris,dark.tris,dark.tricount*3*sizeof(BgVertex)));
    /* Missing STAN keeps neutral shading. INAIR places props at their pads;
       the character starts at the same zero-height floor in both previews. */
    assert(ObjectLoadSetupGeometry("test",&setup,NULL,1,&missing,&why)); Compare(&white,&missing,NULL,STAN_TILE_NONE);
    pads[0].pos[0]=1000;
    assert(ObjectLoadSetupGeometry("test",&setup,&stan,1,&moved,&why));
    assert(moved.tricount==white.tricount);
    for (DWORD i=0;i<white.tricount*3;i++)
    {
        const BgVertex *a=&white.tris[i],*b=&moved.tris[i];
        assert(a->r==b->r && a->g==b->g && a->b==b->b && a->a==b->a);
    }
    ObjectGeometryFree(&white); ObjectGeometryFree(&dark); ObjectGeometryFree(&again);
    ObjectGeometryFree(&missing); ObjectGeometryFree(&moved);
    puts("PASS: real console/attached monitor/posed guard/head/held weapon placement, illuminated flags, emissive screens, repeat rebuilds, missing tiles and movement between tile colors.");
}
int main(int argc,char **argv)
{
    assert(argc==2); Colors(); Fixtures(argv[1]); Placement();
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++) { free(files[i].data); }
    return 0;
}
