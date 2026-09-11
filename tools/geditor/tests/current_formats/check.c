#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"
#include "romexport.h"
#include "bgdocument.h"

#define SIZE (2u*1024u*1024u)
#define MANIFEST 0x101000u
#define CMAP 0x110000u
#define LEVELS (CMAP+0x300u)
#define STRINGS (CMAP+0x400u)
#define ENV (CMAP+0x700u)
#define VADDR 0x80000000u
BOOL TestUpdateLevelTable(const GEditorProject *,RomFile *,const char **);
static void Put32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Get32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Save(const char *path,const void *data,size_t size)
{ FILE *f=fopen(path,"wb");assert(f&&fwrite(data,1,size,f)==size&&!fclose(f)); }
static void Float(unsigned char *p,float value) { DWORD bits;memcpy(&bits,&value,4);Put32(p,bits); }
static void Entry(unsigned char *data,int index,DWORD kind,DWORD start,DWORD end,DWORD flags)
{ unsigned char *p=data+MANIFEST+24+index*16;Put32(p,kind);Put32(p+4,start);Put32(p+8,end);Put32(p+12,flags); }
static void Reject(const char *path,unsigned char *data)
{ const char *why="";RomFile rom;Save(path,data,SIZE);assert(!RomLoad(path,&rom,&why)&&!rom.data&&why[0]); }
static void RomAndProject(const char *dir)
{
    unsigned char *data=calloc(SIZE,1),*row=data+LEVELS;DWORD i,cursor=STRINGS;
    const char *strings[]={"Bunker 1","UsetupsevbunkerZ","bg/bg_sev_all_p.seg","Tbg_sev_all_p_stanZ","bg/bgx.seg"};
    char path[MAX_PATH],base[MAX_PATH+16],projectfile[MAX_PATH];const char *why="";
    RomFile rom;GEditorProject project,loaded;RomInfo info;FILE *f;
    assert(data);snprintf(path,sizeof(path),"%s/input.z64",dir);
    Put32(data,0x80371240);memcpy(data+0x20,"GOLDENEYE",9);
    memcpy(data+MANIFEST,"GUDGEDITORMANIF",16);Put32(data+MANIFEST+16,2);Put32(data+MANIFEST+20,12);
    Entry(data,0,0x494d4753,0x102000,0x102080,0);Entry(data,1,0x4f425347,0x150000,0x160000,0);
    Entry(data,2,0x4d555346,0x160000,0x170000,1);Entry(data,3,0x53544754,LEVELS,LEVELS+72,2);
    Entry(data,4,0x434d4150,CMAP,0x140000,VADDR);Entry(data,5,0x4654424c,CMAP+0x600,0,0);
    Entry(data,6,0x454e5654,ENV,0,104);Entry(data,7,0x54585442,CMAP+0x2000,CMAP+0xa008,4096);
    Entry(data,8,0x54584346,CMAP+0x900,CMAP+0x908,1);
    Entry(data,9,0x4d4f4e41,CMAP+0x920,CMAP+0x9f0,52);
    Entry(data,10,0x4d4f4e54,CMAP+0xa00,CMAP+0xc58,12);
    Entry(data,11,0x4d4f4e44,CMAP+0xc60,CMAP+0xcd4,116);
    Put32(row,9);Float(row+20,1.25f);Float(row+24,0.5f);row[29]=13;row[31]=14;row[33]=15;
    for(i=0;i<5;i++)
    {
        Put32(i<4 ? row+4+i*4 : row+36+12,VADDR+cursor-CMAP);
        strcpy((char *)data+cursor,strings[i]);cursor+=strlen(strings[i])+1;
    }
    Put32(row+36,0xffffffffu); /* Sentinel has a NULL levelName. */
    Put32(data+ENV,9);Put32(data+ENV+4,1);Float(data+ENV+8,10);Float(data+ENV+12,1000);
    Put32(data+ENV+36,995);Put32(data+ENV+40,1000);data[ENV+44]=10;data[ENV+45]=20;data[ENV+46]=30;
    Save(path,data,SIZE);assert(RomLoad(path,&rom,&why));
    assert(rom.info.levelcount==1&&!strcmp(rom.info.levels[0].name,"Bunker 1"));
    assert(rom.info.levels[0].levelscale==1.25f&&rom.info.levels[0].renderScale==0.5f);
    assert(rom.info.levels[0].music==13&&rom.info.levels[0].bgsound==14&&rom.info.levels[0].xtrack==15);
    assert(rom.info.levels[0].hasbackgroundcolor&&rom.info.levels[0].backgroundcolor[1]==20);
    assert(rom.info.levels[0].fog.enabled&&rom.info.levels[0].fog.start==995);info=rom.info;RomFree(&rom);
    Put32(data+MANIFEST+16,1);Reject(path,data);Put32(data+MANIFEST+16,2);
    Put32(data+MANIFEST+24+3*16+8,LEVELS+64);Reject(path,data);Put32(data+MANIFEST+24+3*16+8,LEVELS+72);
    Put32(data+MANIFEST+24+7*16,0x42414421);Reject(path,data);Put32(data+MANIFEST+24+7*16,0x54585442);
    Put32(data+MANIFEST+24+7*16,0x54584346);Reject(path,data);Put32(data+MANIFEST+24+7*16,0x54585442);
    Put32(row+4,0);Reject(path,data);Put32(row+4,VADDR+STRINGS-CMAP);Save(path,data,SIZE);
    assert(ProjectCreate("Current",dir,&info,&project,&why));strcpy(projectfile,project.geppath);
    snprintf(base,sizeof(base),"%s\\base.z64",project.dir);Save(base,data,SIZE);
    assert(ProjectRead(projectfile,&loaded));assert(loaded.levelcount==1&&!strcmp(loaded.levels[0].world,"sev"));
    strcpy(loaded.levels[0].name,"Stale label");assert(RomExportRefreshProjectLevelMetadata(&loaded,&why));
    assert(!strcmp(loaded.levels[0].name,"Bunker 1")&&loaded.levels[0].hasbackgroundcolor);
    assert(loaded.levels[0].backgroundcolor[2]==30&&loaded.levels[0].fog.enabled);
    assert(RomLoad(base,&rom,&why));loaded.levels[0].levelscale=2;loaded.levels[0].renderScale=0.25f;
    loaded.levels[0].music=21;loaded.levels[0].bgsound=22;loaded.levels[0].xtrack=23;
    assert(TestUpdateLevelTable(&loaded,&rom,&why));
    assert(Get32(rom.data+LEVELS+20)==0x40000000&&Get32(rom.data+LEVELS+24)==0x3e800000);
    assert(rom.data[LEVELS+29]==21&&rom.data[LEVELS+31]==22&&rom.data[LEVELS+33]==23);
    assert(!memcmp(rom.data+LEVELS,data+LEVELS,20));RomFree(&rom);
    loaded.levels[0].levelID=999;assert(!RomExportRefreshProjectLevelMetadata(&loaded,&why));
    assert(DeleteFile(base));assert(!RomExportRefreshProjectLevelMetadata(&project,&why));
    f=fopen(projectfile,"wb");assert(f);fputs("GEditor Project 1\nname = Old\n",f);fclose(f);assert(!ProjectRead(projectfile,&loaded));
    f=fopen(projectfile,"wb");assert(f);fputs("GEditor Project 2\nname = Incomplete\n",f);fclose(f);assert(!ProjectRead(projectfile,&loaded));
    assert(ProjectSave(&project,&why)&&ProjectRead(projectfile,&loaded));free(data);
    puts("PASS: current GEP/manifest/36-byte table, authored names, NULL sentinel, fog, level export, and rejected legacy/missing/mismatched metadata.");
}
static void VertexBatches(void)
{
    unsigned char data[512]={0},copy[512];BgFile bg={0};const char *why="";
    bg.data=data;bg.size=sizeof(data);Put32(data+4,32);Put32(data+56,0x0e000080);Put32(data+60,0x0e000100);
    Put32(data+124,64);Put32(data+252,24);Put32(data+256,0x04200030);Put32(data+260,0);
    Put32(data+264,0xbf000000);Put32(data+268,0x00000a14);Put32(data+272,0xb8000000);
    memcpy(copy,data,sizeof(data));assert(BgFileValidateVertexBatches(&bg,&why)&&!memcmp(data,copy,sizeof(data)));
    /* All vertices are resident, but some lie outside the last VTX load's
     * CPU bullet bounds. Reject this obsolete save pattern without repairing. */
    Put32(data+252,32);Put32(data+264,0x04030010);Put32(data+268,48);
    Put32(data+272,0xbf000000);Put32(data+276,0x00000a14);Put32(data+280,0xb8000000);
    memcpy(copy,data,sizeof(data));assert(!BgFileValidateVertexBatches(&bg,&why)&&!memcmp(data,copy,sizeof(data)));
    Put32(data+56,0);Put32(data+252,8);Put32(data+256,0xb8000000);Put32(data+260,0);
    assert(BgFileValidateVertexBatches(&bg,&why)); /* Empty Streets-style room. */
    Put32(data+256,0x04200030);assert(!BgFileValidateVertexBatches(&bg,&why));
    puts("PASS: valid and empty BG rooms preserved; unsafe vertex batches rejected without automatic repair.");
}
int main(int argc,char **argv) { assert(argc==2);RomAndProject(argv[1]);VertexBatches();return 0; }
