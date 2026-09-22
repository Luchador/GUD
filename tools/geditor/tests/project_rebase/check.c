#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include "projectrebase.h"
#include "romexport.h"
#include "imageedits.h"
#include "texrom.h"
#include "modelcompile.h"
#include "modeledits.h"
#include "newprops.h"
#include "setupload.h"

/* PNG encoding uses Windows WIC, outside this host test. */
BOOL TexEncodePng(const TexPixel *pixels,int width,int height,unsigned char **data,DWORD *size)
{ *size=8;*data=calloc(*size,1);return *data!=NULL; }
#define SIZE 0x200000u
#define SHIFT 0x8000u
#define MANIFEST 0x101000u
#define CMAP 0x110000u
#define TABLE (CMAP+0x100u)
#define LEVELS (CMAP+0x200u)
#define TEXTURES (CMAP+0x1000u)
#define CONFIG (CMAP+0xb000u)
#define IMAGES 0x150000u
#define OBJECTS 0x160000u
#define MODEL (OBJECTS+640u)
static const char *why="";
#define OK(expr) do { if (!(expr)) { fprintf(stderr,"%s:%d: %s: %s\n",__FILE__,__LINE__,#expr,why); abort(); } } while(0)
static void Put32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Get32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Float(unsigned char *p,float f) { DWORD u;memcpy(&u,&f,4);Put32(p,u); }
static void Save(const char *path,const void *p,size_t size)
{ FILE *f=fopen(path,"wb");OK(f && fwrite(p,1,size,f)==size && !fclose(f)); }
static unsigned char *Read(const char *path,DWORD *size)
{
    FILE *f=fopen(path,"rb");unsigned char *p;OK(f);OK(!fseek(f,0,SEEK_END));*size=ftell(f);rewind(f);
    p=malloc(*size);OK(p && fread(p,1,*size,f)==*size && !fclose(f));return p;
}
static void Path(char out[MAX_PATH],const char *dir,const char *name)
{ OK(snprintf(out,MAX_PATH,"%s/%s",dir,name)<MAX_PATH); }
static void Folder(const char *dir,const char *name)
{ char path[MAX_PATH];Path(path,dir,name);OK(CreateDirectory(path,NULL)); }
static DWORD Hash(const char *path)
{ DWORD size,hash;unsigned char *p=Read(path,&size);hash=TexDataHash(p,size);free(p);return hash; }
static void Same(const char *a,const char *b,const char *name)
{ char x[MAX_PATH],y[MAX_PATH];Path(x,a,name);Path(y,b,name);OK(Hash(x)==Hash(y)); }
static void Entry(unsigned char *data,DWORD shift,int i,const char *kind,DWORD start,DWORD end,DWORD flags)
{
    unsigned char *p=data+MANIFEST+shift+24+i*16;
    memcpy(p,kind,4);Put32(p+4,start+shift);Put32(p+8,end ? end+shift : 0);Put32(p+12,flags);
}
static unsigned char *Fixture(DWORD shift,const unsigned char *model,DWORD modelsize)
{
    unsigned char *data=calloc(SIZE,1),*row=data+LEVELS+shift,*record;
    DWORD i,size,strings=CMAP+shift+0x400,names[6],vbase=0x80000000u+shift*2;
    const char *namestr[]={"Jungle","UsetuptestZ","bg/bg_test.seg","Tbg_test_stanZ","Pjungle3_treeZ",""};
    TexPixel pixels[64];TexImportOptions options={1,0,1,2};
    Put32(data,0x80371240);memcpy(data+0x20,"GOLDENEYE",9);data[0x2000]=shift ? 0x22 : 0x11;
    memcpy(data+MANIFEST+shift,"GUDGEDITORMANIF",16);
    Put32(data+MANIFEST+shift+16,3);Put32(data+MANIFEST+shift+20,17);
    for(i=0;i<64;i++) { pixels[i]=(TexPixel){64,128,(i%4)*64,(i%2)*255}; }
    OK(TexEncodeRecord(pixels,8,8,&options,&record,&size,&why));memcpy(data+IMAGES+shift,record,size);free(record);
    Entry(data,shift,0,"IMGS",IMAGES,IMAGES+size,0);
    Entry(data,shift,1,"OBSG",OBJECTS,MODEL+modelsize+156,0);
    Entry(data,shift,2,"MUSF",0x180000,0x181000,0);
    Entry(data,shift,3,"STGT",LEVELS,LEVELS+36,1);
    Entry(data,shift,4,"CMAP",CMAP,CMAP+0x10000,vbase);
    Entry(data,shift,5,"FTBL",TABLE,0,0);
    Entry(data,shift,6,"ENVT",CMAP+0x800,0,104);
    Entry(data,shift,7,"TXTB",TEXTURES,TEXTURES+4097*8,4096);
    Entry(data,shift,8,"TXCF",CONFIG,CONFIG+8,1);
    Entry(data,shift,9,"MONA",CMAP+0xd000,CMAP+0xd034,52);
    Entry(data,shift,10,"MONT",CMAP+0xd100,CMAP+0xd10c,12);
    Entry(data,shift,11,"MOND",CMAP+0xd200,CMAP+0xd274,116);
    Entry(data,shift,15,"NPRP",CONFIG+16,CONFIG+32,1);
    Entry(data,shift,16,"NPMD",0,0,1);
    Put32(data+MANIFEST+shift+24+16*16+4,0);
    Put32(data+CONFIG+shift+16,1);
    for(i=0;i<6;i++) { names[i]=vbase+strings-CMAP-shift;strcpy((char *)data+strings,namestr[i]);strings+=strlen(namestr[i])+1; }
    for(i=0;i<3;i++)
    {
        DWORD off=CMAP+shift+0xc000+i*16,rows=CMAP+shift+0xc100+i*0x100;
        Entry(data,shift,12+i,i==0 ? "CHRM" : i==1 ? "PROP" : "ITEM",off-shift,off-shift+16,0x80000000);
        Put32(data+off,vbase+rows-CMAP-shift);Put32(data+off+4,1);Put32(data+off+8,i==0 ? 20 : i==1 ? 12 : 56);Put32(data+off+12,1);
        Put32(data+rows,vbase+0xe000);Put32(data+rows+4,names[4]);Float(data+rows+8,1);
        if(i==2) { Put32(data+rows+12,vbase+0xe100); }
    }
    Put32(row,21);for(i=0;i<4;i++) { Put32(row+4+i*4,names[i]); }
    Float(row+20,1.0f);Float(row+24,1.0f);row[29]=5;row[31]=6;row[33]=7;
    Put32(data+CMAP+shift+0x800,21);Put32(data+CMAP+shift+0x804,1);
    Float(data+CMAP+shift+0x808,10);Float(data+CMAP+shift+0x80c,1000);
    Put32(data+CMAP+shift+0x824,996);Put32(data+CMAP+shift+0x828,1000);
    data[CMAP+shift+0x82f]=1;Float(data+CMAP+shift+0x830,shift ? 7500 : 5000);
    Float(data+CMAP+shift+0x838,70);Float(data+CMAP+shift+0x83c,199);Float(data+CMAP+shift+0x840,186);
    if (shift)
    {
        /* Rebase an old 104-byte environment onto the compact native format. */
        Entry(data,shift,6,"ENVT",CMAP+0x800,0,88);
        memmove(data+CMAP+shift+0x810,data+CMAP+shift+0x820,72);
        memset(data+CMAP+shift+0x858,0,16);
    }
    Put32(data+CONFIG+shift,IMAGES+shift);Put32(data+CONFIG+shift+4,1);
    Put32(data+TEXTURES+shift,0x12000000|size);Put32(data+TEXTURES+shift+8,0xffff);
    /* First file is the named empty placeholder, then setup/BG/stan/model. */
    Put32(data+TABLE+shift+4,names[5]);
    for(i=1;i<=4;i++)
    {
        DWORD at=TABLE+shift+i*12,offset=i==1 ? OBJECTS : i==2 ? OBJECTS+64 : i==3 ? OBJECTS+576 : MODEL;
        Put32(data+at,i);Put32(data+at+4,names[i]);Put32(data+at+8,offset+shift);
    }
    /* An existing MP resource becomes selectable when a later ROM exposes
     * its catalog row. Its empty native setup is independent of the solo one. */
    strcpy((char *)data+CMAP+shift+0x4e0,"Ump_setuptestZ");
    Put32(data+TABLE+shift+5*12,5);Put32(data+TABLE+shift+5*12+4,vbase+0x4e0);
    Put32(data+TABLE+shift+5*12+8,MODEL+modelsize+shift);
    Put32(data+OBJECTS+shift+12,48); /* Native setup with a two-word objective command. */
    Put32(data+OBJECTS+shift+48,25);Put32(data+OBJECTS+shift+56,48);
    row=data+OBJECTS+shift+64; /* Valid native BG with a three-vertex batch. */
    Put32(row+4,32);Put32(row+56,0x0e000080);Put32(row+60,0x0e000100);
    Put32(row+124,64);Put32(row+252,24);Put32(row+256,0x04200030);
    Put32(row+264,0xbf000000);Put32(row+268,0x00000a14);Put32(row+272,0xb8000000);
    row=data+OBJECTS+shift+576; /* One real native STAN tile, terminator and footer. */
    Put32(row+4,12);Put32(row+12,0x10001);row[18]=0x30;row[19]=0x12;
    row[33]=100;row[37]=100;memcpy(row+52,"unstric",8);
    memcpy(data+MODEL+shift,model,modelsize);
    row=data+MODEL+modelsize+shift; /* Empty MP setup with valid list terminators. */
    Put32(row+8,40);Put32(row+40,9);Put32(row+24,44);Put32(row+28,88);
    return data;
}
static void NoTemps(const char *dir)
{
    DIR *d=opendir(dir);struct dirent *e;OK(d);
    while((e=readdir(d))) { OK(strncmp(e->d_name,".geditor-rebase-",16)); }closedir(d);
}
static void CatalogRebase(const GEditorProject *source,const char *incoming,const char *parent)
{
    DWORD size,offset,span,soloHash;unsigned char *data=Read(incoming,&size);
    DWORD vbase=0x80000000u+SHIFT*2;
    unsigned char *solo=data+LEVELS+SHIFT,*mp=solo+36,*title=mp+36,*last=title+36;
    char path[MAX_PATH],expanded[MAX_PATH],exported[MAX_PATH];
    GEditorProject updated,loaded,again;ProjectRebaseReport report;RomFile rom={0};
    SetupFile setup={0};SetupIntroEntry *entries=NULL;DWORD count;BOOL changed;
    SetupIntroEdit edit={SETUP_INTRO_ADD,{0,SETUP_INTRO_AMMO,{AMMO_9MM,100}}};
    memcpy(mp,solo,36);Put32(mp,421);Put32(mp+4,vbase+0x480);Put32(mp+8,vbase+0x4e0);
    strcpy((char *)data+CMAP+SHIFT+0x480,"Jungle (MP)");
    memset(title,0,72);Put32(title,90);Put32(title+4,vbase+0x490);
    strcpy((char *)data+CMAP+SHIFT+0x490,"Title");
    Float(title+20,1);Float(title+24,1);memset(title+28,0xff,6);
    Put32(last,57);Put32(last+12,vbase+0x4a0);
    strcpy((char *)data+CMAP+SHIFT+0x4a0,"bg/bgx.seg");
    Entry(data,SHIFT,3,"STGT",LEVELS,LEVELS+4*36,4);
    Path(expanded,parent,"expanded.z64");Save(expanded,data,size);
    OK(ProjectRebaseCheck(source,expanded,&report,&why));
    OK(ProjectRebaseCreate(source,expanded,parent,"Expanded",&updated,&report,&why));
    OK(source->levelcount==1&&updated.levelcount==3);
    OK(updated.levels[0].music==source->levels[0].music);
    OK(updated.levels[1].levelID==421&&!strcmp(updated.levels[1].setupname,"Ump_setuptestZ"));
    OK(updated.levels[2].levelID==90&&!updated.levels[2].bgname[0]&&!updated.levels[2].setupname[0]);
    OK(ProjectRead(updated.geppath,&loaded)&&RomExportRefreshProjectLevelMetadata(&loaded,&why));
    Path(path,updated.dir,"setup/UsetuptestZ.set");soloHash=Hash(path);
    OK(SetupLoadProjectFile(updated.dir,updated.levels[1].setupname,&setup,&why));
    OK(SetupFileEditIntroEquipment(&setup,&edit,&changed,&why)&&changed);
    OK(SetupSaveProjectFile(updated.dir,&setup,&why));SetupFileFree(&setup);
    OK(Hash(path)==soloHash);
    OK(RomExportCreate(&updated,"ExpandedPlayable",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&rom,&why)&&rom.info.levelcount==3);
    OK(RomFindFile(&rom,"Ump_setuptestZ",&offset,&span,&why));
    setup.data=malloc(span);OK(setup.data);memcpy(setup.data,rom.data+offset,span);setup.size=span;
    strcpy(setup.name,"Ump_setuptestZ");
    OK(SetupFileGetIntroEquipment(&setup,&entries,&count,&why)&&count==1&&entries[0].value[1]==100);
    free(entries);SetupFileFree(&setup);RomFree(&rom);
    OK(ProjectRebaseCreate(&updated,expanded,parent,"ExpandedAgain",&again,&report,&why));
    OK(again.levelcount==3);Same(updated.dir,again.dir,"setup/Ump_setuptestZ.set");
    /* Removing an old ID or introducing duplicates must still be rejected. */
    Put32(solo,22);Save(expanded,data,size);OK(!ProjectRebaseCheck(source,expanded,&report,&why));
    Put32(solo,21);Put32(mp,21);Save(expanded,data,size);OK(!ProjectRebaseCheck(source,expanded,&report,&why));
    free(data);
    puts("PASS: additive MP/Title rebase, reopen, separate MP setup edits, playable export, repeat rebase and removed/duplicate-ID rejection.");
}
static void Reject(const GEditorProject *project,const char *rom,const char *parent,const char *name)
{
    GEditorProject output;ProjectRebaseReport report;char dest[MAX_PATH],base[MAX_PATH];DWORD hash;
    Path(base,project->dir,"base.z64");hash=Hash(base);
    OK(!ProjectRebaseCreate(project,rom,parent,name,&output,&report,&why));OK(why[0] && !output.dir[0]);
    Path(dest,parent,name);OK(GetFileAttributes(dest)==INVALID_FILE_ATTRIBUTES);OK(Hash(base)==hash);NoTemps(parent);
}
static void ImageRebases(const GEditorProject *source,const char *incoming,const char *parent)
{
    GEditorProject grown,shrunk,again;ProjectRebaseReport report;
    RomFile rom,before,after;TexRomBank bank,a,b;
    char large[MAX_PATH],path[MAX_PATH],saved[MAX_PATH],exported[MAX_PATH];
    DWORD id,sourcehash,romhash,size,othersize,i;unsigned char *data,*other;
    TexPixel pixels[64],preview[64];int width,height;const TexImportOptions options={1,1,3,4};
    /* Incoming ROM absorbs the project's three imported slots verbatim, then
     * adds a new stock slot with legacy surface/detail flags. */
    OK(RomLoad(incoming,&rom,&why));OK(ImageEditsExportToRom(source->dir,&rom,&why));
    OK(TexRomReadBank(&rom,&bank,&why) && bank.count==4);
    {
        const unsigned char *records[5]={0};DWORD sizes[5]={0};unsigned char surfaces[5]={0};
        records[4]=rom.data+bank.images;sizes[4]=Get32(rom.data+bank.table)&0xffffffu;
        OK(TexRomUpdateImages(&rom,&bank,records,sizes,surfaces,5,&why));
        Put32(rom.data+bank.table+4*8,0xfa000000u|sizes[4]);
        Put32(rom.data+bank.table+4*8+4,0x12345678);
    }
    Path(large,parent,"larger-images.z64");Save(large,rom.data,rom.size);RomFree(&rom);
    Path(saved,source->dir,"images/native/0001.gtex");sourcehash=Hash(saved);romhash=Hash(large);
    OK(ProjectRebaseCheck(source,large,&report,&why) && report.imagesadded==4 && !report.imagesretained);
    OK(Hash(saved)==sourcehash && Hash(large)==romhash);NoTemps(parent);
    OK(ProjectRebaseCreate(source,large,parent,"MoreImages",&grown,&report,&why));
    Path(path,grown.dir,"base.z64");OK(Hash(path)==romhash);
    OK(RomLoad(path,&rom,&why));OK(TexRomReadBank(&rom,&bank,&why) && bank.count==5);
    /* Only the eight fingerprint bytes change, including GTI3 source paths,
     * GTI2 imports and deleted-image metadata. */
    for(i=1;i<=3;i++)
    {
        char relative[64];snprintf(relative,sizeof(relative),"images/native/%04lX.gtex",(unsigned long)i);
        Path(path,source->dir,relative);data=Read(path,&size);
        Path(path,grown.dir,relative);other=Read(path,&othersize);
        OK(size==othersize && !memcmp(data,other,4) && !memcmp(data+12,other+12,size-12));
        OK(Get32(other+4)==bank.hash && Get32(other+8)==bank.count);
        if(i==1) { OK(!memcmp(other,"GTI3",4) && !strcmp((char *)other+40,"C:\\art\\missing-original.bmp")); }
        free(data);free(other);
    }
    Same(source->dir,grown.dir,"bg/bg_test.seg");Same(source->dir,grown.dir,"models/newprops.gnp");
    Same(source->dir,grown.dir,"models/native/Pjungle3_treeZ.gmodel");
    Same(source->dir,grown.dir,"models/objects/PpendantZ.gltf");
    Same(source->dir,grown.dir,"images/0001.bmp");
    OK(TexDecodeRecord(rom.data+bank.images,Get32(rom.data+bank.table)&0xffffffu,pixels,&width,&height));
    OK(TexLoadSavedProjectImage(grown.dir,4,preview,&width,&height) && width==8 && height==8);
    OK(!memcmp(pixels,preview,sizeof(pixels)));
    OK(ImageEditsGetDeletedPixels(grown.dir,2,preview,&width,&height) && width==1 && height==1 && !preview[0].a);
    OK(ImageEditsNextId(grown.dir,&id,&why) && id==5);
    /* Keep an override and a new project-only import on top of the larger base. */
    OK(ImageEditsReplace(grown.dir,0,pixels,8,8,&options,NULL,&why));
    OK(ImageEditsImport(grown.dir,pixels,8,8,&options,NULL,&id,&why) && id==5);
    OK(ImageEditsSave(grown.dir,&why));ImageEditsReset();
    OK(RomExportCreate(&grown,"BeforeShrink",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&before,&why));OK(TexRomReadBank(&before,&a,&why) && a.count==6);
    /* This is the lamp case: rebase onto a clean build lacking baked images.
     * The retained suffix must not become an override that loses detail flags. */
    OK(ProjectRebaseCheck(&grown,incoming,&report,&why) && report.imagesretained==4 && !report.imagesadded);
    OK(ProjectRebaseCreate(&grown,incoming,parent,"FewerImages",&shrunk,&report,&why));
    Path(path,shrunk.dir,"base.z64");OK(RomLoad(path,&after,&why));OK(TexRomReadBank(&after,&b,&why));
    OK(b.count==bank.count && b.hash==bank.hash && b.imagebytes==bank.imagebytes);
    OK(!memcmp(after.data+b.images,rom.data+bank.images,bank.imagebytes));
    OK(!memcmp(after.data+b.table,rom.data+bank.table,(bank.count+1)*8));RomFree(&after);RomFree(&rom);
    Same(grown.dir,shrunk.dir,"images/native/0000.gtex");Same(grown.dir,shrunk.dir,"images/native/0001.gtex");
    Same(grown.dir,shrunk.dir,"images/native/0002.gtex");Same(grown.dir,shrunk.dir,"images/native/0005.gtex");
    Same(grown.dir,shrunk.dir,"images/0004.bmp");Same(grown.dir,shrunk.dir,"models/newprops.gnp");
    OK(RomExportCreate(&shrunk,"AfterShrink",parent,exported,sizeof(exported),&why));
    OK(RomLoad(exported,&after,&why));OK(TexRomReadBank(&after,&b,&why));
    OK(a.count==b.count && a.imagebytes==b.imagebytes && a.hash==b.hash);
    OK(!memcmp(before.data+a.images,after.data+b.images,a.imagebytes));
    OK(!memcmp(before.data+a.table,after.data+b.table,(a.count+1)*8));RomFree(&before);RomFree(&after);
    OK(ImageEditsNextId(shrunk.dir,&id,&why) && id==6);
    OK(ProjectRebaseCreate(&shrunk,incoming,parent,"RepeatImages",&again,&report,&why));
    /* Edited BMPs, settings and missing metadata must not be silently absorbed
     * by incoming stock IDs, even if their older native payload would match. */
    Path(path,source->dir,"images/0001.bmp");data=Read(path,&size);
    for(i=0;i<64;i++) { preview[i]=(TexPixel){255,0,255,255}; }
    OK(TexWriteBmp(path,preview,8,8));
    OK(!ProjectRebaseCheck(source,large,&report,&why) && strstr(why,"Image 0001"));
    Reject(source,large,parent,"BmpCollision");Save(path,data,size);free(data);
    Path(path,source->dir,"images/native/0001.gtex");data=Read(path,&size);data[28]^=1;Save(path,data,size);
    Reject(source,large,parent,"SettingsCollision");data[28]^=1;Save(path,data,size);free(data);
    Path(path,source->dir,"images/native/0003.gtex");Path(saved,source->dir,"images/native/0003.hold");
    OK(MoveFileEx(path,saved,0));Reject(source,large,parent,"OrphanImage");OK(MoveFileEx(saved,path,0));
    /* A publication failure after fingerprint migration still leaves no copy. */
    test_fail_move=1;Reject(source,large,parent,"ImagePublishFail");
    Path(path,source->dir,"images/native/0001.gtex");OK(Hash(path)==sourcehash && Hash(large)==romhash);
    NoTemps(parent);OK(RomExportValidateProject(source,&why));
    puts("PASS: larger/smaller image banks, stable model/BG references, imported/deleted slots, exact native flags, source paths, new previews, further imports, repeat rebase and image collision rollback.");
}

