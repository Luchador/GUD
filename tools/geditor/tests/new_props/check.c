#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "newprops.h"
#include "modelcompile.h"
#include "modeledits.h"
#include "texload.h"
#include "gltf.h"
/* Exercise the real private placement cache and shading with the same assets. */
#include "objectload.c"
#include "propcompile.h"
static const char *why="";
static BOOL realpendant;
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
{ *w=id==0xb00?64:id==0xa93?48:realpendant?16:32;*h=realpendant && id==0xd4?64:32;return id==0xd4 || id==0xb00 || id==0xa93; }
BOOL TexLoadProjectImage(const char *project,DWORD id,TexPixel *out,int *w,int *h)
{ if (!TexGetProjectImageSize(project,id,w,h)) return FALSE;memset(out,255,*w**h*sizeof(*out));return TRUE; }
BOOL TexEncodePng(const TexPixel *pixels,int w,int h,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(8,1);return *data!=NULL; }
static void Assign(const char *project,DWORD slot,DWORD texture)
{
    ModelSource source={0};DWORD revision;
    CHECK(ModelEditsReadSource(project,"PpendantZ",&source,&revision,&why));
    ModelFreeSource(&source);
    CHECK(ModelEditsSetMaterial(project,"PpendantZ",revision,slot,texture,&why));
}
static void CheckSlots(const char *project)
{
    ModelSource source={0};DWORD revision,i,size;
    const unsigned char *data;
    CHECK(ModelEditsReadSource(project,"PpendantZ",&source,&revision,&why));
    CHECK(source.materials.count==4);
    CHECK(!strcmp(source.materials.slots[0].name,"Cable"));
    CHECK(!strcmp(source.materials.slots[1].name,"Metal"));
    for(i=0;i<4;i++) CHECK(BG_TEX_ID(source.tags[i])==BG_TEX_NONE);
    CHECK(source.materials.faces[0].uv[0]==1.0f && source.materials.faces[0].uv[2]==0.0f);ModelFreeSource(&source);
    CHECK(!ModelEditsSetMaterial(project,"PpendantZ",revision,0,0xbad,&why));
    Assign(project,0,0xd4);Assign(project,1,0xd4);
    CHECK(ModelEditsReadSource(project,"PpendantZ",&source,&revision,&why));
    CHECK(source.materials.count==4 && source.materials.faces[0].slot!=source.materials.faces[1].slot);
    ModelFreeSource(&source);
    Assign(project,1,0xb00);Assign(project,0,BG_TEX_NONE);Assign(project,0,0xd4);
    data=NewPropsData(project,"PpendantZ",&size);CHECK(ModelMaterialsNativeSize(data,size)<size);
    /* Repeated changes must replace editor metadata, not accumulate trailers. */
    for(i=0;i<100;i++) { Assign(project,0,BG_TEX_NONE);Assign(project,0,0xd4); }
    { DWORD after;NewPropsData(project,"PpendantZ",&after);CHECK(after==size); }
}
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
    CHECK(source.vertices[0].s==32 && source.vertices[3].s==64);
    CHECK(source.vertices[1].s==0 && source.vertices[4].s==0);
    CHECK(source.vertices[0].t==32 && source.vertices[2].t==0);
    ModelFreeSource(&source);
}
static void ActualPendant(const char *project,const char *path)
{
    ModelSource source={0};DWORD revision,count,i,bySlot[4]={0},size;
    const unsigned char *data;char exported[MAX_PATH];
    realpendant=TRUE;
    CHECK(NewPropsImport(project,"PpendantZ",path,FALSE,&count,&why) && count==80);
    CHECK(ModelEditsReadSource(project,"PpendantZ",&source,&revision,&why));
    CHECK(source.materials.count==4);
    for(i=0;i<count;i++)
    {
        DWORD slot=source.materials.faces[i].slot;
        CHECK(slot<4 && BG_TEX_ID(source.tags[i])==BG_TEX_NONE);bySlot[slot]++;
        if(slot==3) CHECK(source.vertices[i*3].a==25 && (source.flags[i]&BG_RENDER_BLEND));
    }
    CHECK(bySlot[0]==10 && bySlot[1]==34 && bySlot[2]==20 && bySlot[3]==16);
    ModelFreeSource(&source);Assign(project,0,0xd4);Assign(project,1,0xa93);
    CHECK(ModelEditsReadSource(project,"PpendantZ",&source,&revision,&why));
    for(i=0;i<count;i++)
    {
        DWORD slot=source.materials.faces[i].slot,k;
        CHECK(BG_TEX_ID(source.tags[i])==(slot==0?0xd4:slot==1?0xa93:BG_TEX_NONE));
        if(slot<2) for(k=0;k<3;k++)
        {
            CHECK(fabsf(source.vertices[i*3+k].s-source.materials.faces[i].uv[k*2]*(slot==0?16:48))<=1.f/64+1e-5f);
            CHECK(fabsf(source.vertices[i*3+k].t-source.materials.faces[i].uv[k*2+1]*(slot==0?64:32))<=1.f/64+1e-5f);
        }
        if(slot==3) CHECK(source.vertices[i*3].a==25 && BG_TEX_ID(source.tags[i])==BG_TEX_NONE);
    }
    ModelFreeSource(&source);CHECK(NewPropsSave(project,&why));NewPropsReset();CHECK(NewPropsOpen(project,&why));
    data=NewPropsData(project,"PpendantZ",&size);CHECK(ModelReadSource(data,size,&source,&why));
    snprintf(exported,sizeof(exported),"%s/actual-roundtrip.gltf",project);
    CHECK(ModelEditsExport(project,"PpendantZ",exported,&why));
    CHECK(NewPropsImport(project,"PpendantZ",exported,TRUE,&count,&why) && count==80);
    data=NewPropsData(project,"PpendantZ",&size);
    { ModelSource check={0};CHECK(ModelReadSource(data,size,&check,&why));
      CHECK(check.materials.count==source.materials.count && check.count==source.count);
      CHECK(!memcmp(check.materials.slots,source.materials.slots,4*sizeof(*source.materials.slots)));
      CHECK(!memcmp(check.materials.faces,source.materials.faces,count*sizeof(*source.materials.faces)));
      CHECK(!memcmp(check.vertices,source.vertices,count*3*sizeof(*source.vertices)));
      CHECK(!memcmp(check.tags,source.tags,count*sizeof(*source.tags)));
      CHECK(!memcmp(check.flags,source.flags,count*sizeof(*source.flags)));ModelFreeSource(&check); }
    ModelFreeSource(&source);
    ModelEditsReset();puts("PASS supplied pendant: 80 faces, four unassigned slots, 00D4/0A93 bindings, alpha 25 and save/export/reimport.");
}

