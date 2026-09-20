#include <assert.h>
#include "romexport.c"

/* Model replacement is orthogonal to this resource-cleanup harness. */
int ModelEditsReadReplacement(const char *dir, const char *name,
    const unsigned char *base, DWORD size, unsigned char **out, DWORD *length, const char **why)
{ return 0; }
static const char *why;
static void Require(BOOL ok) { if (!ok) { fprintf(stderr, "%s\n", why); abort(); } }
static void Put(unsigned char *p, DWORD v) { RomExportWrite32(p, v); }
static DWORD Get(const unsigned char *p) { return RomExportRead32(p); }
static void Float(unsigned char *p, float value)
{ union { float f; DWORD u; } v; v.f=value; Put(p,v.u); }
static void Save(const char *path, const void *data, DWORD size)
{ FILE *f=fopen(path,"wb"); assert(f && fwrite(data,1,size,f)==size); assert(!fclose(f)); }

static void SamePolygon(const unsigned char *a,DWORD x,const unsigned char *b,DWORD y)
{
    assert((x&0xff000000u)==(y&0xff000000u)); x&=0xffffffu;y&=0xffffffu;
    assert(a[x]==b[y]&&!memcmp(a+x,b+y,4+a[x]*12));
}

static void SameBackground(const BgFile *before,const BgFile *after)
{
    const unsigned char *a=before->data,*b=after->data;
    if(Get(a)) { assert(before->size==after->size&&!memcmp(a,b,before->size)); return; }
    DWORD ar=Get(a+4)&0xffffffu,br=Get(b+4)&0xffffffu;
    for(DWORD room=1;;room++)
    {
        const unsigned char *x=a+ar+room*24,*y=b+br+room*24;
        if(!Get(x+4)) { assert(!Get(y+4)); break; }
        assert(!memcmp(x+12,y+12,12));
        for(DWORD field=0;field<3;field++)
        {
            DWORD old=Get(x+field*4)&0xffffffu,next=Get(y+field*4)&0xffffffu;
            if(!old) { assert(!next); continue; }
            DWORD size=Get(a+old-4);assert(size==Get(b+next-4)&&!memcmp(a+old,b+next,size));
        }
    }
    DWORD ap=Get(a+8)&0xffffffu,bp=Get(b+8)&0xffffffu;
    assert(!ap==!bp);
    if(ap) for(DWORD i=0;;i+=8)
    {
        if(!Get(a+ap+i)) { assert(!Get(b+bp+i)); break; }
        assert(!memcmp(a+ap+i+4,b+bp+i+4,4));
        SamePolygon(a,Get(a+ap+i),b,Get(b+bp+i));
        /* Preserve shared identity, not just coincident point coordinates. */
        for(DWORD j=0;j<i;j+=8)
            assert((Get(a+ap+i)==Get(a+ap+j))==(Get(b+bp+i)==Get(b+bp+j)));
    }
    DWORD av=Get(a+12)&0xffffffu,bv=Get(b+12)&0xffffffu;
    assert(!av==!bv);
    if(av) for(DWORD i=0;;i+=8)
    {
        assert(!memcmp(a+av+i,b+bv+i,4));
        if(a[av+i]==0x64) { SamePolygon(a,Get(a+av+i+4),b,Get(b+bv+i+4)); }
        else { assert(Get(a+av+i+4)==Get(b+bv+i+4)); }
        if(!a[av+i]) { break; }
    }
}

