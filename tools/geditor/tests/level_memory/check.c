#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"
static const char *why = "";
#define OK(x) do { if (!(x)) { fprintf(stderr, "%d: %s: %s\n", __LINE__, #x, why); abort(); } } while (0)
static void Put32(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Fixture(RomFile *rom, unsigned char data[2048])
{
    memset(rom,0,sizeof(*rom));memset(data,0xa5,2048);rom->data=data;rom->size=2048;
    rom->info.entrycount=3;rom->info.levelcount=3;
    rom->info.entries[0]=(RomManifestEntry){0x434d4150,64,2048,0x80000000};
    rom->info.entries[1]=(RomManifestEntry){0x53544754,128,288,4};
    rom->info.entries[2]=(RomManifestEntry){0x4c4d454d,512,768,64};
    const DWORD ids[]={27,427,90,0};
    const char *tokens[]={"-ml0 -me0 -mgfx100 -mvtx50 -mt725 -ma150", "-ml0 -me0 -mgfx130 -mvtx100 -mt550 -ma170",
        "-ml0 -me0 -mgfx80 -mvtx20 -mt646 -ma001", "-ml0 -me0 -mgfx100 -mvtx50 -mt700 -ma400"};
    for(int i=0;i<4;i++)
    {
        Put32(data+128+40*i,ids[i]);Put32(data+148+40*i,0x80000000+512+64*i-64);
        strcpy((char *)data+512+64*i,tokens[i]);
        if(i<3) { rom->info.levels[i].levelID=ids[i]; }
    }
}
static void Data(const char *dir)
{
    unsigned char data[2048],before[2048];RomFile rom;
    LevelMemoryTable base,saved;LevelMemoryOverrides changes={0};LevelMemory value,actual;
    Fixture(&rom,data);OK(LevelMemoryReadRom(&rom,&base,&why));
    OK(base.count==3&&base.rows[0].kib[2]==725&&base.rows[1].id==427&&base.rows[2].kib[3]==1);
    value=base.rows[0];value.kib[0]=200;value.kib[1]=75;value.kib[2]=1000;value.kib[3]=400;
    OK(LevelMemorySet(&base,&changes,&value,&why));OK(changes.count==1&&changes.rows[0].fields==15);
    OK(LevelMemoryGet(&base,&changes,27,&actual)&&!memcmp(&value,&actual,sizeof(value)));
    GEditorProject project={0},loaded={0};strcpy(project.name,"Memory");strcpy(project.dir,dir);
    snprintf(project.geppath,sizeof(project.geppath),"%s/memory.gep",dir);
    project.levelcount=1;project.levels[0]=rom.info.levels[0];strcpy(project.levels[0].name,"Bunker 2");
    project.levels[0].levelscale=project.levels[0].renderScale=1;project.memory=base;project.memoryOverrides=changes;
    OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded));
    OK(!memcmp(&changes,&loaded.memoryOverrides,sizeof(changes)));
    char line[128];FILE *f=fopen(project.geppath,"r");OK(f&&fgets(line,sizeof(line),f));fclose(f);OK(!strcmp(line,"GEditor Project 4\n"));
    memcpy(before,data,sizeof(data));OK(LevelMemoryApplyRom(&rom,&changes,&why));
    memset(before+512,0,64);strcpy((char *)before+512,"-ml0 -me0 -mgfx200 -mvtx75 -mt1000 -ma400");
    OK(!memcmp(data,before,sizeof(data))); /* Only one independently allocated slot changes. */
    OK(LevelMemoryReadRom(&rom,&saved,&why)&&!memcmp(&saved.rows[0],&value,sizeof(value)));
    OK(LevelMemoryApplyRom(&rom,&changes,&why)&&!memcmp(data,before,sizeof(data)));
    OK(LevelMemorySet(&base,&changes,&base.rows[0],&why)&&!changes.count);
    project.memoryOverrides=changes;OK(ProjectSave(&project,&why));
    f=fopen(project.geppath,"r");OK(f&&fgets(line,sizeof(line),f));fclose(f);OK(!strcmp(line,"GEditor Project 2\n"));
    const char *bad[]={"", "0", "-1", "8193", "1.5", "64KiB", "0x100", "99999999999999999999999999"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) { OK(!LevelMemoryParseField(&value,0,bad[i],&why)); }
    OK(LevelMemoryParseField(&value,0," 8192 ",&why)&&value.kib[0]==8192);
    OK(!LevelMemoryReadOverride(&changes,"27|unknown|10"));
    OK(LevelMemoryReadOverride(&changes,"27|mgfx|200")&&!LevelMemoryReadOverride(&changes,"27|mgfx|300"));
    OK(!LevelMemoryReadOverride(&changes,"27|mt|1garbage")&&!LevelMemoryReadOverride(&changes,"-1|mt|10"));
    f=fopen(project.geppath,"w");OK(f);fputs("GEditor Project 3\nname = Old\nmemory = 27|mgfx|200\n",f);fclose(f);
    OK(!ProjectRead(project.geppath,&loaded));
    /* Read malformed tables without following arbitrary pointers or accepting
     * shared allocation storage. A failed export never changes ROM bytes. */
    for(int fault=0;fault<7;fault++)
    {
        Fixture(&rom,data);
        switch(fault)
        {
        case 0: rom.info.entrycount=2;break;
        case 1: rom.info.entries[2].romend=2049;break;
        case 2: Put32(data+148,0xffffffff);break;
        case 3: Put32(data+188,0x800001c0);break; /* alias first slot */
        case 4: memset(data+512,'a',64);break;
        case 5: strcpy((char *)data+512,"-ml0 -me0 -mgfx100 -mvtx50 -mt725 -ma150 -unknown1");break;
        case 6: rom.info.entries[2].flags=32;break;
        }
        memcpy(before,data,sizeof(data));OK(!LevelMemoryApplyRom(&rom,&changes,&why)&&!memcmp(data,before,sizeof(data)));
    }
    LevelMemoryTable next=base;next.rows[0].kib[2]=800;
    OK(LevelMemoryRebase(&base,&next,&changes,&why));
    OK(LevelMemoryGet(&next,&changes,27,&value)&&value.kib[0]==200&&value.kib[2]==800);
    LevelMemoryOverrides snapshot=changes;next.rows[0].kib[0]=150;
    OK(!LevelMemoryRebase(&base,&next,&changes,&why)&&strstr(why,"Display lists")&&!memcmp(&snapshot,&changes,sizeof(changes)));
    next.rows[0].kib[0]=200;OK(LevelMemoryRebase(&base,&next,&changes,&why)&&!changes.count);
    puts("PASS: independent solo/MP/Title budgets, all fields, project persistence/versions, exact ROM writes, repeat export/reset, validation and three-way merges.");
}

#include "ui.c"
int main(int argc,char **argv) { assert(argc==2);Data(argv[1]);UI();return 0; }