static void CheckUvOrientation(const char *project)
{
    const char *variants[]={"display","native","legacy","primitive","invalid"};
    const float authored[]={.125f,.25f,.75f,-.125f,1.25f,.875f};
    for (int variant=0;variant<5;variant++)
    {
        char path[MAX_PATH];DWORD count=0;unsigned short *tags=NULL;BgRenderFlags *flags=NULL;
        ModelMaterials materials={0};BgVertex *vertices;
        snprintf(path,sizeof(path),"%s/uv-%s.glb",project,variants[variant]);
        vertices=GltfReadNewProp(path,project,&count,&tags,&flags,&materials,&why);
        if (variant==4) { CHECK(!vertices && strstr(why,"goldeneyeUvOrientation")); }
        else
        {
            CHECK(vertices && count==4 && materials.facecount==4);
            for (int corner=0;corner<3;corner++)
            {
                float s=authored[corner*2],t=authored[corner*2+1];
                if (variant==0) { s=1.f-s;t=1.f-t; }
                CHECK(vertices[corner].s==s && vertices[corner].t==t);
                CHECK(materials.faces[0].uv[corner*2]==s && materials.faces[0].uv[corner*2+1]==t);
            }
            /* Orientation metadata for one material must not affect another. */
            CHECK(vertices[3].s==1.f && vertices[3].t==1.f);
        }
        free(vertices);free(tags);free(flags);ModelMaterialsFree(&materials);
    }
    puts("PASS display/native UV orientation, tiling, legacy exports and material/primitive metadata.");
}

static void PaintedProp(const char *project, BOOL paint)
{
    ModelSource source={0}; DWORD revision,size;
    const unsigned char *data;
    unsigned char rgba[4]={17,99,201,163};
    CHECK(ModelEditsReadSource(project,"PsecondZ",&source,&revision,&why));
    if (paint)
    {
        ModelVertexPaint change;
        DWORD oldsize; const unsigned char *old=NewPropsData(project,"PsecondZ",&oldsize);
        unsigned char *snapshot=malloc(oldsize); CHECK(snapshot);memcpy(snapshot,old,oldsize);
        DWORD offset=source.vertexoffsets[0];
        CHECK(ModelEditsSetVertexColor(project,"PsecondZ",revision,0,rgba,&change,&why));
        CHECK(ModelEditsRestoreVertexColor(project,"PsecondZ",&change,FALSE,&why));
        data=NewPropsData(project,"PsecondZ",&size);
        CHECK(size==oldsize && !memcmp(data,snapshot,size));
        CHECK(ModelEditsRestoreVertexColor(project,"PsecondZ",&change,TRUE,&why));
        data=NewPropsData(project,"PsecondZ",&size);CHECK(size==oldsize);
        for(DWORD i=0;i<size;i++)
            CHECK(data[i]==(i>=offset+12 && i<offset+16 ? rgba[i-offset-12] : snapshot[i]));
        free(snapshot);ModelFreeSource(&source);
        CHECK(ModelEditsReadSource(project,"PsecondZ",&source,&revision,&why));
        rgba[3]=91;
        CHECK(ModelEditsSetVertexColor(project,"PsecondZ",revision,9,rgba,NULL,&why));
        ModelFreeSource(&source);
        CHECK(ModelEditsReadSource(project,"PsecondZ",&source,&revision,&why));
    }
    CHECK(source.vertices[0].r==17 && source.vertices[0].g==99 && source.vertices[0].b==201 && source.vertices[0].a==163);
    CHECK(source.vertices[9].r==17 && source.vertices[9].a==91);
    if (paint) CHECK(source.materials.count==4 && source.materials.faces[0].uv[0]==1.f);
    ModelFreeSource(&source);
}

