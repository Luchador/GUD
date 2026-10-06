#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "newprops.h"
#include "modeledits.h"
#include "modelcompile.h"
#include "characterload.h"
#include "idleposes.h"
#include "texload.h"
#include "objectload.h"
static const char *why="";
static void Culling(const ModelSource *source);
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);exit(1); } } while (0)
static DWORD Word(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD n) { p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static void Write(const char *path,const void *data,DWORD size)
{ FILE *f=fopen(path,"wb");OK(f && fwrite(data,1,size,f)==size && !fclose(f)); }
BOOL RomLoad(const char *path,RomFile *rom,const char **reason)
{
    FILE *f=fopen(path,"rb");OK(f);memset(rom,0,sizeof(*rom));OK(!fseek(f,0,SEEK_END));
    rom->size=ftell(f);rewind(f);rom->data=malloc(rom->size);OK(rom->data);
    OK(fread(rom->data,1,rom->size,f)==rom->size && !fclose(f));rom->info.size=rom->size;
    rom->info.manifestoffset=0x100;rom->info.entrycount=3;
    for (DWORD i=0;i<3;i++) { const unsigned char *p=rom->data+0x118+i*16;
        rom->info.entries[i]=(RomManifestEntry){Word(p),Word(p+4),Word(p+8),Word(p+12)}; }
    return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data);memset(rom,0,sizeof(*rom)); }
