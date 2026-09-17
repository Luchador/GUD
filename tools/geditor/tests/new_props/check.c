#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "newprops.h"
#include "modelcompile.h"
#include "modeledits.h"
#include "texload.h"
/* Exercise the real private placement cache and shading with the same assets. */
#include "objectload.c"
#include "propcompile.h"
static const char *why="";
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why);exit(1); } } while(0)
static DWORD Word(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD n) { p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static float Float(const unsigned char *p) { DWORD n=Word(p);float f;memcpy(&f,&n,4);return f; }
static void Write(const char *path,const void *data,DWORD size)
{ FILE *f=fopen(path,"wb");CHECK(f);CHECK(fwrite(data,1,size,f)==size);CHECK(!fclose(f)); }
BOOL CharacterGetModelDefinition(int id,CharacterModelDefinition *out) { return FALSE; }
BOOL RomLoad(const char *path,RomFile *rom,const char **reason)
{
    FILE *file=fopen(path,"rb");DWORD i;
    memset(rom,0,sizeof(*rom));CHECK(file);CHECK(!fseek(file,0,SEEK_END));
    rom->size=ftell(file);CHECK(!fseek(file,0,SEEK_SET));rom->data=malloc(rom->size);CHECK(rom->data);
    CHECK(fread(rom->data,1,rom->size,file)==rom->size);CHECK(!fclose(file));
    rom->info.size=rom->size;rom->info.manifestoffset=0x100;rom->info.entrycount=Word(rom->data+0x114);
    CHECK(rom->info.entrycount<=ROM_MAX_ENTRIES);
    for (i=0;i<rom->info.entrycount;i++)
    {
        const unsigned char *p=rom->data+0x118+i*16;RomManifestEntry *e=&rom->info.entries[i];
        e->kind=Word(p);e->romstart=Word(p+4);e->romend=Word(p+8);e->flags=Word(p+12);
    }
    return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data);memset(rom,0,sizeof(*rom)); }