int main(int argc,char **argv)
{
    const char *project=argv[1],*name;float scale;DWORD count,size,hash,oldsize,start,i;
    char path[MAX_PATH],base[MAX_PATH],source[MAX_PATH];RomFile rom={0};unsigned char *snapshot;
    const unsigned char *data;extern int test_fail_move;
    CHECK(argc==2 || argc==3);snprintf(base,sizeof(base),"%s/base.z64",project);
    rom.size=0x200000;rom.data=calloc(rom.size,1);CHECK(rom.data);
    Put(rom.data+0x114,2);Put(rom.data+0x118,CUSTOM_PROP_MANIFEST_KIND);
    Put(rom.data+0x11c,0x200);Put(rom.data+0x120,0x210);Put(rom.data+0x124,1);
    Put(rom.data+0x128,CUSTOM_PROP_DATA_KIND);Put(rom.data+0x134,1);Put(rom.data+0x200,1);
    Write(base,rom.data,rom.size);RomFree(&rom);
    CHECK(NewPropsOpen(project,&why));CHECK(NewPropsCount()==0);
    if(argc==3) { ActualPendant(project,argv[2]);return 0; }
    CheckUvOrientation(project);
    snprintf(source,sizeof(source),"%s/pendant.glb",project);
    CHECK(!NewPropsImport(project,"PnativeZ",source,FALSE,&count,&why));
    CHECK(NewPropsImport(project,"PpendantZ",source,FALSE,&count,&why));CHECK(count==4);
    CHECK(NewPropsDefinition(512,&name,&scale));CHECK(!strcmp(name,"PpendantZ") && fabsf(scale-.1f)<1e-6);
    { BOOL character=TRUE;int id=-1;
      CHECK(ObjectResolvePlaceableModel("PpendantZ",&character,&id) && !character && id==512); }
    CHECK(!NewPropsDefinition(511,NULL,NULL) && !NewPropsDefinition(513,NULL,NULL));
    CheckSlots(project);
    CHECK(!NewPropsImport(project,"PpendantZ",source,FALSE,&count,&why));
    CHECK(NewPropsImport(project,"PpendantZ",source,TRUE,&count,&why));
    snprintf(path,sizeof(path),"%s/missing.glb",project);
    CHECK(NewPropsImport(project,"PpendantZ",path,TRUE,&count,&why));
    CheckModel(project,4);data=NewPropsData(project,"PpendantZ",&size);hash=ModelDataHash(data,size);
    {
        const char *bad[]={"animation","skin","mask","collapse","range","line"};
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
    PaintedProp(project,TRUE);
    CHECK(ModelEditsSave(project,&why));CHECK(RomLoad(base,&rom,&why));CHECK(NewPropsExportToRom(project,&rom,&why));
    start=Word(rom.data+0x204);CHECK(start>=0x101000 && !(start&15));
    CHECK(Word(rom.data+start)==CUSTOM_PROP_MAGIC && Word(rom.data+start+4)==2);
    {
        DWORD at=start+Word(rom.data+start+16+64),length=Word(rom.data+start+16+68);
        CHECK(ModelMaterialsNativeSize(rom.data+at,length)==length);
        ModelSource native={0};CHECK(ModelReadSource(rom.data+at,length,&native,&why));
        CHECK(native.materials.count==0 && native.count==5);ModelFreeSource(&native);
        at=start+Word(rom.data+start+16+CUSTOM_PROP_ENTRY_SIZE+64);
        length=Word(rom.data+start+16+CUSTOM_PROP_ENTRY_SIZE+68);
        CHECK(ModelReadSource(rom.data+at,length,&native,&why));
        CHECK(native.vertices[0].r==17 && native.vertices[0].a==163 && native.vertices[9].a==91);
        ModelFreeSource(&native);
    }
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
    PaintedProp(project,FALSE);
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
