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
static DWORD String(unsigned char *data, DWORD *cursor, const char *text)
{
    DWORD pointer=VADDR+*cursor-CMAP;
    strcpy((char *)data+*cursor,text);*cursor+=(DWORD)strlen(text)+1;return pointer;
}
static void ExpandedCatalog(const char *dir, unsigned char *data)
{
    unsigned char *solo=data+LEVELS,*mp=solo+44,*title=mp+44,*fallback=title+44,*sentinel=fallback+44;
    DWORD cursor=CMAP+0x500;
    char path[MAX_PATH],base[MAX_PATH+16];const char *why="";
    RomFile rom;GEditorProject project,loaded;
    Put32(solo,27);Put32(solo+4,String(data,&cursor,"Bunker 2"));
    Put32(solo+8,String(data,&cursor,"UsetupsevbZ"));
    memcpy(mp,solo,44);Put32(mp,427);Put32(mp+4,String(data,&cursor,"Bunker 2 (MP)"));
    Put32(mp+8,String(data,&cursor,"Ump_setupsevbZ"));
    memset(title,0,132);Put32(title,90);Put32(title+4,String(data,&cursor,"Title"));
    Float(title+24,1);Float(title+28,1);Float(title+32,1);memset(title+36,0xff,6);
    Float(fallback+24,1);Float(fallback+28,1);Float(fallback+32,1);memset(fallback+36,0xff,6);
    Put32(sentinel,57);Put32(sentinel+12,String(data,&cursor,"bg/bgx.seg"));
    Float(sentinel+24,1);Float(sentinel+28,1);Float(sentinel+32,1);memset(sentinel+36,0xff,6);
    Entry(data,3,0x53544754,LEVELS,LEVELS+220,5);
    Put32(data+ENV,27);memcpy(data+ENV+104,data+ENV,104);Put32(data+ENV+104,227);
    data[ENV+104+44]=77;
    memcpy(data+ENV+208,data+ENV,104);Put32(data+ENV+208,427);data[ENV+208+44]=88;
    Put32(data+ENV+312,0);
    snprintf(path,sizeof(path),"%s/catalog.z64",dir);Save(path,data,SIZE);
    assert(RomLoad(path,&rom,&why)&&rom.info.levelcount==3);
    assert(rom.info.levels[0].levelID==27&&rom.info.levels[1].levelID==427&&rom.info.levels[2].levelID==90);
    assert(!strcmp(rom.info.levels[1].setupname,"Ump_setupsevbZ"));
    assert(!strcmp(rom.info.levels[0].bgname,rom.info.levels[1].bgname));
    assert(rom.info.levels[0].backgroundcolor[0]==10&&rom.info.levels[1].backgroundcolor[0]==77);
    assert(!rom.info.levels[2].bgname[0]&&!rom.info.levels[2].setupname[0]&&!rom.info.levels[2].stanname[0]);
    assert(ProjectCreate("Catalog",dir,&rom.info,&project,&why));
    snprintf(base,sizeof(base),"%s\\base.z64",project.dir);Save(base,data,SIZE);
    assert(ProjectRead(project.geppath,&loaded)&&loaded.levelcount==3);
    assert(!strcmp(loaded.levels[2].name,"Title")&&!loaded.levels[2].world[0]);
    assert(RomExportRefreshProjectLevelMetadata(&loaded,&why));
    loaded.levels[1].music=51;loaded.levels[1].renderScale=0.75f;loaded.levels[1].chrLODDistance=1.5f;
    assert(TestUpdateLevelTable(&loaded,&rom,&why));
    assert(!memcmp(rom.data+LEVELS,data+LEVELS,44)); /* MP edits leave the solo row intact. */
    assert(!memcmp(rom.data+LEVELS+44,data+LEVELS+44,28)); /* Preserve resource/allocation pointers. */
    assert(Get32(rom.data+LEVELS+44+28)==0x3f400000&&rom.data[LEVELS+44+37]==51);
    assert(!memcmp(rom.data+LEVELS+88,data+LEVELS+88,132)); /* Title/default/MAX survive. */
    assert(ProjectSave(&loaded,&why)&&ProjectRead(loaded.geppath,&project));
    assert(project.levels[1].music==51&&project.levels[1].renderScale==0.75f&&project.levels[1].chrLODDistance==1.5f);
    RomFree(&rom);
    Put32(mp,27);Reject(path,data);Put32(mp,427);
    puts("PASS: named MP/Title catalog, shared BG, two-player environment, empty resource fields, independent MP settings and duplicate-ID rejection.");
}
static void RomAndProject(const char *dir, DWORD rowSize)
{
    unsigned char *data=calloc(SIZE,1),*row=data+LEVELS;DWORD i,cursor=STRINGS;
    const char *strings[]={"Bunker 1","UsetupsevbunkerZ","bg/bg_sev_all_p.seg","Tbg_sev_all_p_stanZ","bg/bgx.seg"};
    const char *optional[]={"SFXC","SFXT","INSC","INST","MUST","MUSD","MUSV","ANID",
                            "ANIF","ANIC","ANIO","CHRM","PROP","ITEM","AIGL","TXBK"};
    char path[MAX_PATH],base[MAX_PATH+16],projectfile[MAX_PATH],projectname[32];const char *why="";
    RomFile rom;GEditorProject project,loaded;RomInfo info;FILE *f;
    assert(data);snprintf(path,sizeof(path),"%s/input.z64",dir);
    Put32(data,0x80371240);memcpy(data+0x20,"GOLDENEYE",9);
    memcpy(data+MANIFEST,"GUDGEDITORMANIF",16);Put32(data+MANIFEST+16,3);Put32(data+MANIFEST+20,28);
    Entry(data,0,0x494d4753,0x102000,0x102080,0);Entry(data,1,0x4f425347,0x150000,0x160000,0);
    Entry(data,2,0x4d555346,0x160000,0x170000,0);Entry(data,3,0x53544754,LEVELS,LEVELS+2*rowSize,2);
    Entry(data,4,0x434d4150,CMAP,0x140000,VADDR);Entry(data,5,0x4654424c,CMAP+0x600,0,0);
    Entry(data,6,0x454e5654,ENV,0,104);Entry(data,7,0x54585442,CMAP+0x2000,CMAP+0xa008,4096);
    Entry(data,8,0x54584346,CMAP+0x900,CMAP+0x908,1);
    Entry(data,9,0x4d4f4e41,CMAP+0x920,CMAP+0x9f0,52);
    Entry(data,10,0x4d4f4e54,CMAP+0xa00,CMAP+0xc58,12);
    Entry(data,11,0x4d4f4e44,CMAP+0xc60,CMAP+0xcd4,116);
    /* The current reader must retain optional entries without interpreting
       their payloads. Actual native catalogs are checked by manifest_catalogs. */
    for(i=0;i<16;i++)
    {
        DWORD start=CMAP+0xb000+i*16;
        DWORD flags=(i==6 || i>=9) ? 0x80000000u : i==4 ? 8 : 0;
        Entry(data,12+i,Get32((const unsigned char *)optional[i]),start,start+16,flags);
        memcpy(data+start,optional[i],4);
    }
    Put32(row,9);Float(row+24,1.25f);Float(row+28,0.5f);Float(row+32,0.825000048f);row[37]=13;row[39]=14;row[41]=15;
    for(i=0;i<5;i++)
    {
        Put32(i<4 ? row+4+i*4 : row+rowSize+12,VADDR+cursor-CMAP);
        strcpy((char *)data+cursor,strings[i]);cursor+=strlen(strings[i])+1;
    }
    if (rowSize==44)
    {
        Put32(row+20,VADDR+cursor-CMAP);
        strcpy((char *)data+cursor,"-ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma150");
    }
    Put32(row+rowSize,0xffffffffu); /* Sentinel has a NULL levelName. */
    Put32(data+ENV,9);Put32(data+ENV+4,1);Float(data+ENV+8,10);Float(data+ENV+12,1000);
    Put32(data+ENV+36,995);Put32(data+ENV+40,1000);data[ENV+44]=10;data[ENV+45]=20;data[ENV+46]=30;
    data[ENV+47]=1;Float(data+ENV+48,5000);Float(data+ENV+56,230);
    Float(data+ENV+60,220);Float(data+ENV+64,210);Float(data+ENV+92,7);
    Save(path,data,SIZE);assert(RomLoad(path,&rom,&why));
    assert(rom.info.entrycount==28);
    for(i=0;i<16;i++)
    {
        const RomManifestEntry *entry=&rom.info.entries[12+i];
        assert(entry->kind==Get32((const unsigned char *)optional[i]));
        assert(entry->romstart==CMAP+0xb000+i*16 && entry->romend==entry->romstart+16);
    }
    assert(rom.info.levelcount==1&&!strcmp(rom.info.levels[0].name,"Bunker 1"));
    assert(rom.info.levels[0].levelscale==1.25f&&rom.info.levels[0].renderScale==0.5f&&rom.info.levels[0].chrLODDistance==0.825000048f);
    assert(rom.info.levels[0].music==13&&rom.info.levels[0].bgsound==14&&rom.info.levels[0].xtrack==15);
    assert(rom.info.levels[0].hasbackgroundcolor&&rom.info.levels[0].backgroundcolor[1]==20);
    assert(rom.info.levels[0].fog.enabled&&rom.info.levels[0].fog.start==995);
    assert(rom.info.levels[0].clouds.enabled&&rom.info.levels[0].clouds.textureid==0x08b4);
    assert(rom.info.levels[0].clouds.height==5000&&rom.info.levels[0].clouds.color[1]==220);
    info=rom.info;RomFree(&rom);
    Put32(data+MANIFEST+24+27*16+8,SIZE+1);Reject(path,data);
    Put32(data+MANIFEST+24+27*16+8,CMAP+0xb100);
    Put32(data+MANIFEST+20,12);Save(path,data,SIZE);assert(RomLoad(path,&rom,&why));RomFree(&rom);
    Put32(data+MANIFEST+20,28); /* Discovery entries are optional, not new required features. */
    Put32(data+MANIFEST+16,2);Reject(path,data);Put32(data+MANIFEST+16,3);
    Put32(data+MANIFEST+24+3*16+8,LEVELS+64);Reject(path,data);Put32(data+MANIFEST+24+3*16+8,LEVELS+2*rowSize);
    Put32(data+MANIFEST+24+7*16,0x42414421);Reject(path,data);Put32(data+MANIFEST+24+7*16,0x54585442);
    Put32(data+MANIFEST+24+7*16,0x54584346);Reject(path,data);Put32(data+MANIFEST+24+7*16,0x54585442);
    Put32(row+4,0);Reject(path,data);Put32(row+4,VADDR+STRINGS-CMAP);Save(path,data,SIZE);
    for(DWORD oldsize=36;oldsize<=40;oldsize+=4) {
        Put32(data+MANIFEST+24+3*16+8,LEVELS+2*oldsize);Reject(path,data);
    }
    Put32(data+MANIFEST+24+3*16+8,LEVELS+2*rowSize);
    const float invalid[]={0,-1,NAN,INFINITY};
    for(unsigned j=0;j<sizeof(invalid)/sizeof(*invalid);j++) {Float(row+32,invalid[j]);Reject(path,data);}
    Float(row+32,.825000048f);Save(path,data,SIZE);
    snprintf(projectname,sizeof(projectname),"Current%u",(unsigned)rowSize);
    assert(ProjectCreate(projectname,dir,&info,&project,&why));strcpy(projectfile,project.geppath);
    snprintf(base,sizeof(base),"%s\\base.z64",project.dir);Save(base,data,SIZE);
    assert(ProjectRead(projectfile,&loaded));assert(loaded.levelcount==1&&!strcmp(loaded.levels[0].world,"sev"));
    strcpy(loaded.levels[0].name,"Stale label");assert(RomExportRefreshProjectLevelMetadata(&loaded,&why));
    assert(!strcmp(loaded.levels[0].name,"Bunker 1")&&loaded.levels[0].hasbackgroundcolor);
    assert(loaded.levels[0].backgroundcolor[2]==30&&loaded.levels[0].fog.enabled);
    assert(loaded.levels[0].clouds.enabled&&loaded.levels[0].clouds.height==5000);
    assert(loaded.levels[0].clouds.color[2]==210&&loaded.levels[0].clouds.horizonoffset==7);
    assert(RomLoad(base,&rom,&why));loaded.levels[0].levelscale=2;loaded.levels[0].renderScale=0.25f;loaded.levels[0].chrLODDistance=1.375f;
    loaded.levels[0].music=21;loaded.levels[0].bgsound=22;loaded.levels[0].xtrack=23;
    assert(TestUpdateLevelTable(&loaded,&rom,&why));
    assert(Get32(rom.data+LEVELS+24)==0x40000000&&Get32(rom.data+LEVELS+28)==0x3e800000);
    assert(Get32(rom.data+LEVELS+32)==0x3fb00000);
    assert(rom.data[LEVELS+37]==21&&rom.data[LEVELS+39]==22&&rom.data[LEVELS+41]==23);
    assert(!memcmp(rom.data+LEVELS,data+LEVELS,24));
    assert(!memcmp(rom.data+MANIFEST,data+MANIFEST,24+28*16));
    assert(!memcmp(rom.data+CMAP+0xb000,data+CMAP+0xb000,16*16));RomFree(&rom);
    loaded.levels[0].chrLODDistance=1.375f;
    assert(ProjectSave(&loaded,&why)&&ProjectRead(projectfile,&project)&&project.levels[0].chrLODDistance==1.375f);
    for(unsigned j=0;j<sizeof(invalid)/sizeof(*invalid);j++) {
        loaded.levels[0].chrLODDistance=invalid[j];assert(!ProjectSave(&loaded,&why));
    }
    loaded.levels[0].chrLODDistance=1.375f;
    loaded.levels[0].levelID=999;assert(!RomExportRefreshProjectLevelMetadata(&loaded,&why));
    assert(DeleteFile(base));assert(!RomExportRefreshProjectLevelMetadata(&project,&why));
    f=fopen(projectfile,"wb");assert(f);fputs("GEditor Project 1\nname = Old\n",f);fclose(f);assert(!ProjectRead(projectfile,&loaded));
    f=fopen(projectfile,"wb");assert(f);fputs("GEditor Project 2\nname = Incomplete\n",f);fclose(f);assert(!ProjectRead(projectfile,&loaded));
    for(int version=2;version<=4;version++) {
        f=fopen(projectfile,"wb");assert(f);fprintf(f,"GEditor Project %d\nname = Old\n",version);fclose(f);
        assert(!ProjectRead(projectfile,&loaded));
    }
    assert(ProjectSave(&project,&why)&&ProjectRead(projectfile,&loaded));
    if (rowSize==44) { ExpandedCatalog(dir,data); }
    free(data);
    printf("PASS: %u-byte level table load/export preserves pointers, names, sentinel and settings; malformed metadata rejected.\n",(unsigned)rowSize);
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
int main(int argc,char **argv) { assert(argc==2);RomAndProject(argv[1],44);VertexBatches();return 0; }