static void Background(const char *dir)
{
    BgFile source={0}, packed={0}, again={0}, loaded={0};
    source.size=2048; source.data=calloc(source.size,1); strcpy(source.name,"bg/bg_clean_all_p.seg");
    unsigned char *b=source.data;
    Put(b+4,0x0f000014); Put(b+8,0x0f000100); Put(b+12,0x0f000180);
    Put(b+44,0x0f000504); Put(b+48,0x0f000584);
    Float(b+56,17.5f); Float(b+60,-42);
    Put(b+0x100,0x0f000200); b[0x104]=1; b[0x105]=2; b[0x106]=23;
    Put(b+0x108,0x0f000200); b[0x10c]=2; b[0x10d]=1;
    b[0x180]=b[0x188]=0x64; Put(b+0x184,0x0f000200); Put(b+0x18c,0x0f000280);
    b[0x200]=b[0x280]=3; b[0x201]=0x17; b[0x281]=0x29;
    memset(b+0x300,0xcc,0x200); /* obsolete table and polygon allocations */
    Put(b+0x500,48); Put(b+0x580,16); Put(b+0x584,0xb8000000);
    Require(BgFileCompact(&source,&packed,&why)); assert(packed.size<source.size);
    SameBackground(&source,&packed);
    DWORD table=Get(packed.data+8)&0xffffffu, vis=Get(packed.data+12)&0xffffffu;
    DWORD polygon=Get(packed.data+table)&0xffffffu;
    assert(Get(packed.data+table)==Get(packed.data+table+8));
    assert(Get(packed.data+vis+4)==Get(packed.data+table));
    assert(!memcmp(packed.data+polygon,b+0x200,40));
    assert(!memcmp(packed.data+(Get(packed.data+vis+12)&0xffffffu),b+0x280,40));
    DWORD rooms=Get(packed.data+4)&0xffffffu;
    assert(!memcmp(packed.data+rooms+36,b+56,12));
    assert(!memcmp(packed.data+(Get(packed.data+rooms+24)&0xffffffu),b+0x504,48));
    for(int pass=0;pass<30;pass++)
    {
        Require(BgFileCompact(&packed,&again,&why));
        assert(again.size==packed.size&&!memcmp(again.data,packed.data,packed.size)); BgFileFree(&again);
    }
    /* Disk cleanup must not relocate the live source used by undo/redo. */
    source.newportaloffsets[3]=0x200;
    Require(BgSaveProjectFile(dir,&source,&why));
    assert(Get(source.data+8)==0x0f000100 && source.newportaloffsets[3]==0x200);
    Require(BgLoadProjectFile(dir,source.name,&loaded,&why));
    assert(loaded.size==packed.size&&!memcmp(loaded.data,packed.data,packed.size));
    BgFileFree(&loaded);
    Put(b+0x184,0x0ffffffc); /* malformed script pointer fails without output */
    assert(!BgFileCompact(&source,&again,&why)&&!again.data&&!again.size);
    BgFileFree(&packed); BgFileFree(&source);
}

static StanFile Collision(void)
{
    unsigned char b[256]={0}; StanFile stan={0};
    Put(b+4,0x0e00000c); Put(b+12,0x00010001); /* p1a, room 1 */
    b[18]=0x30;b[19]=0x12;
    /* Clockwise triangle; runtime queries use native integer points. */
    b[32]=0;b[33]=100; b[36]=0;b[37]=100;
    memcpy(b+52,"unstric",8); memset(b+76,0xa5,sizeof(b)-76);
    Require(StanLoadNative(b,sizeof(b),.25f,&stan,&why));
    strcpy(stan.name,"Tbg_clean_all_p_stanZ"); return stan;
}

static void Pads(const char *dir)
{
    StanFile stan=Collision();
    unsigned char b[512]={0}, *packed=NULL,*again=NULL; DWORD size,againSize;
    SetupStanRefresh stats;
    Put(b+24,48); Put(b+28,224);
    for(int i=0;i<3;i++)
    {
        DWORD at=48+i*44;
        Float(b+at,i==1 ? 1000 : 10); Float(b+at+4,20); Float(b+at+8,10);
        Float(b+at+16,1);Float(b+at+32,1);Put(b+at+36,400);
    }
    Put(b+48+2*44+40,0x47455044u); /* deleted pad */
    Float(b+224,10);Float(b+228,20);Float(b+232,10);
    Float(b+240,1);Float(b+256,1);Put(b+260,400);
    memcpy(b+400,"missing",8);
    char resolved[16]; float pos[3]={40,80,40};
    Require(StanResolveSavedPadName(&stan,"",pos,resolved)); assert(!strcmp(resolved,"p1a"));
    Require(SetupRefreshPadStanNative(b,sizeof(b),&stan,&packed,&size,&stats,&why));
    assert(stats.updated==2&&stats.unresolved==1&&stats.firstunresolved.index==1&&!stats.firstunresolved.bound);
    for(int i=0;i<3;i++)
    {
        DWORD old=48+i*44, at=Get(packed+24)+i*44;
        assert(!memcmp(b+old,packed+at,36)); assert(Get(b+old+40)==Get(packed+at+40));
        assert(!strcmp((char *)packed+Get(packed+at+36),i ? "missing" : "p1a"));
    }
    Require(SetupRefreshPadStanNative(packed,size,&stan,&again,&againSize,&stats,&why));
    assert(!stats.updated&&stats.unresolved==1&&size==againSize&&!memcmp(packed,again,size));
    free(again);
    SetupFile setup={0},loaded={0};setup.data=b;setup.size=sizeof(b);strcpy(setup.name,"UsetupcleanZ");
    Require(SetupSaveProjectFileWithStan(dir,&setup,&stan,&stats,&why));
    Require(SetupLoadProjectFile(dir,setup.name,&loaded,&why));
    assert(loaded.size==size&&!memcmp(loaded.data,packed,size));SetupFileFree(&loaded);
    assert(!strcmp((char *)b+400,"missing"));
    DWORD native; Require(StanMeasureNative(stan.data,stan.size,&native,&why)); assert(native==76);
    Require(StanSaveProjectFile(dir,&stan,&why));
    StanFile reloaded={0};Require(StanLoadProjectFile(dir,stan.name,stan.levelscale,&reloaded,&why));
    assert(reloaded.size==76&&reloaded.tilecount==1);StanFileFree(&reloaded);
    stan.data[60]=1;Require(StanMeasureNative(stan.data,stan.size,&native,&why)); assert(native==stan.size);
    /* Unknown footer extensions are retained, rather than silently truncated. */
    free(packed);StanFileFree(&stan);
}

