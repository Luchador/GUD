#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "project.h"
#include "texencode.h"
#include "../../../src/game/levelmenuwallet.h"
static const char *why="";
#define OK(x) do {if(!(x)){fprintf(stderr,"%d: %s: %s\n",__LINE__,#x,why);abort();}}while(0)
static void Put32(unsigned char *p,unsigned v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static void Fixture(RomFile *rom,unsigned char *data)
{
    const int ids[32]=LEVEL_MENU_DEFAULT_STAGES;
    memset(rom,0,sizeof(*rom));memset(data,0,8192);rom->data=data;rom->size=8192;
    rom->info.entrycount=3;rom->info.entries[0]=(RomManifestEntry){0x434d4150,64,8192,0x80000000};
    rom->info.entries[1]=(RomManifestEntry){LEVEL_MENU_KIND,128,260,1};
    rom->info.entries[2]=(RomManifestEntry){LEVEL_ASSETS_KIND,512,512+LEVEL_ASSETS_ROM_SIZE,1};
    Put32(data+128,1);Put32(data+512,1);
    for(unsigned i=0;i<32;i++) {
        Put32(data+132+4*i,ids[i]);if(ids[i]<0)continue;
        RomLevel *l=&rom->info.levels[rom->info.levelcount++];l->levelID=ids[i];
        strcpy(l->name,"Level");strcpy(l->bgname,"bg/test.seg");strcpy(l->setupname,"UtestZ");strcpy(l->stanname,"TtestZ");
        l->levelscale=l->renderScale=l->chrLODDistance=1;
    }
}
static void Data(const char *dir)
{
    unsigned char data[8192],before[8192];RomFile rom;LevelSlotTable base,read;LevelSlotOverrides edits={0},snapshot;
    LevelMenuAsset asset={0},actual;
    Fixture(&rom,data);OK(LevelSlotsReadRom(&rom,&base,&why)&&base.assetsAvailable);
    asset.flags=127;asset.images[0]=0xa19;asset.images[1]=0x9f8;asset.images[2]=0x9f9;
    strcpy(asset.names[0],"Menu | icon");strcpy(asset.names[1],"Mission");strcpy(asset.names[2],"Part");asset.briefingStage=26;
    OK(LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    asset.flags=LEVEL_ASSET_MP_FIELDS;strcpy(asset.names[0],"Arena");
    OK(LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,31,&asset,&why));
    snapshot=edits;asset.flags|=LEVEL_ASSET_BIT(4);
    OK(!LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,31,&asset,&why)&&!memcmp(&edits,&snapshot,sizeof(edits)));
    asset.flags=LEVEL_ASSET_BIT(3);memset(asset.names[0],'x',32);
    OK(!LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    strcpy(asset.names[0],"bad\nname");OK(!LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    asset.flags=LEVEL_ASSET_BIT(6);asset.briefingStage=LEVELID_COMPLEX;memset(asset.names,0,sizeof(asset.names));
    OK(!LevelAssetsSet(&base,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    GEditorProject project={0},loaded={0};strcpy(project.name,"Assets");strcpy(project.dir,dir);
    snprintf(project.geppath,sizeof(project.geppath),"%s/assets.gep",dir);project.levelcount=rom.info.levelcount;
    memcpy(project.levels,rom.info.levels,sizeof(project.levels));project.levelSlots=base;project.levelSlotOverrides=edits;
    OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded));
    OK(!memcmp(&edits,&loaded.levelSlotOverrides,sizeof(edits)));
    memcpy(before,data,sizeof(data));OK(LevelSlotsApplyRom(&rom,&edits,&why));
    OK(!memcmp(data,before,512)&&!memcmp(data+512+LEVEL_ASSETS_ROM_SIZE,before+512+LEVEL_ASSETS_ROM_SIZE,8192-512-LEVEL_ASSETS_ROM_SIZE));
    OK(LevelSlotsReadRom(&rom,&read,&why));LevelAssetsGet(&read,NULL,0,&actual);
    OK(actual.flags==127&&actual.images[2]==0x9f9&&!strcmp(actual.names[0],"Menu | icon")&&actual.briefingStage==26);
    /* Explicit Default clears a custom base field, then survives save/reopen. */
    LevelMenuAsset cleared=actual;cleared.flags&=~LEVEL_ASSET_BIT(3);LevelSlotOverrides reset={0};
    OK(LevelAssetsSet(&read,&reset,rom.info.levels,rom.info.levelcount,0,&cleared,&why));
    OK(reset.fields[0]==LEVEL_ASSET_BIT(3)&&!reset.assets[0].flags);
    project.levelSlots=read;project.levelSlotOverrides=reset;
    OK(ProjectSave(&project,&why)&&ProjectRead(project.geppath,&loaded)&&!memcmp(&loaded.levelSlotOverrides,&reset,sizeof(reset)));
    LevelSlotTable next=read;strcpy(next.assets.slots[0].names[0],"incoming");strcpy(next.assets.slots[0].names[2],"new part");
    LevelAssetsGet(&next,&reset,0,&actual);OK(!(actual.flags&LEVEL_ASSET_BIT(3))&&!strcmp(actual.names[2],"new part"));
    const char *bad[]={"0|3|00","0|3|0A","0|3|7F","0|3|a","0|3|GG","32|3|41","0|7|1","0|0|-1","0|0|4096","20|4|41","0|6|2147483648"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){LevelSlotOverrides invalid={0};OK(!LevelAssetsReadOverride(&invalid,bad[i]));}
    LevelSlotOverrides parsed={0};OK(LevelAssetsReadOverride(&parsed,"0|3|default")&&!LevelAssetsReadOverride(&parsed,"0|3|41"));
    for(int fault=0;fault<6;fault++) {
        Fixture(&rom,data);
        switch(fault) {
        case 0:rom.info.entries[2].romend++;break;
        case 1:rom.info.entries[2].flags=2;break;
        case 2:Put32(data+512,2);break;
        case 3:Put32(data+516,128);break;
        case 4:memset(data+536,'x',32);break;
        case 5:rom.info.entries[3]=rom.info.entries[2];rom.info.entrycount++;break;
        }
        memcpy(before,data,sizeof(data));OK(!LevelSlotsApplyRom(&rom,&edits,&why)&&!memcmp(data,before,sizeof(data)));
    }
    Fixture(&rom,data);rom.info.entrycount=2;OK(LevelSlotsReadRom(&rom,&read,&why)&&!read.assetsAvailable);
    OK(!LevelSlotsApplyRom(&rom,&edits,&why));
    memset(&edits,0,sizeof(edits));OK(LevelSlotsSet(&read,&edits,rom.info.levels,rom.info.levelcount,0,26,&why));
    OK(LevelSlotsApplyRom(&rom,&edits,&why));
    puts("PASS: all asset fields, independent SP/MP settings, project round-trip, exact ROM writes, per-field defaults/rebase, malformed data and v1 compatibility.");
}
static void Wallet(const char *path)
{
    FILE *f=fopen(path,"rb");OK(f);fseek(f,0,SEEK_END);unsigned size=ftell(f);rewind(f);
    unsigned char *data=malloc(size),*original=malloc(size);OK(fread(data,1,size,f)==size);fclose(f);memcpy(original,data,size);
    unsigned stock[60],changed[60];LevelMenuAsset assets[32]={{0}};int order[20];for(int i=0;i<20;i++)order[i]=i;
    OK(levelWalletPatch(data,size,43,84,assets,order,stock)&&!memcmp(data,original,size));
    OK(stock[0]==0xa19&&stock[1]==0x9f8&&stock[2]==0x9f9);
    assets[0].flags=7;assets[0].images[0]=0xabc;assets[0].images[1]=0xabd;assets[0].images[2]=0xabe;
    assets[1].flags=1;assets[1].images[0]=stock[0]; /* Must not cascade to 0xabc. */
    OK(levelWalletPatch(data,size,43,84,assets,order,NULL));
    LevelMenuAsset empty[32]={{0}};OK(levelWalletPatch(data,size,43,84,empty,order,changed));
    OK(changed[0]==0xabc&&changed[1]==0xabd&&changed[2]==0xabe&&changed[3]==stock[0]);
    for(int i=4;i<60;i++)OK(changed[i]==stock[i]);
    int found=0;for(int i=0;i<84;i++){unsigned id=levelWalletRead(data+172+i*12);if(id==0xabc)found++;OK(id!=stock[1]&&id!=stock[2]);}
    OK(found==1);memcpy(data,original,size);data[21*4]=0xff;memcpy(original,data,size);
    OK(!levelWalletPatch(data,size,43,84,assets,order,NULL)&&!memcmp(data,original,size));
    free(data);free(original);puts("PASS: actual wallet model icon/briefing halves, other slots preserved, noncascading replacements, exact preloads, malformed model atomic rejection.");
}

static void Resources(const char *wallet)
{
    unsigned char *data=calloc(100000,1),*image;DWORD bytes;
    RomFile rom;Fixture(&rom,data);rom.size=100000;
    FILE *file=fopen(wallet,"rb");OK(file);fseek(file,0,SEEK_END);DWORD size=ftell(file);rewind(file);
    OK(fread(data+20000,1,size,file)==size);fclose(file);
    rom.info.entries[rom.info.entrycount++]=(RomManifestEntry){0x4654424c,5000,5048,0};
    rom.info.entries[rom.info.entrycount++]=(RomManifestEntry){0x4f425347,20000,20000+size+64,0};
    const char *names[]={"PwalletbondZ","UbriefdamZ","LdamE"};
    DWORD positions[]={20000,20000+size,20000+size+48};
    for(int i=0;i<3;i++) {
        strcpy((char *)data+6000+i*32,names[i]);Put32(data+5000+i*12,i+1);
        Put32(data+5004+i*12,0x80000000+6000+i*32-64);Put32(data+5008+i*12,positions[i]);
    }
    TexPixel *pixels=calloc(128*64,sizeof(*pixels));TexImportOptions options={5,0,0,0};
    OK(TexEncodeRecord(pixels,68,44,&options,&image,&bytes,&why));memcpy(data+8192,image,bytes);free(image);
    DWORD half=8192+bytes;options.format=6;
    OK(TexEncodeRecord(pixels,128,64,&options,&image,&bytes,&why));memcpy(data+half,image,bytes);free(image);
    rom.info.entries[rom.info.entrycount++]=(RomManifestEntry){0x494d4753,8192,half+bytes,0};
    LevelSlotTable table;LevelSlotOverrides edits={0};LevelMenuAsset asset={0};
    OK(LevelSlotsReadRom(&rom,&table,&why));asset.flags=7|LEVEL_ASSET_BIT(6);asset.images[1]=asset.images[2]=1;asset.briefingStage=33;
    OK(LevelAssetsSet(&table,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    OK(LevelAssetsValidateRom(&rom,&table,&edits,&why));
    /* Validation must not bake artwork into the project's original model. */
    unsigned original[60];LevelMenuAsset empty[20]={{0}};int order[20];for(int i=0;i<20;i++)order[i]=i;
    OK(levelWalletPatch(data+20000,size,43,84,empty,order,original)&&original[0]==0xa19);
    asset.images[0]=2;OK(LevelAssetsSet(&table,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    OK(!LevelAssetsValidateRom(&rom,&table,&edits,&why));
    asset.images[0]=1;OK(LevelAssetsSet(&table,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    OK(!LevelAssetsValidateRom(&rom,&table,&edits,&why)); /* Valid texture, wrong role dimensions. */
    asset.images[0]=0;OK(LevelAssetsSet(&table,&edits,rom.info.levels,rom.info.levelcount,0,&asset,&why));
    data[8197]=2;OK(!LevelAssetsValidateRom(&rom,&table,&edits,&why));data[8197]=1;
    data[6064]='X';OK(!LevelAssetsValidateRom(&rom,&table,&edits,&why));data[6064]='L';
    data[20000+84]=0xff;OK(!LevelAssetsValidateRom(&rom,&table,&edits,&why));data[20000+84]=5;
    OK(LevelAssetsValidateRom(&rom,&table,&edits,&why));free(pixels);free(data);
    puts("PASS: final ROM artwork and briefing resources, exact native dimensions/formats, missing images/banks, mipmaps, malformed wallet rejection and no source mutation.");
}
int main(int argc,char **argv){assert(argc==3);Data(argv[1]);Wallet(argv[2]);Resources(argv[2]);return 0;}
