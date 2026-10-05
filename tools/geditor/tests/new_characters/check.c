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
int main(int argc,char **argv)
{
    OK(argc==3);const char *project=argv[1],*root=argv[2];char base[MAX_PATH],path[MAX_PATH],body[MAX_PATH],head[MAX_PATH];
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
    OK(NewPropsImportCharacter(project,"CtestbodyZ",body,5,&count,&why));
    OK(count>0 && NewPropsCharacterId("CtestbodyZ")==80);
    OK(!NewPropsImportCharacter(project,"CtestbodyZ",body,5,&count,&why));
    OK(!NewPropsImportCharacter(project,"CbadZ",head,5,&count,&why)); /* wrong source/rig */
    OK(!NewPropsImportCharacter(project,"CbadZ",head,41,&count,&why)); /* watch hand */
    OK(!NewPropsImportCharacter(project,"CHEADBROSNANZ",head,78,&count,&why));
    OK(NewPropsCount()==2);
    /* A head need not have a filename starting with Chead. */
    OK(NewPropsImportCharacter(project,"CactorZ",head,78,&count,&why));
    CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    OK(ModelEditsExport(project,"CtestbodyZ",body,&why));
    OK(ModelEditsImport(project,"CtestbodyZ",body,&before,&after,&why) && before==after);
    /* Exported positions keep native character units, unlike static props. */
    ModelSource source={0};DWORD revision;OK(ModelEditsReadSource(project,"CtestbodyZ",&source,&revision,&why));
    float maxy=-1e10f;for(DWORD i=0;i<source.count*3;i++) if(source.vertices[i].y>maxy) maxy=source.vertices[i].y;
    OK(maxy>500);ModelFreeSource(&source);
    OK(ModelEditsReadSource(project,"CactorZ",&source,&revision,&why));ModelFreeSource(&source);
    OK(ModelEditsSetMaterial(project,"CactorZ",revision,0,BG_TEX_NONE,&why));
    OK(NewPropsSave(project,&why));ModelEditsReset();OK(NewPropsOpen(project,&why));
    CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    OK(ModelEditsReadSource(project,"CactorZ",&source,&revision,&why));
    OK(source.materials.slots[0].texture==BG_TEX_NONE);ModelFreeSource(&source);
    OK(RomLoad(base,&rom,&why));OK(NewPropsExportToRom(project,&rom,&why));
    DWORD bank=Word(rom.data+0x204);OK(Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+84)==CUSTOM_CHARACTER_BODY);
    OK(Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88)==5 && Word(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+92)==80);
    Put(rom.data+0x20c,CUSTOM_PROP_FEATURE_MODEL_CATEGORIES);DWORD hash=ModelDataHash(rom.data,rom.size);
    OK(!NewPropsCheckRebase(project,&rom,&why) && !NewPropsExportToRom(project,&rom,&why));OK(hash==ModelDataHash(rom.data,rom.size));
    Put(rom.data+0x20c,3);Put(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88,0);OK(!NewPropsCheckRebase(project,&rom,&why));Put(rom.data+bank+16+CUSTOM_PROP_ENTRY_SIZE+88,5);
    OK(NewPropsCheckRebase(project,&rom,&why));Write(base,rom.data,rom.size);RomFree(&rom);
    snprintf(path,sizeof(path),"%s/models/newprops.gnp",project);OK(DeleteFile(path));ModelEditsReset();
    OK(NewPropsOpen(project,&why));CheckCharacter(project,"CtestbodyZ",80,5);CheckCharacter(project,"CactorZ",81,78);
    OK(NewPropsData(project,"CactorZ",&bytes) && bytes>0);OK(NewPropsSave(project,&why));
    ModelEditsReset();puts("PASS real head/body GLB imports, stable IDs, rig/animation, placement, reimport units, save/reload, ROM extraction and incompatible rebase/export.");
    return 0;
}
