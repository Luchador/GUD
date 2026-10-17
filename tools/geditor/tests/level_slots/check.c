#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"
static const char *why;
#define OK(x) do { if (!(x)) { fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why); abort(); } } while (0)
static void Put32(unsigned char *p,unsigned v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Fixture(RomFile *rom,unsigned char *data)
{
    static const int defaults[32]=LEVEL_MENU_DEFAULT_STAGES;
    memset(rom,0,sizeof(*rom));memset(data,0xa5,2048);rom->data=data;rom->size=2048;
    rom->info.entrycount=2;
    rom->info.entries[0]=(RomManifestEntry){0x434d4150,64,2048,0x80000000};
    rom->info.entries[1]=(RomManifestEntry){LEVEL_MENU_KIND,128,260,1};
    Put32(data+128,1);
    for(unsigned i=0;i<32;i++)
    {
        Put32(data+132+4*i,defaults[i]);
        if(defaults[i]<0)continue;
        RomLevel *level=&rom->info.levels[rom->info.levelcount++];
        level->levelID=defaults[i];strcpy(level->name,"Level");strcpy(level->setupname,"UsetupZ");
        strcpy(level->bgname,"bg/bg.seg");strcpy(level->stanname,"TstanZ");
        level->levelscale=level->renderScale=level->chrLODDistance=1;
    }
}
int main(int argc,char **argv)
{
    assert(argc==2);unsigned char data[2048],before[2048];RomFile rom;
    LevelSlotTable base,read;LevelSlotOverrides changes={0},snapshot;
    Fixture(&rom,data);OK(LevelSlotsReadRom(&rom,&base,&why)&&base.available);
    OK(LevelSlotsGet(&base,&changes,27)==LEVELID_FACILITY_MP);
    OK(LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,0,LEVELID_FRIGATE,&why));
    OK(LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,31,LEVELID_FACILITY,&why));
    OK(LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,21,LEVELID_NONE,&why));
    snapshot=changes;
    OK(!LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,0,LEVELID_NONE,&why));
    OK(!LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,0,LEVELID_TITLE,&why));
    OK(!LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,32,LEVELID_DAM,&why));
    OK(!LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,0,999,&why));
    OK(!memcmp(&changes,&snapshot,sizeof(changes)));
    GEditorProject project={0},loaded;
    strcpy(project.name,"Slots");strcpy(project.dir,argv[1]);snprintf(project.geppath,sizeof(project.geppath),"%s/slots.gep",argv[1]);
    project.levelcount=rom.info.levelcount;memcpy(project.levels,rom.info.levels,sizeof(project.levels));
    project.levelSlots=base;project.levelSlotOverrides=changes;
    OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded));
    OK(!memcmp(&changes,&loaded.levelSlotOverrides,sizeof(changes)));
    memcpy(before,data,sizeof(data));OK(LevelSlotsApplyRom(&rom,&changes,&why));
    Put32(before+132,LEVELID_FRIGATE);Put32(before+132+31*4,LEVELID_FACILITY);Put32(before+132+21*4,-1);
    OK(!memcmp(data,before,sizeof(data)));
    OK(LevelSlotsReadRom(&rom,&read,&why)&&read.value.stages[31]==LEVELID_FACILITY);
    OK(LevelSlotsApplyRom(&rom,&changes,&why)&&!memcmp(data,before,sizeof(data)));
    OK(LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,0,LEVELID_DAM,&why)&&!(changes.mask&1));
    for(unsigned i=20;i<32;i++)
        if(i!=30)OK(LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,i,-1,&why));
    OK(!LevelSlotsSet(&base,&changes,rom.info.levels,rom.info.levelcount,30,-1,&why));
    const char *bad[]={"","32|21","-1|21","0|-1","0|90x","0|2147483648","0|999999999999999999999","0|21|22","0"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) { LevelSlotOverrides invalid={0};OK(!LevelSlotsReadOverride(&invalid,bad[i])); }
    LevelSlotOverrides parsed={0};OK(LevelSlotsReadOverride(&parsed,"31|-1")&&!LevelSlotsReadOverride(&parsed,"31|21"));
    for(int fault=0;fault<7;fault++)
    {
        Fixture(&rom,data);
        switch(fault)
        {
        case 0:rom.info.entries[1].romend=2049;break;
        case 1:rom.info.entries[1].romstart=60;break;
        case 2:rom.info.entries[1].flags=2;break;
        case 3:Put32(data+128,2);break;
        case 4:rom.info.entries[2]=rom.info.entries[1];rom.info.entrycount=3;break;
        case 5:Put32(data+132,999);break;
        case 6:rom.info.entries[2]=(RomManifestEntry){0x53544754,252,300,1};rom.info.entrycount=3;break;
        }
        memcpy(before,data,sizeof(data));OK(!LevelSlotsApplyRom(&rom,&snapshot,&why)&&!memcmp(data,before,sizeof(data)));
    }
    Fixture(&rom,data);rom.info.entrycount=1;LevelSlotOverrides empty={0};
    OK(LevelSlotsReadRom(&rom,&read,&why)&&!read.available);
    OK(LevelSlotsApplyRom(&rom,&empty,&why)&&!LevelSlotsApplyRom(&rom,&snapshot,&why));
    puts("PASS: slot validation, duplicates, exact solo/MP IDs, highest mask bit, save/reopen, bounded ROM writes, repeat export, malformed tables and legacy ROMs.");
}
