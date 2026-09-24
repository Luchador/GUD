#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"

static const char *why = "";
#define OK(x) do { if (!(x)) { fprintf(stderr, "%d: %s: %s\n", __LINE__, #x, why); abort(); } } while (0)
static void Put32(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static DWORD U32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Float(unsigned char *p, float v) { DWORD u;memcpy(&u,&v,4);Put32(p,u); }
static float F32(const unsigned char *p) { DWORD u=U32(p);float f;memcpy(&f,&u,4);return f; }
static EditorEnvironment Row(DWORD id)
{
    EditorEnvironment r={0};Put32(r.data,id);Put32(r.data+4,1);
    Float(r.data+8,10);Float(r.data+12,3000);
    Put32(r.data+16,999);Put32(r.data+20,996);Put32(r.data+24,1000);
    r.data[30]=8;r.data[31]=1;Float(r.data+32,7500);
    Float(r.data+40,70);Float(r.data+44,199);Float(r.data+48,186);
    Float(r.data+56,-1000);r.data[61]=1;Float(r.data+68,255);Float(r.data+72,255);
    Float(r.data+76,25);Float(r.data+80,18);Float(r.data+84,16);
    /* Reserved fields are opaque, not part of any editor override. */
    r.data[38]=0x93;r.data[39]=0x57;r.data[53]=0xb1;r.data[54]=0xb2;r.data[55]=0xb3;r.data[62]=0xc1;r.data[63]=0xc2;
    return r;
}
static void Edit(EditorEnvironment *r, const char *key, const char *text)
{
    for(int i=0;i<ENVIRONMENT_FIELD_COUNT;i++)if(!strcmp(key,g_EnvironmentFields[i].key))
    { OK(EnvironmentParseField(r,i,text,&why));return; }
    abort();
}
static void RoundTrip(const char *dir)
{
    const DWORD ids[]={29,129,929,229,329,429,UINT32_MAX,200,300,400};
    EnvironmentTable base={0};EnvironmentOverrides changes={0};EditorEnvironment edited,actual;
    for(unsigned i=0;i<sizeof(ids)/sizeof(ids[0]);i++)base.rows[base.count++]=Row(ids[i]);
    edited=base.rows[0];
    const char *texts[]={"0","5.25","6400.5","4294967295","995","1050",
        "1","2","3","0","8000.25","2","60.5","150.25","200.125","1","-123.5","2","100.5","90.25","80.125","12.5","24","12"};
    for(int i=0;i<ENVIRONMENT_FIELD_COUNT;i++)OK(EnvironmentParseField(&edited,i,texts[i],&why));
    OK(EnvironmentSet(&base,&changes,&edited,&why));OK(changes.count==1);
    OK(EnvironmentGet(&base,&changes,29,&actual)&&!memcmp(&actual,&edited,sizeof(actual)));
    OK(EnvironmentValidateOverrides(&base,&changes,&why));
    GEditorProject project={0},loaded={0};
    strcpy(project.name,"Environment");snprintf(project.geppath,sizeof(project.geppath),"%s/environment.gep",dir);
    strcpy(project.dir,dir);project.levelcount=1;project.levels[0].levelID=29;strcpy(project.levels[0].name,"Depot");
    project.levels[0].levelscale=project.levels[0].renderScale=project.levels[0].chrLODDistance=1;
    project.environments=base;project.environmentOverrides=changes;
    OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded));
    OK(!memcmp(&loaded.environmentOverrides,&changes,sizeof(changes)));
    FILE *f=fopen(project.geppath,"r");char header[80];OK(f&&fgets(header,sizeof(header),f));fclose(f);
    OK(!strcmp(header,"GEditor Project 5\n"));
    /* All fields have independent, known native offsets/types. */
    OK(!U32(actual.data+4)&&F32(actual.data+8)==5.25f&&F32(actual.data+12)==6400.5f);
    OK(U32(actual.data+16)==UINT32_MAX&&U32(actual.data+20)==995&&U32(actual.data+24)==1050);
    OK(actual.data[28]==1&&actual.data[29]==2&&actual.data[30]==3&&!actual.data[31]);
    OK(F32(actual.data+32)==8000.25f&&actual.data[37]==2);
    OK(F32(actual.data+40)==60.5f&&F32(actual.data+44)==150.25f&&F32(actual.data+48)==200.125f);
    OK(actual.data[52]==1&&F32(actual.data+56)==-123.5f&&actual.data[61]==2);
    OK(F32(actual.data+64)==100.5f&&F32(actual.data+68)==90.25f&&F32(actual.data+72)==80.125f);
    OK(F32(actual.data+76)==12.5f&&F32(actual.data+80)==24&&F32(actual.data+84)==12);
    unsigned char bytes[2048],before[2048];memset(bytes,0xa5,sizeof(bytes));
    for(DWORD i=0;i<base.count;i++)memcpy(bytes+128+i*88,base.rows[i].data,88);
    memset(bytes+128+base.count*88,0,88);memcpy(before,bytes,sizeof(bytes));
    RomFile rom={.data=bytes,.size=sizeof(bytes)};rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,64,sizeof(bytes),0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x454e5654,128,0,88};
    OK(EnvironmentApplyRom(&rom,&changes,&why));
    memcpy(before+128,edited.data,88);OK(!memcmp(bytes,before,sizeof(bytes)));
    EnvironmentTable saved;OK(EnvironmentReadRom(&rom,&saved,NULL,&why));
    OK(!memcmp(saved.rows[0].data,edited.data,88));
    OK(EnvironmentApplyRom(&rom,&changes,&why)&&!memcmp(bytes,before,sizeof(bytes)));
    /* Reserved data, row order/IDs, other variants, sentinel and surrounding code remain byte-identical. */
    OK(EnvironmentSet(&base,&changes,&base.rows[0],&why)&&!changes.count);
    project.environmentOverrides=changes;OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded));
    f=fopen(project.geppath,"r");OK(f&&fgets(header,sizeof(header),f));fclose(f);OK(!strcmp(header,"GEditor Project 5\n"));
    puts("PASS: every environment field, exact native bytes, integer/float round trips, project versions, reset/compaction, untouched ROM data and repeat exports.");

    EnvironmentChoice choices[ENVIRONMENT_MAX_CHOICES];
    OK(EnvironmentChoices(&base,29,choices)==6&&choices[0].id==29&&choices[1].id==129&&choices[2].id==929);
    OK(EnvironmentChoices(&base,429,choices)==3&&choices[0].id==229&&choices[2].id==429);
    OK(EnvironmentChoices(&base,430,choices)==3&&choices[0].id==200&&choices[0].shared);
    OK(EnvironmentChoices(&base,30,choices)==1&&choices[0].id==UINT32_MAX&&choices[0].shared);
    RomLevel levels[4]={{.levelID=29},{.levelID=429},{.levelID=430},{.levelID=30}};
    Edit(&base.rows[3],"fogRed","50");Edit(&base.rows[6],"fogRed","60");Edit(&base.rows[7],"fogRed","70");
    EnvironmentRefreshLevels(&base,&changes,levels,4);
    OK(levels[0].backgroundcolor[0]==0&&levels[1].backgroundcolor[0]==50&&levels[2].backgroundcolor[0]==70&&levels[3].backgroundcolor[0]==60);
    OK(levels[0].clouds.enabled&&levels[0].clouds.height==7500&&levels[0].clouds.color[1]==199);
    puts("PASS: gameplay/alternate/cinema and MP choices, explicit shared defaults, game-correct fallback and fog/cloud previews.");
}
static void Invalid(void)
{
    EnvironmentTable base={.count=1};base.rows[0]=Row(29);EnvironmentOverrides changes={0};
    EditorEnvironment r=base.rows[0];
    const char *bad[]={"", "NaN", "Infinity", "1abc", "1e1000", "-1"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++)OK(!EnvironmentParseField(&r,1,bad[i],&why));
    OK(!EnvironmentParseField(&r,3,"4294967296",&why));OK(!EnvironmentParseField(&r,6,"1.5",&why));
    OK(!EnvironmentParseField(&r,11,"3",&why));OK(!EnvironmentParseField(&r,0,"2",&why));
    Edit(&r,"nearclip","0");OK(!EnvironmentSet(&base,&changes,&r,&why)&&!changes.count);r=base.rows[0];
    Edit(&r,"farclip","5");OK(!EnvironmentSet(&base,&changes,&r,&why));r=base.rows[0];
    Edit(&r,"fogend","996");OK(!EnvironmentSet(&base,&changes,&r,&why));
    Edit(&r,"fogEnabled","0");OK(EnvironmentSet(&base,&changes,&r,&why));
    r=base.rows[0];Edit(&r,"fogend","1050");OK(EnvironmentSet(&base,&changes,&r,&why));
    Edit(&r,"propEndFade","18");OK(!EnvironmentSet(&base,&changes,&r,&why));
    Edit(&r,"propStartFade","-1");OK(EnvironmentSet(&base,&changes,&r,&why));
    EnvironmentOverrides saved=changes;
    OK(!EnvironmentReadOverride(&changes,"29|farclip|NaN"));OK(!memcmp(&changes,&saved,sizeof(changes)));
    OK(!EnvironmentReadOverride(&changes,"29|unknown|12"));OK(!EnvironmentReadOverride(&changes,"4294967296|farclip|20"));
    OK(!EnvironmentReadOverride(&changes,"-1|farclip|20"));OK(!EnvironmentReadOverride(&changes,"29|propStartFade|1"));
    unsigned char bytes[400]={0},before[400];memcpy(bytes+16,base.rows[0].data,88);memcpy(before,bytes,sizeof(bytes));
    RomFile rom={.data=bytes,.size=sizeof(bytes)};rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x434d4150,0,192,0x80000000};
    rom.info.entries[1]=(RomManifestEntry){0x454e5654,16,0,88};
    Put32(changes.rows[0].value.data,999);OK(!EnvironmentApplyRom(&rom,&changes,&why)&&!memcmp(bytes,before,sizeof(bytes)));
    EnvironmentTable table;memcpy(bytes+104,bytes+16,88);OK(!EnvironmentReadRom(&rom,&table,NULL,&why)&&!table.count);
    Put32(bytes+104,30);OK(!EnvironmentReadRom(&rom,&table,NULL,&why)&&!table.count);
    memset(bytes+104,0,88);rom.info.entries[1].flags=100;OK(!EnvironmentReadRom(&rom,&table,NULL,&why));
    puts("PASS: invalid numbers, clipping/fog/fade constraints, image bounds, malformed/duplicate overrides and ROM tables; failed export leaves bytes unchanged.");
}
static void Rebase(void)
{
    EnvironmentTable old={.count=1};old.rows[0]=Row(29);EnvironmentTable next=old;
    EnvironmentOverrides changes={0};EditorEnvironment r=old.rows[0];
    Edit(&r,"farclip","6000");OK(EnvironmentSet(&old,&changes,&r,&why));
    Edit(&next.rows[0],"skyheight","9000");OK(EnvironmentRebase(&old,&next,&changes,&why));
    OK(EnvironmentGet(&next,&changes,29,&r)&&F32(r.data+12)==6000&&F32(r.data+32)==9000);
    EnvironmentOverrides saved=changes;Edit(&next.rows[0],"farclip","5000");
    OK(!EnvironmentRebase(&old,&next,&changes,&why)&&strstr(why,"farclip")&&!memcmp(&saved,&changes,sizeof(saved)));
    Edit(&next.rows[0],"farclip","6000");OK(EnvironmentRebase(&old,&next,&changes,&why)&&!changes.count);
    r=old.rows[0];Edit(&r,"farclip","6000");OK(EnvironmentSet(&old,&changes,&r,&why));next.count=0;
    OK(!EnvironmentRebase(&old,&next,&changes,&why));
    puts("PASS: field-level three-way rebase, new untouched defaults, identical-edit cleanup, conflict/missing-row rejection and rollback.");
}
#include "migration.c"
#include "ui.c"
int main(int argc,char **argv) { assert(argc==2);RoundTrip(argv[1]);Invalid();Rebase();Migration(argv[1]);UI(argv[1]);return 0; }