#include "retired.c"
#include "memory.c"
#include "occluders.c"
#include "objectfade.c"

int main(int argc,char **argv)
{
    unsigned char *model,*old,*next,*edited;DWORD modelsize,i,offset,span,size,id;
    char oldpath[MAX_PATH],nextpath[MAX_PATH],path[MAX_PATH],backup[MAX_PATH],exported[MAX_PATH],destination[MAX_PATH];
    GEditorProject project,rebased,loaded,again;ProjectRebaseReport report;RomFile rom,output;ModelSource native;
    TexPixel pixels[64];TexImportOptions options={1,1,3,4};TexRomBank bank;
    assert(argc==4);model=Read(argv[2],&modelsize);old=Fixture(0,model,modelsize);next=Fixture(SHIFT,model,modelsize);
    Path(oldpath,argv[1],"old.z64");Path(nextpath,argv[1],"new.z64");Save(oldpath,old,SIZE);
    /* Incoming setup and sound changes; our BG/music changes must survive. */
    next[OBJECTS+SHIFT+52]=0x56;next[LEVELS+SHIFT+31]=8;Save(nextpath,next,SIZE);
    OK(RomLoad(oldpath,&rom,&why));OK(ProjectCreate("Original",argv[1],&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));
    OK(RomExportRefreshProjectLevelMetadata(&project,&why));
    EditorEnvironment environment=project.environments.rows[0];
    OK(EnvironmentParseField(&environment,2,"6000",&why));
    OK(EnvironmentParseField(&environment,6,"25",&why));
    OK(EnvironmentSet(&project.environments,&project.environmentOverrides,&environment,&why));
    Folder(project.dir,"bg");Folder(project.dir,"setup");Folder(project.dir,"stan");Folder(project.dir,"images");
    Folder(project.dir,"models");Folder(project.dir,"models/native");Folder(project.dir,"models/objects");Folder(project.dir,"notes");
    for(i=1;i<=5;i++)
    {
        if(i==4) { continue; } /* Models use their own edit sidecars. */
        char name[64];OK(RomGetFileByIndex(&rom,i,name,sizeof(name),&offset,&span));
        OK(RomExportProjectResourcePath(&project,name,path,sizeof(path))==1);Save(path,rom.data+offset,span);
    }
    OK(RomExportProjectResourcePath(&project,"bg/bg_test.seg",path,sizeof(path))==1);
    edited=Read(path,&size);edited[128]=1;Save(path,edited,size);free(edited);
    project.levels[0].music=12;project.levels[0].levelscale=.375f;project.levels[0].renderScale=.875f;OK(ProjectSave(&project,&why));
    Path(path,project.dir,"notes/.artist-note");Save(path,"keep me",7);
    /* A real parsed native model, with a changed position and original fingerprint. */
    OK(ModelReadSource(model,modelsize,&native,&why));OK(native.count>0);offset=native.vertexoffsets[0];
    edited=malloc(modelsize+16);memcpy(edited,"GMD1",4);Put32(edited+4,ModelDataHash(model,modelsize));
    Put32(edited+8,modelsize);memcpy(edited+16,model,modelsize);edited[16+offset+1]^=1;
    Put32(edited+12,ModelDataHash(edited+16,modelsize));ModelFreeSource(&native);
    Path(path,project.dir,"models/native/Pjungle3_treeZ.gmodel");Save(path,edited,modelsize+16);free(edited);
    Path(path,project.dir,"models/objects/Pjungle3_treeZ.gltf");Save(path,"preview sidecar",15);
    for(i=0;i<64;i++) { pixels[i]=(TexPixel){64,128,(i%4)*64,(i%2)*255}; }
    OK(ImageEditsImport(project.dir,pixels,8,8,&options,"C:\\art\\missing-original.bmp",&id,&why) && id==1);
    OK(ImageEditsSave(project.dir,&why));
    OK(ImageEditsImport(project.dir,pixels,8,8,&options,NULL,&id,&why) && id==2);
    OK(ImageEditsSave(project.dir,&why));OK(ImageEditsDelete(project.dir,2,&why));OK(ImageEditsSave(project.dir,&why));ImageEditsReset();
    Path(path,project.dir,"images/0000.bmp");OK(TexWriteBmp(path,pixels,8,8));
    OK(NewPropsImport(project.dir,"PpendantZ",argv[3],FALSE,&size,&why) && size==4);
    /* The slot accepts an image absent from base.z64, including an unsaved edit. */
    OK(ImageEditsImport(project.dir,pixels,8,8,&options,NULL,&id,&why) && id==3);
    { DWORD revision;ModelSource slots={0};
      OK(ModelEditsReadSource(project.dir,"PpendantZ",&slots,&revision,&why));ModelFreeSource(&slots);
      OK(ModelEditsSetMaterial(project.dir,"PpendantZ",revision,0,id,&why));
      OK(ModelEditsReadSource(project.dir,"PpendantZ",&slots,&revision,&why));ModelFreeSource(&slots);
      OK(ModelEditsSetMaterial(project.dir,"PpendantZ",revision,1,1,&why)); }
    OK(ImageEditsSave(project.dir,&why));
    OK(NewPropsSave(project.dir,&why));
    OK(ProjectRebaseCheck(&project,nextpath,&report,&why));OK(report.kept==1 && report.updated==1 && !report.conflicts);
    OK(ProjectRebaseCreate(&project,nextpath,argv[1],"Updated",&rebased,&report,&why));NoTemps(argv[1]);
    OK(project.levels[0].clouds.enabled && project.levels[0].clouds.height==5000);
    OK(rebased.levels[0].clouds.enabled && rebased.levels[0].clouds.height==7500);
    OK(ProjectRead(rebased.geppath,&loaded));OK(!strcmp(loaded.name,"Updated"));
    OK(RomExportRefreshProjectLevelMetadata(&loaded,&why));
    OK(loaded.levels[0].levelscale==.375f&&loaded.levels[0].renderScale==.875f);
    OK(loaded.levels[0].fog.farclip==6000&&loaded.levels[0].backgroundcolor[0]==25&&loaded.levels[0].clouds.height==7500);
    OK(loaded.levels[0].music==12 && loaded.levels[0].bgsound==8);
    Same(project.dir,rebased.dir,"bg/bg_test.seg");Same(project.dir,rebased.dir,"stan/Tbg_test_stanZ.stan");
    Same(project.dir,rebased.dir,"models/native/Pjungle3_treeZ.gmodel");Same(project.dir,rebased.dir,"models/objects/Pjungle3_treeZ.gltf");
    Same(project.dir,rebased.dir,"models/newprops.gnp");
    Same(project.dir,rebased.dir,"models/objects/PpendantZ.gltf");
    Same(project.dir,rebased.dir,"images/native/0001.gtex");Same(project.dir,rebased.dir,"images/native/0002.gtex");
    Same(project.dir,rebased.dir,"images/native/0003.gtex");
    Same(project.dir,rebased.dir,"images/0001.bmp");Same(project.dir,rebased.dir,"notes/.artist-note");
    Path(path,rebased.dir,"Original.gep");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    Path(path,rebased.dir,"base.z64");OK(Hash(path)==Hash(nextpath));Path(path,project.dir,"base.z64");OK(Hash(path)==Hash(oldpath));
    OK(RomExportCreate(&rebased,"Playable",argv[1],exported,sizeof(exported),&why));OK(RomLoad(exported,&output,&why));
    OK(output.info.levels[0].levelscale==.375f&&output.info.levels[0].renderScale==.875f);
    OK(output.data[0x2000]==0x22 && output.info.levels[0].music==12 && output.info.levels[0].bgsound==8);
    OK(output.info.levels[0].fog.farclip==6000&&output.info.levels[0].backgroundcolor[0]==25&&output.info.levels[0].clouds.height==7500);
    OK(RomGetFileByIndex(&output,1,path,sizeof(path),&offset,&span) && output.data[offset+52]==0x56);
    OK(RomGetFileByIndex(&output,2,path,sizeof(path),&offset,&span));
    { DWORD rooms=Get32(output.data+offset+4)&0xffffffu;
      DWORD vertices=Get32(output.data+offset+rooms+24)&0xffffffu;
      OK(output.data[offset+vertices]==1); }
    OK(RomGetFileByIndex(&output,4,path,sizeof(path),&offset,&span) && span==modelsize && memcmp(output.data+offset,model,modelsize));
    OK(TexRomReadBank(&output,&bank,&why) && bank.count==4);
    {
        RomManifestEntry *props=&output.info.entries[16];
        OK(props->kind==CUSTOM_PROP_DATA_KIND && props->romend>props->romstart);
        OK(!memcmp(output.data+props->romstart,"GNP1",4));
        OK(props->romend<=bank.images || props->romstart>=bank.images+bank.imagebytes);
        OK(NewPropsCheckRebase(rebased.dir,&output,&why));
    }
    RomFree(&output);
    OK(ProjectRebaseCreate(&rebased,nextpath,argv[1],"Again",&again,&report,&why));
    OK(again.levels[0].levelscale==.375f&&again.levels[0].renderScale==.875f);
    puts("PASS: project scale edits, relocated ROM tables/code, three-way asset/settings merge, native model edits, imported/deleted images, source settings, sidecars, reopen, ROM export and repeat rebase.");
    ImageRebases(&project,nextpath,argv[1]);
    Float(next+CMAP+SHIFT+0x80c,5000);Save(nextpath,next,SIZE);
    OK(!ProjectRebaseCheck(&project,nextpath,&report,&why)&&report.conflicts&&strstr(report.details,"farclip"));
    Reject(&project,nextpath,argv[1],"EnvironmentConflict");Float(next+CMAP+SHIFT+0x80c,1000);Save(nextpath,next,SIZE);
    puts("PASS: saved environment overrides survive reopen, relocated ROM export and repeated rebases; untouched sky defaults update and conflicting field changes block publication.");
    /* Conflicts and format changes must fail without touching the source. */
    next[OBJECTS+SHIFT+64+128]=2;Save(nextpath,next,SIZE);
    OK(!ProjectRebaseCheck(&project,nextpath,&report,&why) && report.conflicts==1 && strstr(report.details,"bg/bg_test.seg"));
    Reject(&project,nextpath,argv[1],"Conflict");next[OBJECTS+SHIFT+64+128]=0;
    next[LEVELS+SHIFT+29]=13;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"MusicConflict");next[LEVELS+SHIFT+29]=5;
    next[MODEL+SHIFT+modelsize-1]^=1;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"ModelConflict");next[MODEL+SHIFT+modelsize-1]^=1;
    next[TEXTURES+SHIFT]=0x23;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"TextureConflict");next[TEXTURES+SHIFT]=0x12;
    /* Different new stock data at an imported ID remains a real conflict. */
    OK(TexRomReadBank(&rom,&bank,&why));
    memcpy(next+IMAGES+SHIFT+bank.imagebytes,next+IMAGES+SHIFT,bank.imagebytes);
    Put32(next+CONFIG+SHIFT+4,2);Put32(next+TEXTURES+SHIFT+8,0x12000000|bank.imagebytes);
    Put32(next+TEXTURES+SHIFT+16,0xffff);Put32(next+MANIFEST+SHIFT+24+8,IMAGES+SHIFT+bank.imagebytes*2);
    Save(nextpath,next,SIZE);OK(!ProjectRebaseCheck(&project,nextpath,&report,&why) && strstr(why,"Image 0001"));
    Reject(&project,nextpath,argv[1],"ImageIds");
    memset(next+IMAGES+SHIFT+bank.imagebytes,0,bank.imagebytes);Put32(next+CONFIG+SHIFT+4,1);
    Put32(next+TEXTURES+SHIFT+8,0xffff);Put32(next+TEXTURES+SHIFT+16,0);
    Put32(next+MANIFEST+SHIFT+24+8,IMAGES+SHIFT+bank.imagebytes);
    next[CMAP+SHIFT+0xc108]^=1;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"CatalogConflict");next[CMAP+SHIFT+0xc108]^=1;
    Float(next+LEVELS+SHIFT+20,2);Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"ScaleConflict");Float(next+LEVELS+SHIFT+20,1);
    Put32(next+MANIFEST+SHIFT+16,2);Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"OldFormat");Put32(next+MANIFEST+SHIFT+16,3);
    /* Corrupt file names must not match by a truncated 63-byte prefix. */
    Put32(next+TABLE+SHIFT+16,0x80000000u+SHIFT*2+0xf000);
    memset(next+CMAP+SHIFT+0xf000,'x',100);Save(nextpath,next,SIZE);
    Reject(&project,nextpath,argv[1],"LongFileName");
    { unsigned char *fresh=Fixture(SHIFT,model,modelsize);memcpy(next+TABLE+SHIFT+16,fresh+TABLE+SHIFT+16,4);free(fresh); }
    Save(nextpath,next,SIZE);
    /* The same BG change in both inputs is already resolved. */
    next[OBJECTS+SHIFT+64+128]=1;Save(nextpath,next,SIZE);OK(ProjectRebaseCheck(&project,nextpath,&report,&why));
    next[OBJECTS+SHIFT+64+128]=0;Save(nextpath,next,SIZE);
    Path(path,project.dir,"images/0001.bmp");Path(backup,project.dir,"images/0001.hold");OK(MoveFileEx(path,backup,0));
    Reject(&project,nextpath,argv[1],"MissingBmp");OK(MoveFileEx(backup,path,0));
    Path(path,project.dir,"models/native/Pjungle3_treeZ.gmodel");edited=Read(path,&size);edited[4]^=1;Save(path,edited,size);
    Reject(&project,nextpath,argv[1],"Fingerprint");edited[4]^=1;Save(path,edited,size);free(edited);
    OK(ProjectRebaseCheck(&project,nextpath,&report,&why));
    Path(path,project.dir,"base.z64");Path(backup,project.dir,"base.hold");OK(MoveFileEx(path,backup,0));
    OK(!ProjectRebaseCheck(&project,nextpath,&report,&why));OK(MoveFileEx(backup,path,0));
    puts("PASS: conflicts, changed native assets/catalogs/scales/image IDs, malformed names, obsolete ROM format, identical edits, missing base/image and corrupt model fingerprint.");
    /* Fail after staging has started; neither partial output nor temp leftovers. */
    test_fail_copy=2;Reject(&project,nextpath,argv[1],"CopyFail");
    test_fail_write=1;Reject(&project,nextpath,argv[1],"WriteFail");
    test_fail_move=1;Reject(&project,nextpath,argv[1],"PublishFail");
    OK(!ProjectRebaseDestination(&project,project.dir,"Nested",destination,&why));
    Path(path,project.dir,"notes");OK(!ProjectRebaseDestination(&project,path,"Nested",destination,&why));
    OK(!ProjectRebaseDestination(&project,argv[1],"Original",destination,&why));
    OK(!ProjectRebaseDestination(&project,argv[1],"old.z64",destination,&why));
    OK(!ProjectRebaseDestination(&project,argv[1],"../bad",destination,&why));
    OK(!ProjectRebaseDestination(&project,argv[1],"CON",destination,&why));
    test_publish_race=1;OK(!ProjectRebaseCreate(&project,nextpath,argv[1],"Race",&loaded,&report,&why));
    Path(path,argv[1],"Race");OK(GetFileAttributes(path)&FILE_ATTRIBUTE_DIRECTORY);OK(RemoveDirectory(path));NoTemps(argv[1]);
    /* POSIX symlinks stand in for Windows reparse points in this file shim. */
    { char source[MAX_PATH],alias[MAX_PATH];Path(source,argv[1],"Original");Path(alias,argv[1],"Alias");OK(!symlink(source,alias));
      OK(!ProjectRebaseDestination(&project,alias,"Nested",destination,&why));OK(!unlink(alias));
      Path(alias,source,"notes/link");OK(!symlink(oldpath,alias));Reject(&project,nextpath,argv[1],"Linked");OK(!unlink(alias)); }
    Path(path,project.dir,"base.z64");OK(Hash(path)==Hash(oldpath));OK(RomExportValidateProject(&project,&why));
    puts("PASS: copy/write/publish failures, destination race, existing/reserved/nested paths and reparse-point rejection; original project remains exportable.");
    CatalogRebase(&project,nextpath,argv[1]);
    RetiredRebases(&project,nextpath,argv[1]);
    MemoryRebases(&project,nextpath,argv[1]);
    OccluderRebases(&project,nextpath,argv[1]);
    ObjectFadeRebases(&project,nextpath,argv[1]);
    free(model);free(old);free(next);RomFree(&rom);return 0;
}