BOOL RomFindFile(const RomFile *rom,const char *name,DWORD *offset,DWORD *size,const char **reason)
{ return !strcmp(name,"PnativeZ"); }
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h)
{ *w=id==0xb00?64:32;*h=32;return id==0xd4 || id==0xb00; }
BOOL TexLoadProjectImage(const char *project,DWORD id,TexPixel *out,int *w,int *h)
{ if (!TexGetProjectImageSize(project,id,w,h)) return FALSE;memset(out,255,*w**h*sizeof(*out));return TRUE; }
BOOL TexEncodePng(const TexPixel *pixels,int w,int h,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(8,1);return *data!=NULL; }
static void CheckModel(const char *project,DWORD expected)
{
    DWORD size,i,blended=0;const unsigned char *data=NewPropsData(project,"PpendantZ",&size);
    ModelSource source={0};float low[3],high[3];
    CHECK(data && !(size&15));CHECK(Word(data)==0x05000004u);
    CHECK(ModelReadSource(data,size,&source,&why));CHECK(source.count==expected);
    CHECK(ModelReadPlacementBounds(data,size,low,high));
    CHECK(low[1]==0 && high[1]==200);CHECK(Float(data+100)>2000);
    for (i=0;i<source.listcount;i++)
    {
        DWORD pc=source.lists[i].offset;
        CHECK(Word(data+pc)==0x01020040 && Word(data+pc+4)==0x03000000);
    }
    for (i=0;i<source.count;i++)
    {
        const BgVertex *v=&source.vertices[i*3];
        CHECK(v->g==160 && v->b==120);
        if (source.flags[i]&BG_RENDER_BLEND)
        {
            DWORD pc=Word(data+136)&0xffffff,geom=0x10000;
            CHECK(v->a==25 && v->r==200);CHECK(BG_TEX_ID(source.tags[i])==BG_TEX_NONE);
            CHECK(source.flags[i]&BG_RENDER_NO_FOG);
            CHECK(!(source.flags[i]&(BG_RENDER_DEPTH_WRITE|BG_RENDER_CULL_BACK)));
            CHECK(source.flags[i]&BG_RENDER_DEPTH_TEST);
            for (;data[pc]!=0xb8;pc+=8)
            {
                if (data[pc]==0xb6) geom&=~Word(data+pc+4);
                if (data[pc]==0xb7) geom|=Word(data+pc+4);
                if (data[pc]==4) CHECK(!(geom&0x30000));
                if (Word(data+pc)==0xb900031d) CHECK(Word(data+pc+4)==0x0c1849d8);
            }
            blended++;
        }
        else
        {
            CHECK(v->a==255);
            CHECK(source.flags[i]&BG_RENDER_DEPTH_WRITE);
            CHECK(source.flags[i]&BG_RENDER_CULL_BACK);
            if (BG_TEX_ID(source.tags[i])==BG_TEX_NONE) CHECK(v->r==100);
        }
    }
    CHECK(blended==1);CHECK(BG_TEX_ID(source.tags[0])==0xd4);CHECK(BG_TEX_ID(source.tags[1])==0xb00);
    CHECK(source.vertices[1].s==32 && source.vertices[4].s==64);
    ModelFreeSource(&source);
}
int main(int argc,char **argv)
{
    const char *project=argv[1],*name;float scale;DWORD count,size,hash,oldsize,start,i;
    char path[MAX_PATH],base[MAX_PATH],source[MAX_PATH];RomFile rom={0};unsigned char *snapshot;
    const unsigned char *data;extern int test_fail_move;
    CHECK(argc==2);snprintf(base,sizeof(base),"%s/base.z64",project);
    rom.size=0x200000;rom.data=calloc(rom.size,1);CHECK(rom.data);
    Put(rom.data+0x114,2);Put(rom.data+0x118,CUSTOM_PROP_MANIFEST_KIND);
    Put(rom.data+0x11c,0x200);Put(rom.data+0x120,0x210);Put(rom.data+0x124,1);
    Put(rom.data+0x128,CUSTOM_PROP_DATA_KIND);Put(rom.data+0x134,1);Put(rom.data+0x200,1);
    Write(base,rom.data,rom.size);RomFree(&rom);
    CHECK(NewPropsOpen(project,&why));CHECK(NewPropsCount()==0);
    snprintf(source,sizeof(source),"%s/pendant.glb",project);
    CHECK(!NewPropsImport(project,"PnativeZ",source,FALSE,&count,&why));
    CHECK(NewPropsImport(project,"PpendantZ",source,FALSE,&count,&why));CHECK(count==4);
    CHECK(NewPropsDefinition(512,&name,&scale));CHECK(!strcmp(name,"PpendantZ") && fabsf(scale-.1f)<1e-6);
    { BOOL character=TRUE;int id=-1;
      CHECK(ObjectResolvePlaceableModel("PpendantZ",&character,&id) && !character && id==512); }
    CHECK(!NewPropsDefinition(511,NULL,NULL) && !NewPropsDefinition(513,NULL,NULL));
    CHECK(!NewPropsImport(project,"PpendantZ",source,FALSE,&count,&why));
    CHECK(NewPropsImport(project,"PpendantZ",source,TRUE,&count,&why));
    CheckModel(project,4);data=NewPropsData(project,"PpendantZ",&size);hash=ModelDataHash(data,size);
    {
        const char *bad[]={"missing","animation","skin","mask","collapse","range","line"};
        for(i=0;i<sizeof(bad)/sizeof(*bad);i++)
        {
            snprintf(path,sizeof(path),"%s/%s.glb",project,bad[i]);
            CHECK(!NewPropsImport(project,"PpendantZ",path,TRUE,&count,&why));
            data=NewPropsData(project,"PpendantZ",&size);CHECK(ModelDataHash(data,size)==hash);
        }
    }
    {
        ModelCacheEntry *cache=calloc(OBJECT_MODEL_CACHE_COUNT,sizeof(*cache)),*entry;
        ObjectBuilder builder={0};ObjectShade shade={{0,0,0,128}};
        CHECK(cache);entry=ObjectGetModel(cache,512,project,&rom);CHECK(entry);
        CHECK(entry->min[1]==0 && entry->max[1]==200 && entry->tricount==4);
        builder.tris=entry->tris;builder.renderflags=entry->renderflags;builder.tricount=entry->tricount;
        ObjectShadeModel(&builder,0,512,&shade);
        CHECK(builder.tris[0].r<200 && builder.tris[9].r==200 && builder.tris[9].a==25);
        free(entry->tris);free(entry->tritags);free(entry->renderflags);free(cache);
    }
    {
        BgVertex vertices[3]={{.x=0,.a=25},{.x=.2f,.a=25},{.y=.2f,.a=25}};
        unsigned short tag=BG_TEX_NONE;BgRenderFlags flags=BG_RENDER_BLEND;
        unsigned char *native=NULL;DWORD length;float radius;ModelSource mesh={0};
        CHECK(PropCompile(vertices,&tag,&flags,1,project,&native,&length,&radius,&why));
        CHECK(Word(native+132)!=0 && Word(native+136)!=0);
        CHECK(native[Word(native+132)&0xffffff]==0xb8);
        CHECK(ModelReadSource(native,length,&mesh,&why) && mesh.count==1);
        ModelFreeSource(&mesh);free(native);
    }
    test_fail_move=1;CHECK(!NewPropsSave(project,&why));CHECK(NewPropsHasUnsaved());
    CHECK(NewPropsSave(project,&why));CHECK(!NewPropsHasUnsaved());NewPropsReset();
    CHECK(NewPropsOpen(project,&why));CheckModel(project,4);
    snprintf(path,sizeof(path),"%s/roundtrip.gltf",project);
    CHECK(ModelEditsExport(project,"PpendantZ",path,&why));
    CHECK(NewPropsImport(project,"PpendantZ",path,TRUE,&count,&why));CheckModel(project,4);
    data=NewPropsData(project,"PpendantZ",&size);CHECK(ModelDataHash(data,size)==hash);
    snprintf(path,sizeof(path),"%s/changed.gltf",project);
    CHECK(NewPropsImport(project,"PpendantZ",path,TRUE,&count,&why));CHECK(count==5);CheckModel(project,5);
    CHECK(NewPropsImport(project,"PsecondZ",source,FALSE,&count,&why));CHECK(NewPropsCount()==2);
    CHECK(NewPropsDefinition(513,&name,NULL) && !strcmp(name,"PsecondZ"));
    CHECK(ModelEditsSave(project,&why));CHECK(RomLoad(base,&rom,&why));CHECK(NewPropsExportToRom(project,&rom,&why));
    start=Word(rom.data+0x204);CHECK(start>=0x101000 && !(start&15));
    CHECK(Word(rom.data+start)==CUSTOM_PROP_MAGIC && Word(rom.data+start+4)==2);
    CHECK(rom.info.entries[1].romstart==start);CHECK(Word(rom.data+0x12c)==start);
    oldsize=rom.size;snapshot=malloc(oldsize);CHECK(snapshot);memcpy(snapshot,rom.data,oldsize);
    CHECK(NewPropsExportToRom(project,&rom,&why));CHECK(oldsize==rom.size && !memcmp(snapshot,rom.data,oldsize));
    free(snapshot);
    rom.info.entrycount=0;CHECK(!NewPropsCheckRebase(project,&rom,&why));rom.info.entrycount=2;
    rom.data[start+16+1]='x';CHECK(!NewPropsCheckRebase(project,&rom,&why));rom.data[start+17]='p';
    CHECK(NewPropsCheckRebase(project,&rom,&why));
    /* A project extracted from an exported ROM inherits the same stable IDs. */
    Write(base,rom.data,rom.size);RomFree(&rom);
    snprintf(path,sizeof(path),"%s/models/newprops.gnp",project);CHECK(DeleteFile(path));ModelEditsReset();
    CHECK(NewPropsOpen(project,&why));CHECK(NewPropsHasUnsaved());CHECK(NewPropsCount()==2);CheckModel(project,5);
    CHECK(NewPropsSave(project,&why));
    /* Negative determinant flips winding along with the transformed mesh. */
    snprintf(path,sizeof(path),"%s/mirror.glb",project);
    CHECK(NewPropsImport(project,"PpendantZ",path,TRUE,&count,&why));
    {
        ModelSource mesh={0};data=NewPropsData(project,"PpendantZ",&size);
        CHECK(ModelReadSource(data,size,&mesh,&why));
        CHECK(mesh.vertices[1].y==200 && mesh.vertices[2].x==-200);ModelFreeSource(&mesh);
    }
    ModelEditsReset();puts("PASS four materials, alpha, bounds, winding, validation, IDs, topology, atomic save, roundtrip, bank export and rebase.");
    return 0;
}
