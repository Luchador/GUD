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
    Put32(data+MANIFEST+shift+16,3);Put32(data+MANIFEST+shift+20,15);
    for(i=0;i<64;i++) { pixels[i]=(TexPixel){64,128,(i%4)*64,(i%2)*255}; }
    OK(TexEncodeRecord(pixels,8,8,&options,&record,&size,&why));memcpy(data+IMAGES+shift,record,size);free(record);
    Entry(data,shift,0,"IMGS",IMAGES,IMAGES+size,0);
    Entry(data,shift,1,"OBSG",OBJECTS,MODEL+modelsize,0);
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
    data[CMAP+shift+0x82f]=1;Float(data+CMAP+shift+0x830,shift ? 7500 : 5000);
    Float(data+CMAP+shift+0x838,70);Float(data+CMAP+shift+0x83c,199);Float(data+CMAP+shift+0x840,186);
    Put32(data+CONFIG+shift,IMAGES+shift);Put32(data+CONFIG+shift+4,1);
    Put32(data+TEXTURES+shift,0x12000000|size);Put32(data+TEXTURES+shift+8,0xffff);
    /* First file is the named empty placeholder, then setup/BG/stan/model. */
    Put32(data+TABLE+shift+4,names[5]);
    for(i=1;i<=4;i++)
    {
        DWORD at=TABLE+shift+i*12,offset=i==1 ? OBJECTS : i==2 ? OBJECTS+64 : i==3 ? OBJECTS+576 : MODEL;
        Put32(data+at,i);Put32(data+at+4,names[i]);Put32(data+at+8,offset+shift);
    }
    Put32(data+OBJECTS+shift+12,48); /* Native setup with a two-word objective command. */
    Put32(data+OBJECTS+shift+48,25);Put32(data+OBJECTS+shift+56,48);
    row=data+OBJECTS+shift+64; /* Valid native BG with a three-vertex batch. */
    Put32(row+4,32);Put32(row+56,0x0e000080);Put32(row+60,0x0e000100);
    Put32(row+124,64);Put32(row+252,24);Put32(row+256,0x04200030);
    Put32(row+264,0xbf000000);Put32(row+268,0x00000a14);Put32(row+272,0xb8000000);
    memcpy(data+MODEL+shift,model,modelsize);return data;
}
static void NoTemps(const char *dir)
{
    DIR *d=opendir(dir);struct dirent *e;OK(d);
    while((e=readdir(d))) { OK(strncmp(e->d_name,".geditor-rebase-",16)); }closedir(d);
}
static void Reject(const GEditorProject *project,const char *rom,const char *parent,const char *name)
{
    GEditorProject output;ProjectRebaseReport report;char dest[MAX_PATH],base[MAX_PATH];DWORD hash;
    Path(base,project->dir,"base.z64");hash=Hash(base);
    OK(!ProjectRebaseCreate(project,rom,parent,name,&output,&report,&why));OK(why[0] && !output.dir[0]);
    Path(dest,parent,name);OK(GetFileAttributes(dest)==INVALID_FILE_ATTRIBUTES);OK(Hash(base)==hash);NoTemps(parent);
}
int main(int argc,char **argv)
{
    unsigned char *model,*old,*next,*edited;DWORD modelsize,i,offset,span,size,id;
    char oldpath[MAX_PATH],nextpath[MAX_PATH],path[MAX_PATH],backup[MAX_PATH],exported[MAX_PATH],destination[MAX_PATH];
    GEditorProject project,rebased,loaded,again;ProjectRebaseReport report;RomFile rom,output;ModelSource native;
    TexPixel pixels[64];TexImportOptions options={1,1,3,4};TexRomBank bank;
    assert(argc==3);model=Read(argv[2],&modelsize);old=Fixture(0,model,modelsize);next=Fixture(SHIFT,model,modelsize);
    Path(oldpath,argv[1],"old.z64");Path(nextpath,argv[1],"new.z64");Save(oldpath,old,SIZE);
    /* Incoming setup and sound changes; our BG/music changes must survive. */
    next[OBJECTS+SHIFT+52]=0x56;next[LEVELS+SHIFT+31]=8;Save(nextpath,next,SIZE);
    OK(RomLoad(oldpath,&rom,&why));OK(ProjectCreate("Original",argv[1],&rom.info,&project,&why));
    OK(RomExportStoreProjectBase(&project,&rom,&why));
    Folder(project.dir,"bg");Folder(project.dir,"setup");Folder(project.dir,"stan");Folder(project.dir,"images");
    Folder(project.dir,"models");Folder(project.dir,"models/native");Folder(project.dir,"models/objects");Folder(project.dir,"notes");
    for(i=1;i<=3;i++)
    {
        char name[64];OK(RomGetFileByIndex(&rom,i,name,sizeof(name),&offset,&span));
        OK(RomExportProjectResourcePath(&project,name,path,sizeof(path))==1);Save(path,rom.data+offset,span);
    }
    OK(RomExportProjectResourcePath(&project,"bg/bg_test.seg",path,sizeof(path))==1);
    edited=Read(path,&size);edited[128]=1;Save(path,edited,size);free(edited);
    project.levels[0].music=12;OK(ProjectSave(&project,&why));
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
    OK(ProjectRebaseCheck(&project,nextpath,&report,&why));OK(report.kept==1 && report.updated==1 && !report.conflicts);
    OK(ProjectRebaseCreate(&project,nextpath,argv[1],"Updated",&rebased,&report,&why));NoTemps(argv[1]);
    OK(project.levels[0].clouds.enabled && project.levels[0].clouds.height==5000);
    OK(rebased.levels[0].clouds.enabled && rebased.levels[0].clouds.height==7500);
    OK(ProjectRead(rebased.geppath,&loaded));OK(!strcmp(loaded.name,"Updated"));
    OK(loaded.levels[0].music==12 && loaded.levels[0].bgsound==8);
    Same(project.dir,rebased.dir,"bg/bg_test.seg");Same(project.dir,rebased.dir,"stan/Tbg_test_stanZ.stan");
    Same(project.dir,rebased.dir,"models/native/Pjungle3_treeZ.gmodel");Same(project.dir,rebased.dir,"models/objects/Pjungle3_treeZ.gltf");
    Same(project.dir,rebased.dir,"images/native/0001.gtex");Same(project.dir,rebased.dir,"images/native/0002.gtex");
    Same(project.dir,rebased.dir,"images/0001.bmp");Same(project.dir,rebased.dir,"notes/.artist-note");
    Path(path,rebased.dir,"Original.gep");OK(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    Path(path,rebased.dir,"base.z64");OK(Hash(path)==Hash(nextpath));Path(path,project.dir,"base.z64");OK(Hash(path)==Hash(oldpath));
    OK(RomExportCreate(&rebased,"Playable",argv[1],exported,sizeof(exported),&why));OK(RomLoad(exported,&output,&why));
    OK(output.data[0x2000]==0x22 && output.info.levels[0].music==12 && output.info.levels[0].bgsound==8);
    OK(RomGetFileByIndex(&output,1,path,sizeof(path),&offset,&span) && output.data[offset+52]==0x56);
    OK(RomGetFileByIndex(&output,2,path,sizeof(path),&offset,&span) && output.data[offset+128]==1);
    OK(RomGetFileByIndex(&output,4,path,sizeof(path),&offset,&span) && span==modelsize && memcmp(output.data+offset,model,modelsize));
    OK(TexRomReadBank(&output,&bank,&why) && bank.count==3);RomFree(&output);
    OK(ProjectRebaseCreate(&rebased,nextpath,argv[1],"Again",&again,&report,&why));
    puts("PASS: relocated ROM tables/code, three-way asset/settings merge, native model edits, imported/deleted images, source settings, sidecars, reopen, ROM export and repeat rebase.");
    /* Conflicts and format changes must fail without touching the source. */
    next[OBJECTS+SHIFT+64+128]=2;Save(nextpath,next,SIZE);
    OK(!ProjectRebaseCheck(&project,nextpath,&report,&why) && report.conflicts==1 && strstr(report.details,"bg/bg_test.seg"));
    Reject(&project,nextpath,argv[1],"Conflict");next[OBJECTS+SHIFT+64+128]=0;
    next[LEVELS+SHIFT+29]=13;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"MusicConflict");next[LEVELS+SHIFT+29]=5;
    next[MODEL+SHIFT+modelsize-1]^=1;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"ModelConflict");next[MODEL+SHIFT+modelsize-1]^=1;
    next[TEXTURES+SHIFT]=0x23;Save(nextpath,next,SIZE);Reject(&project,nextpath,argv[1],"TextureConflict");next[TEXTURES+SHIFT]=0x12;
    /* A new stock image would occupy the first imported ID: reject it. */
    OK(TexRomReadBank(&rom,&bank,&why));
    memcpy(next+IMAGES+SHIFT+bank.imagebytes,next+IMAGES+SHIFT,bank.imagebytes);
    Put32(next+CONFIG+SHIFT+4,2);Put32(next+TEXTURES+SHIFT+8,0x12000000|bank.imagebytes);
    Put32(next+TEXTURES+SHIFT+16,0xffff);Put32(next+MANIFEST+SHIFT+24+8,IMAGES+SHIFT+bank.imagebytes*2);
    Save(nextpath,next,SIZE);OK(!ProjectRebaseCheck(&project,nextpath,&report,&why) && strstr(why,"image count"));
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
    free(model);free(old);free(next);RomFree(&rom);return 0;
}