static void RealRom(const char *path,const char *dir)
{
    RomFile rom={0};GEditorProject project={0};char output[MAX_PATH];
    Require(RomLoad(path,&rom,&why)); strcpy(project.dir,dir);
    project.levelcount=rom.info.levelcount;memcpy(project.levels,rom.info.levels,sizeof(project.levels));
    for(DWORD index=0;;index++)
    {
        char name[64];DWORD offset,length;
        if(!RomGetFileByIndex(&rom,index,name,sizeof(name),NULL,NULL)) { break; }
        if(strncmp(name,"bg/",3)) { continue; }
        Require(RomGetFileByIndex(&rom,index,name,sizeof(name),&offset,&length));
        BgFile before={0},after={0};before.data=rom.data+offset;before.size=length;
        Require(BgFileCompact(&before,&after,&why));SameBackground(&before,&after);BgFileFree(&after);
    }
    for(DWORD i=0;i<project.levelcount;i++) if(!strcmp(project.levels[i].setupname,"UsetupdepoZ"))
    {
        DWORD offset,length,so,sl,packedsize;unsigned char *packed;StanFile stan={0};SetupStanRefresh stats;
        Require(RomFindFile(&rom,project.levels[i].setupname,&offset,&length,&why));
        Require(RomFindFile(&rom,project.levels[i].stanname,&so,&sl,&why));
        Require(StanLoadNative(rom.data+so,sl,project.levels[i].levelscale,&stan,&why));
        Require(SetupRefreshPadStanNative(rom.data+offset,length,&stan,&packed,&packedsize,&stats,&why));
        printf("Depot: %lu pad references refreshed; %lu unresolved.\n",(unsigned long)stats.updated,(unsigned long)stats.unresolved);
        free(packed);StanFileFree(&stan);break;
    }
    for(int pass=0;pass<2;pass++)
    {
        Require(RomExportReplaceProjectResources(&project,&rom,&why));
        printf("Export %d: %s\n",pass+1,RomExportCleanupWarning());
        snprintf(output,sizeof(output),"%s/pass%d.z64",dir,pass+1);Save(output,rom.data,rom.size);
    }
    /* An unopened level's edited STAN must win over its base-ROM copy when
     * refreshing setup names. Change only the ID of pad 0's selected tile. */
    for(DWORD i=0;i<project.levelcount;i++) if(!strcmp(project.levels[i].setupname,"UsetupdepoZ"))
    {
        const RomLevel *level=&project.levels[i];StanFile stan={0};
        DWORD offset,length,so,sl,size;unsigned char *expected;SetupStanRefresh stats;
        Require(RomFindFile(&rom,level->setupname,&offset,&length,&why));
        Require(RomFindFile(&rom,level->stanname,&so,&sl,&why));
        Require(StanLoadNative(rom.data+so,sl,level->levelscale,&stan,&why));
        const unsigned char *setup=rom.data+offset;DWORD pad=Get(setup+24);float pos[3];
        for(int axis=0;axis<3;axis++)
        { union{DWORD u;float f;}v;v.u=Get(setup+pad+axis*4);pos[axis]=v.f*(1.0f/stan.levelscale); }
        DWORD tile=StanResolvePadTile(&stan,(char *)setup+Get(setup+pad+36),pos);
        assert(tile!=STAN_TILE_NONE);stan.tiles[tile].id=(30000u<<8)|1u;
        Put(stan.data+stan.tiles[tile].sourceoffset,(stan.tiles[tile].id<<8)|stan.tiles[tile].room);
        strcpy(stan.name,level->stanname);Require(StanSaveProjectFile(dir,&stan,&why));
        Require(SetupRefreshPadStanNative(setup,length,&stan,&expected,&size,&stats,&why));
        assert(stats.updated);
        Require(RomExportReplaceProjectResources(&project,&rom,&why));
        Require(RomFindFile(&rom,level->setupname,&offset,&length,&why));
        assert(length>=size&&!memcmp(rom.data+offset,expected,size));
        free(expected);StanFileFree(&stan);
        puts("PASS: unopened Depot setup references the edited STAN selected for export.");break;
    }
    RomFree(&rom);
}

int main(int argc,char **argv)
{
    assert(argc==2||argc==3);Background(argv[1]);Pads(argv[1]);
    if(argc==3) { RealRom(argv[2],argv[1]); }
    puts("PASS: live BG reachability, shared/script-only polygons, save identity, pad refresh, unresolved/deleted pads, STAN tail and repeat stability.");
    return 0;
}