BOOL RomFindFile(const RomFile *rom,const char *name,DWORD *offset,DWORD *size,const char **reason)
{
    int which=!strcmp(name,"CdjbondZ") ? 0 : !strcmp(name,"CheadbrosnanZ") ? 1 : -1;
    if (which<0) { *reason="Not found";return FALSE; }
    *offset=Word(rom->data+0x220+which*8);*size=Word(rom->data+0x224+which*8);*reason="";return TRUE;
}
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { *w=*h=32;return TRUE; }
BOOL TexLoadProjectImage(const char *project,DWORD id,TexPixel *out,int *w,int *h)
{ *w=*h=32;memset(out,255,32*32*sizeof(*out));return TRUE; }
BOOL TexEncodePng(const TexPixel *pixels,int w,int h,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(8,1);return *data!=NULL; }
static void CheckCharacter(const char *project,const char *name,int id,int templateid)
{
    CharacterModelDefinition def,stock;int sourceid=-1;const char *stored=NULL;BOOL character=FALSE;int place=-1;
    OK(NewPropsCharacterId(name)==id && NewPropsCharacter(id,&stored,&sourceid));
    OK(!strcmp(stored,name) && sourceid==templateid);
    OK(CharacterGetModelDefinition(id,&def) && CharacterGetModelDefinition(templateid,&stock));
    OK(!strcmp(def.filename,name) && def.scale==stock.scale && def.hashead==stock.hashead);
    OK(CharacterModelKind(id)==CharacterModelKind(templateid));
    if (CharacterModelKind(id)==1) {
        OK(ObjectResolvePlaceableModel(name,&character,&place) && character && place==id);
        OK(CharacterBodySwitchCount(name)==7);
        ModelSource mesh={0};DWORD revision,size;OK(ModelEditsReadSource(project,name,&mesh,&revision,&why));
        const unsigned char *native=NewPropsData(project,name,&size);ModelCharacterAttachments attachments={0};
        /* Complete source includes every LOD; the animation path must accept it. */
        BgVertex *pose=ModelLoadAnimationPose(native,size,g_EditorPose_idle_unarmed,45,0,mesh.count,&why);
        OK(pose);free(pose);
        OK(ModelReadHeadAttachment(native,size,attachments.head.m[3]));ModelFreeSource(&mesh);
    } else OK(!ObjectResolvePlaceableModel(name,&character,&place));
    OK(!ModelGetPropDefinition(CUSTOM_PROP_BASE+1+id-CUSTOM_CHARACTER_BASE,NULL,NULL));
}
static short Half(const unsigned char *p) { return (short)((p[0]<<8)|p[1]); }
static void HeadLinks(const unsigned char *data,DWORD size,const ModelSource *source)
{
    OK(source->listcount==1);
    const ModelSourceList *p=source->lists;
    DWORD count=(unsigned short)Half(data+p->vertexpointer+4),points=(unsigned short)Half(data+p->vertexpointer+6);
    DWORD xyz=Word(data+p->vertexpointer+8)&0xffffffu,links=Word(data+p->pointusagepointer)&0xffffffu;
    OK(xyz<size && points<=(size-xyz)/16 && links<size && count<=(size-links)/2);
    unsigned char *seen=calloc(count,1);OK(seen);
    for(DWORD i=0;i<points;i++) {
        int v=Half(data+xyz+i*16+6);
        OK(!Word(data+xyz+i*16+8) && Half(data+xyz+i*16+12)==-1);
        while(v>=0) {
            OK((DWORD)v<count && !seen[v]);seen[v]=1;
            OK(!memcmp(data+xyz+i*16,data+p->vertexbase+v*16,6));v=Half(data+links+v*2);
        }
    }
    for(DWORD i=0;i<count;i++) { OK(seen[i]); }
    free(seen);
}
static void Bounds(const BgVertex *vertices,DWORD count,double min[3],double max[3])
{
    for(DWORD i=0;i<count*3;i++) {
        double p[3]={vertices[i].x,vertices[i].y,vertices[i].z};
        for(int a=0;a<3;a++) { if(!i || p[a]<min[a])min[a]=p[a];if(!i || p[a]>max[a])max[a]=p[a]; }
    }
}
static void RawHead(const char *project,const char *name,const char *path,BOOL fit,int id)
{
    ModelSource source={0},stock={0};GltfModelImport raw={0};BgRenderFlags *flags=NULL;
    DWORD triangles,revision,size,before,after;BOOL roundtrip=TRUE;
    OK(ModelEditsReadSource(project,"CheadbrosnanZ",&stock,&revision,&why));
    OK(GltfReadHeadImport(path,revision,project,&raw,&flags,&roundtrip,&why) && !roundtrip);
    OK(NewPropsImportCharacter(project,name,path,78,fit,&triangles,&why));
    CheckCharacter(project,name,id,78);
    OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    OK(source.count==raw.count && triangles==raw.count && source.materials.count==raw.materials.count);
    Culling(&source);
    double lo[3],hi[3],slo[3],shi[3];Bounds(raw.vertices,raw.count,lo,hi);Bounds(stock.vertices,stock.count,slo,shi);
    double scale=fit ? (shi[1]-slo[1])/(hi[1]-lo[1]) : 1;
    for(DWORD i=0;i<source.count*3;i++) {
        double a[3]={source.vertices[i].x,source.vertices[i].y,source.vertices[i].z};
        double b[3]={raw.vertices[i].x,raw.vertices[i].y,raw.vertices[i].z};
        for(int axis=0;axis<3;axis++) {
            double expected=fit ? (b[axis]-(lo[axis]+hi[axis])*.5)*scale+(slo[axis]+shi[axis])*.5 : b[axis];
            OK(fabs(a[axis]-expected)<=.501);
        }
        OK(source.vertices[i].r==raw.vertices[i].r && source.vertices[i].g==raw.vertices[i].g
            && source.vertices[i].b==raw.vertices[i].b && source.vertices[i].a==raw.vertices[i].a);
    }
    for(DWORD i=0;i<source.materials.count;i++) {
        OK(!strcmp(source.materials.slots[i].name,raw.materials.slots[i].name));
        OK(source.materials.slots[i].texture==BG_TEX_NONE);
    }
    OK(!memcmp(source.materials.faces,raw.materials.faces,raw.count*sizeof(*raw.materials.faces)));
    const unsigned char *data=NewPropsData(project,name,&size);OK(data);HeadLinks(data,size,&source);
    ModelFreeSource(&source);
    OK(ModelEditsSetMaterial(project,name,revision,0,5,&why));
    OK(ModelEditsReadSource(project,name,&source,&revision,&why));
    Culling(&source);
    for(DWORD i=0;i<source.count;i++) {
        DWORD slot=source.materials.faces[i].slot;OK(BG_TEX_ID(source.tags[i])==(slot ? BG_TEX_NONE : 5));
    }
    data=NewPropsData(project,name,&size);HeadLinks(data,size,&source);
    char exported[MAX_PATH];snprintf(exported,sizeof(exported),"%s/%s.gltf",project,name);
    OK(ModelEditsExport(project,name,exported,&why));
    OK(ModelEditsImport(project,name,exported,&before,&after,&why) && before==after && after==triangles);
    /* Replacing an existing model still requires its identity. */
    OK(!ModelEditsImport(project,name,path,&before,&after,&why));
    ModelFreeSource(&source);ModelFreeSource(&stock);GltfFreeModelImport(&raw);free(flags);
    printf("PASS raw head %s: %lu triangles, fitted=%d; geometry, colors, UVs, slots, texture assignment, collision chains and round trip.\n",name,(unsigned long)triangles,fit);
}
#include "bodies.c"
#include "headoffset.c"
int main(int argc,char **argv)
{
    OK(argc==3 || argc==6);const char *project=argv[1],*root=argv[2];char base[MAX_PATH],path[MAX_PATH],body[MAX_PATH],head[MAX_PATH];
    snprintf(base,sizeof(base),"%s/base.z64",project);RomFile rom={0};rom.size=0x200000;rom.data=calloc(rom.size,1);OK(rom.data);
    Put(rom.data+0x114,3);Put(rom.data+0x118,CUSTOM_PROP_MANIFEST_KIND);Put(rom.data+0x11c,0x200);Put(rom.data+0x120,0x210);Put(rom.data+0x124,1);
    Put(rom.data+0x128,CUSTOM_PROP_DATA_KIND);Put(rom.data+0x134,1);Put(rom.data+0x138,0x11111111);Put(rom.data+0x13c,0x1000);Put(rom.data+0x140,0x50000);
    Put(rom.data+0x200,1);Put(rom.data+0x20c,CUSTOM_PROP_FEATURE_MODEL_CATEGORIES|CUSTOM_PROP_FEATURE_CHARACTERS);
    const char *models[]={"CdjbondZ","CheadbrosnanZ"};
    for (int i=0;i<2;i++) {
        snprintf(path,sizeof(path),"%s/assets/obseg/chr/%s.bin",root,models[i]);FILE *f=fopen(path,"rb");OK(f);
        OK(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);DWORD offset=0x1000+i*0x20000;
        OK(size>0 && size<0x20000 && fread(rom.data+offset,1,size,f)==(size_t)size && !fclose(f));
        Put(rom.data+0x220+i*8,offset);Put(rom.data+0x224+i*8,size);
    }
    Write(base,rom.data,rom.size);RomFree(&rom);
    snprintf(body,sizeof(body),"%s/body.glb",project);snprintf(head,sizeof(head),"%s/head.glb",project);
    OK(ModelEditsExport(project,"CdjbondZ",body,&why));OK(ModelEditsExport(project,"CheadbrosnanZ",head,&why));
    DWORD count,before,after,bytes;snprintf(path,sizeof(path),"%s/prop.glb",project);
    OK(NewPropsImport(project,"PstaticZ",path,FALSE,&count,&why));
    OK(NewPropsImportCharacter(project,"CtestbodyZ",body,5,TRUE,&count,&why));
    OK(count>0 && NewPropsCharacterId("CtestbodyZ")==80);
    OK(!NewPropsImportCharacter(project,"CtestbodyZ",body,5,TRUE,&count,&why));
    OK(!NewPropsImportCharacter(project,"CbadZ",head,5,TRUE,&count,&why)); /* wrong source/rig */
    OK(!NewPropsImportCharacter(project,"CbadZ",head,41,TRUE,&count,&why)); /* watch hand */
    OK(!NewPropsImportCharacter(project,"CHEADBROSNANZ",head,78,TRUE,&count,&why));
    OK(NewPropsCount()==2);
    /* A head need not have a filename starting with Chead. */
    OK(NewPropsImportCharacter(project,"CactorZ",head,78,TRUE,&count,&why));
    CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    OK(ModelEditsExport(project,"CtestbodyZ",body,&why));
    OK(ModelEditsImport(project,"CtestbodyZ",body,&before,&after,&why) && before==after);
    /* Exported positions keep native character units, unlike static props. */
    ModelSource source={0};DWORD revision;OK(ModelEditsReadSource(project,"CtestbodyZ",&source,&revision,&why));
    float maxy=-1e10f;for(DWORD i=0;i<source.count*3;i++) if(source.vertices[i].y>maxy) maxy=source.vertices[i].y;
    OK(maxy>500);ModelFreeSource(&source);
    OK(ModelEditsReadSource(project,"CactorZ",&source,&revision,&why));ModelFreeSource(&source);
    OK(ModelEditsSetMaterial(project,"CactorZ",revision,0,BG_TEX_NONE,&why));
    snprintf(path,sizeof(path),"%s/raw-normal.glb",project);
    OK(!NewPropsImportCharacter(project,"CunboundbodyZ",path,5,TRUE,&count,&why));
    RawHead(project,"CgeometryZ",path,TRUE,82);RawHead(project,"CnativeheadZ",path,FALSE,83);
    const char *actors[]={"CheadmooreZ","CheadconneryZ","CheaddaltonZ"};
    for(int i=3;i<argc;i++) RawHead(project,actors[i-3],argv[i],TRUE,84+i-3);
    const char *invalid[]={"flat","collapsed","oversize","stale","skinned","animated","blend"};
    for(unsigned int i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        int previous=NewPropsCount();snprintf(path,sizeof(path),"%s/raw-%s.glb",project,invalid[i]);
        OK(!NewPropsImportCharacter(project,"CinvalidheadZ",path,78,i!=2,&count,&why));
        OK(why[0] && NewPropsCount()==previous && NewPropsCharacterId("CinvalidheadZ")==-1);
    }
    const char *bodies[]={"CmooreZ","CconneryZ","CdaltonZ"};
    for(int i=3;i<argc;i++) {
        snprintf(path,sizeof(path),"%s",argv[i]);char *slash=strrchr(path,'/');OK(slash);
        strcpy(slash+1,"body.glb");RawBody(project,bodies[i-3],path,TRUE,87+i-3);
        LegacyBodyRepair(project,bodies[i-3],i==3 ? 2 : i==5 ? 1 : 0);
    }
    snprintf(path,sizeof(path),"%s/raw-body.gltf",project);RawTemplateBody(project,path);
    RawBody(project,"CrawbodyZ",path,FALSE,84+(argc-3)*2);
    HeadOffsetTest(project,"CactorZ",0);
    for(int i=3;i<argc;i++) {
        HeadOffsetFixture(actors[i-3]);HeadOffsetTest(project,actors[i-3],i-2);
    }
    OK(NewPropsSave(project,&why));ModelEditsReset();OK(NewPropsOpen(project,&why));
    HeadOffsetSaved(project,"CactorZ",0);
    for(int i=3;i<argc;i++) HeadOffsetSaved(project,actors[i-3],i-2);
    CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    OK(ModelEditsReadSource(project,"CactorZ",&source,&revision,&why));
    OK(source.materials.slots[0].texture==BG_TEX_NONE);ModelFreeSource(&source);
    CheckCharacter(project,"CgeometryZ",82,78);CheckCharacter(project,"CnativeheadZ",83,78);
    for(int i=3;i<argc;i++) CheckCharacter(project,actors[i-3],84+i-3,78);
    for(int i=3;i<argc;i++) CheckCharacter(project,bodies[i-3],87+i-3,5);
    CheckCharacter(project,"CrawbodyZ",84+(argc-3)*2,5);
    OK(ModelEditsReadSource(project,"CgeometryZ",&source,&revision,&why));
    OK(source.materials.slots[0].texture==5 && source.count==4);ModelFreeSource(&source);
    OK(RomLoad(base,&rom,&why));OK(NewPropsExportToRom(project,&rom,&why));
    DWORD bank=Word(rom.data+0x204);OK(Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+84)==CUSTOM_CHARACTER_BODY);
    OK(Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88)==5 && Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+92)==80);
    Put(rom.data+0x20c,CUSTOM_PROP_FEATURE_MODEL_CATEGORIES);DWORD hash=ModelDataHash(rom.data,rom.size);
    OK(!NewPropsCheckRebase(project,&rom,&why) && !NewPropsExportToRom(project,&rom,&why));OK(hash==ModelDataHash(rom.data,rom.size));
    Put(rom.data+0x20c,3);Put(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88,0);OK(!NewPropsCheckRebase(project,&rom,&why));Put(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88,5);
    OK(NewPropsCheckRebase(project,&rom,&why));Write(base,rom.data,rom.size);RomFree(&rom);
    snprintf(path,sizeof(path),"%s/models/newprops.gnp",project);OK(DeleteFile(path));ModelEditsReset();
    OK(NewPropsOpen(project,&why));CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    HeadOffsetSaved(project,"CactorZ",0);
    for(int i=3;i<argc;i++) HeadOffsetSaved(project,actors[i-3],i-2);
    CheckCharacter(project,"CgeometryZ",82,78);CheckCharacter(project,"CnativeheadZ",83,78);
    for(int i=3;i<argc;i++) CheckCharacter(project,actors[i-3],84+i-3,78);
    for(int i=3;i<argc;i++) CheckCharacter(project,bodies[i-3],87+i-3,5);
    CheckCharacter(project,"CrawbodyZ",84+(argc-3)*2,5);
    OK(NewPropsData(project,"CactorZ",&bytes) && bytes>0);OK(NewPropsSave(project,&why));
    ModelEditsReset();puts("PASS real head/body GLB imports, stable IDs, rig/animation, placement, reimport units, save/reload, ROM extraction and incompatible rebase/export.");
    return 0;
}
