#include "levelslots.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

static BOOL Fail(const char **why,const char *text) { if(why)*why=text;return FALSE; }
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Write32(unsigned char *p,DWORD value) { p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value; }
static BOOL ReadAssets(const RomFile *rom,LevelSlotTable *table,const char **why);
static BOOL ValidateAssets(const LevelSlotTable *table,const LevelSlotOverrides *overrides,const RomLevel *levels,DWORD count,const char **why);
static BOOL WriteAssets(FILE *file,const LevelSlotOverrides *overrides);
static void ApplyAssets(RomFile *rom,const LevelSlotTable *table,const LevelSlotOverrides *overrides);
static const int defaults[LEVEL_MENU_SLOT_COUNT] = LEVEL_MENU_DEFAULT_STAGES;

int LevelSlotsGet(const LevelSlotTable *table,const LevelSlotOverrides *overrides,unsigned slot)
{
    if(slot>=LEVEL_MENU_SLOT_COUNT)return LEVELID_NONE;
    return overrides && (overrides->mask & (1u<<slot)) ? overrides->stages[slot]
        : table && table->available ? table->value.stages[slot] : defaults[slot];
}
static BOOL TargetExists(const RomLevel *levels,DWORD count,unsigned slot,int stage)
{
    if(stage==LEVELID_NONE)return slot>=LEVEL_MENU_SOLO_COUNT;
    for(DWORD i=0;i<count;i++)if(levels[i].levelID==stage)
        return stage!=LEVELID_TITLE && levels[i].setupname[0] && levels[i].bgname[0] && levels[i].stanname[0];
    return FALSE;
}
BOOL LevelSlotsValidate(const LevelSlotTable *table,const LevelSlotOverrides *overrides,
    const RomLevel *levels,DWORD count,const char **why)
{
    if(why)*why="";
    if(!table || !overrides || !levels || count>ROM_MAX_LEVELS)return Fail(why,"Invalid level menu settings.");
    if(!table->available && !ValidateAssets(table,overrides,levels,count,why))return FALSE;
    if(!table->available)
        return !overrides->mask || Fail(why,"Rebuild GUD and rebase this project to enable Level Manager assignments.");
    if(table->value.version!=LEVEL_MENU_VERSION)return Fail(why,"Unsupported level menu version.");
    if(!ValidateAssets(table,overrides,levels,count,why))return FALSE;
    BOOL arena=FALSE;
    for(unsigned i=0;i<LEVEL_MENU_SLOT_COUNT;i++)
    {
        int stage=LevelSlotsGet(table,overrides,i);
        if(!TargetExists(levels,count,i,stage))return Fail(why,"A menu slot references a missing level or a level without setup, background and stan resources.");
        if(i>=LEVEL_MENU_SOLO_COUNT && stage!=LEVELID_NONE)arena=TRUE;
    }
    return arena || Fail(why,"The multiplayer grid needs at least one level in addition to Random.");
}
BOOL LevelSlotsReadRom(const RomFile *rom,LevelSlotTable *table,const char **why)
{
    const RomManifestEntry *entry=NULL,*cmap=NULL;
    memset(table,0,sizeof(*table));table->value.version=LEVEL_MENU_VERSION;
    memcpy(table->value.stages,defaults,sizeof(defaults));
    if(why)*why="";
    if(!rom || !rom->data || rom->info.entrycount>ROM_MAX_ENTRIES)return Fail(why,"No level menu ROM is loaded.");
    for(DWORD i=0;i<rom->info.entrycount;i++)
    {
        const RomManifestEntry *e=&rom->info.entries[i];
        if(e->kind==LEVEL_MENU_KIND) { if(entry)goto invalid;entry=e; }
        if(e->kind==0x434d4150u) { if(cmap)goto invalid;cmap=e; }
    }
    if(!entry)return TRUE; /* Legacy projects remain readable/exportable. */
    if(!cmap || cmap->romstart>=cmap->romend || cmap->romend>rom->size
        || entry->romstart<cmap->romstart || entry->romend>cmap->romend
        || entry->romstart>entry->romend || entry->romend-entry->romstart!=LEVEL_MENU_ROM_SIZE
        || (entry->romstart&3) || entry->flags!=LEVEL_MENU_VERSION
        || Read32(rom->data+entry->romstart)!=LEVEL_MENU_VERSION)goto invalid;
    /* CMAP encloses the data; no other independently editable table may alias it. */
    for(DWORD i=0;i<rom->info.entrycount;i++)
    {
        const RomManifestEntry *other=&rom->info.entries[i];
        if(other!=entry && other!=cmap && other->romend>other->romstart
            && entry->romstart<other->romend && other->romstart<entry->romend)goto invalid;
    }
    if(!ReadAssets(rom,table,why))goto invalid;
    table->available=TRUE;
    for(unsigned i=0;i<LEVEL_MENU_SLOT_COUNT;i++)table->value.stages[i]=(LONG)Read32(rom->data+entry->romstart+4+i*4);
    {
        LevelSlotOverrides empty={0};
        if(LevelSlotsValidate(table,&empty,rom->info.levels,rom->info.levelcount,why))return TRUE;
    }
invalid:
    table->available=FALSE;return Fail(why,"The ROM's level menu table is invalid or unsupported.");
}
BOOL LevelSlotsSet(const LevelSlotTable *table,LevelSlotOverrides *overrides,
    const RomLevel *levels,DWORD count,unsigned slot,int stage,const char **why)
{
    if(!table || !table->available)return Fail(why,"Rebuild GUD and rebase this project to enable Level Manager assignments.");
    if(slot>=LEVEL_MENU_SLOT_COUNT)return Fail(why,"Choose a menu box.");
    LevelSlotOverrides next=*overrides;
    next.mask|=1u<<slot;next.stages[slot]=stage;
    if(!LevelSlotsValidate(table,&next,levels,count,why))return FALSE;
    if(stage==table->value.stages[slot]) { next.mask&=~(1u<<slot);next.stages[slot]=0; }
    *overrides=next;return TRUE;
}
BOOL LevelSlotsReadOverride(LevelSlotOverrides *overrides,const char *text)
{
    char *end;long slot,stage;
    if(!overrides || !text)return FALSE;
    errno=0;slot=strtol(text,&end,10);
    if(errno || end==text || *end!='|' || slot<0 || slot>=LEVEL_MENU_SLOT_COUNT)return FALSE;
    text=end+1;errno=0;stage=strtol(text,&end,10);
    if(errno || end==text || *end || stage<-1 || stage>INT_MAX
        || (stage<0 && slot<LEVEL_MENU_SOLO_COUNT) || (overrides->mask&(1u<<slot)))return FALSE;
    overrides->mask|=1u<<slot;overrides->stages[slot]=(int)stage;return TRUE;
}
BOOL LevelSlotsWriteOverrides(FILE *file,const LevelSlotOverrides *overrides)
{
    for(unsigned i=0;i<LEVEL_MENU_SLOT_COUNT;i++)if(overrides->mask&(1u<<i))
        if(fprintf(file,"levelslot = %u|%d\n",i,overrides->stages[i])<0)return FALSE;
    return WriteAssets(file,overrides);
}
BOOL LevelSlotsApplyRom(RomFile *rom,const LevelSlotOverrides *overrides,const char **why)
{
    LevelSlotTable table;
    if(!LevelSlotsReadRom(rom,&table,why)
        || !LevelSlotsValidate(&table,overrides,rom->info.levels,rom->info.levelcount,why))return FALSE;
    ApplyAssets(rom,&table,overrides);
    if(!overrides->mask)return TRUE;
    for(DWORD e=0;e<rom->info.entrycount;e++)if(rom->info.entries[e].kind==LEVEL_MENU_KIND)
    {
        unsigned char *p=rom->data+rom->info.entries[e].romstart+4;
        for(unsigned i=0;i<LEVEL_MENU_SLOT_COUNT;i++)Write32(p+i*4,(DWORD)LevelSlotsGet(&table,overrides,i));
        return TRUE;
    }
    return Fail(why,"The ROM has no editable level menu table.");
}

static BOOL AssetEqual(const LevelMenuAsset *a,const LevelMenuAsset *b,unsigned f)
{
    unsigned bit=LEVEL_ASSET_BIT(f);
    if((a->flags&bit)!=(b->flags&bit))return FALSE;
    if(!(a->flags&bit))return TRUE;
    return f<3 ? a->images[f]==b->images[f] : f<6 ? !strcmp(a->names[f-3],b->names[f-3]) : a->briefingStage==b->briefingStage;
}
static void AssetCopyField(LevelMenuAsset *to,const LevelMenuAsset *from,unsigned f)
{
    unsigned bit=LEVEL_ASSET_BIT(f);to->flags=(to->flags&~bit)|(from->flags&bit);
    if(f<3)to->images[f]=(from->flags&bit)?from->images[f]:0;
    else if(f<6) { memset(to->names[f-3],0,LEVEL_ASSET_NAME_SIZE);if(from->flags&bit)strcpy(to->names[f-3],from->names[f-3]); }
    else to->briefingStage=(from->flags&bit)?from->briefingStage:0;
}
void LevelAssetsGet(const LevelSlotTable *table,const LevelSlotOverrides *overrides,unsigned slot,LevelMenuAsset *out)
{
    memset(out,0,sizeof(*out));if(slot>=LEVEL_MENU_SLOT_COUNT)return;
    if(table&&table->assetsAvailable)*out=table->assets.slots[slot];
    if(overrides)for(unsigned f=0;f<LEVEL_ASSET_FIELDS;f++)if(overrides->fields[slot]&LEVEL_ASSET_BIT(f))AssetCopyField(out,&overrides->assets[slot],f);
}
static BOOL ValidAsset(const LevelMenuAsset *a,unsigned slot,const RomLevel *levels,DWORD count,const char **why)
{
    if(a->flags&~(slot<20?LEVEL_ASSET_ALL:LEVEL_ASSET_MP_FIELDS))return Fail(why,"These asset settings are not supported for this menu slot.");
    for(unsigned f=0;f<3;f++)if((a->flags&LEVEL_ASSET_BIT(f))&&(unsigned)a->images[f]>=4096)return Fail(why,"Choose a valid project image.");
    for(unsigned f=0;f<3;f++) {
        if(!memchr(a->names[f],0,LEVEL_ASSET_NAME_SIZE))return Fail(why,"Menu names are limited to 31 characters.");
        if(a->flags&LEVEL_ASSET_BIT(f+3))for(const unsigned char *p=(const unsigned char *)a->names[f];*p;p++)
            if(*p<32||*p>126)return Fail(why,"Menu names support single-line English ASCII text.");
    }
    if((a->flags&LEVEL_ASSET_BIT(LEVEL_ASSET_BRIEF_SOURCE))&&a->briefingStage!=-1) {
        unsigned j;for(j=0;j<20&&defaults[j]!=a->briefingStage;j++){}
        if(j==20||!TargetExists(levels,count,0,a->briefingStage))return Fail(why,"Choose an existing single-player briefing source.");
    }
    return TRUE;
}
static BOOL ValidateAssets(const LevelSlotTable *table,const LevelSlotOverrides *overrides,const RomLevel *levels,DWORD count,const char **why)
{
    for(unsigned i=0;i<32;i++) {
        LevelMenuAsset value;
        if(overrides->fields[i]&~LEVEL_ASSET_ALL)return Fail(why,"Invalid saved menu asset fields.");
        if(overrides->fields[i]&&(!table||!table->assetsAvailable))return Fail(why,"Rebuild GUD and rebase the project to customize level menu assets.");
        /* Validate strings before field-wise copies. */
        if(!ValidAsset(&overrides->assets[i],i,levels,count,why))return FALSE;
        LevelAssetsGet(table,overrides,i,&value);if(!ValidAsset(&value,i,levels,count,why))return FALSE;
    }
    return TRUE;
}
BOOL LevelAssetsSet(const LevelSlotTable *table,LevelSlotOverrides *overrides,const RomLevel *levels,DWORD count,unsigned slot,const LevelMenuAsset *value,const char **why)
{
    if(!table||!table->assetsAvailable)return Fail(why,"Rebuild GUD and rebase the project to customize level menu assets.");
    if(slot>=32||!value)return Fail(why,"Select a menu slot.");
    if(!ValidAsset(value,slot,levels,count,why))return FALSE;
    for(unsigned f=0;f<7;f++) {
        unsigned bit=LEVEL_ASSET_BIT(f);
        if(AssetEqual(value,&table->assets.slots[slot],f)) {
            LevelMenuAsset empty={0};overrides->fields[slot]&=~bit;AssetCopyField(&overrides->assets[slot],&empty,f);
        } else {overrides->fields[slot]|=bit;AssetCopyField(&overrides->assets[slot],value,f);}
    }
    return TRUE;
}
static BOOL ReadAssets(const RomFile *rom,LevelSlotTable *table,const char **why)
{
    const RomManifestEntry *entry=NULL,*cmap=NULL;
    for(DWORD i=0;i<rom->info.entrycount;i++) {
        const RomManifestEntry *e=&rom->info.entries[i];
        if(e->kind==LEVEL_ASSETS_KIND){if(entry)return Fail(why,"Duplicate menu asset table.");entry=e;}
        if(e->kind==0x434d4150u)cmap=e;
    }
    if(!entry)return TRUE;
    if(!cmap||entry->flags!=1||entry->romstart<cmap->romstart||entry->romend>cmap->romend||entry->romend>rom->size
        ||entry->romend<entry->romstart||entry->romend-entry->romstart!=LEVEL_ASSETS_ROM_SIZE||(entry->romstart&3)
        ||Read32(rom->data+entry->romstart)!=1)return Fail(why,"Invalid level menu asset table.");
    for(DWORD i=0;i<rom->info.entrycount;i++) {
        const RomManifestEntry *e=&rom->info.entries[i];
        if(e!=entry&&e!=cmap&&e->romend>e->romstart&&e->romstart<entry->romend&&entry->romstart<e->romend)return Fail(why,"Overlapping menu asset table.");
    }
    table->assets.version=1;
    for(unsigned i=0;i<32;i++) {
        const unsigned char *p=rom->data+entry->romstart+4+i*116;LevelMenuAsset *a=&table->assets.slots[i];
        a->flags=Read32(p);for(unsigned f=0;f<3;f++)a->images[f]=(LONG)Read32(p+4+4*f);
        a->briefingStage=(LONG)Read32(p+16);memcpy(a->names,p+20,96);
        if(!ValidAsset(a,i,rom->info.levels,rom->info.levelcount,why))return FALSE;
    }
    table->assetsAvailable=TRUE;return TRUE;
}
static int Hex(char c){return c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;}
BOOL LevelAssetsReadOverride(LevelSlotOverrides *overrides,const char *text)
{
    char *end;long slot,field,value;LevelMenuAsset a={0};
    errno=0;slot=strtol(text,&end,10);if(errno||end==text||*end!='|'||slot<0||slot>=32)return FALSE;
    text=end+1;errno=0;field=strtol(text,&end,10);if(errno||end==text||*end!='|'||field<0||field>=7)return FALSE;
    if(overrides->fields[slot]&LEVEL_ASSET_BIT(field))return FALSE;
    text=end+1;
    if(strcmp(text,"default")) {
        a.flags=LEVEL_ASSET_BIT(field);
        if(field>=3&&field<6) {
            size_t n=strlen(text);if(n>62||(n&1))return FALSE;
            for(size_t i=0;i<n;i+=2) {int h=Hex(text[i]),l=Hex(text[i+1]);if(h<0||l<0||(h*16+l)<32||(h*16+l)>126)return FALSE;a.names[field-3][i/2]=h*16+l;}
        } else {
            errno=0;value=strtol(text,&end,10);if(errno||end==text||*end||value<-1||value>INT_MAX)return FALSE;
            if(field<3){if(value<0||value>=4096)return FALSE;a.images[field]=value;}else a.briefingStage=value;
        }
    }
    if(slot>=20&&(a.flags&~LEVEL_ASSET_MP_FIELDS))return FALSE;
    overrides->fields[slot]|=LEVEL_ASSET_BIT(field);AssetCopyField(&overrides->assets[slot],&a,field);return TRUE;
}
static BOOL WriteAssets(FILE *file,const LevelSlotOverrides *overrides)
{
    for(unsigned i=0;i<32;i++)for(unsigned f=0;f<7;f++)if(overrides->fields[i]&LEVEL_ASSET_BIT(f)) {
        const LevelMenuAsset *a=&overrides->assets[i];if(fprintf(file,"levelasset = %u|%u|",i,f)<0)return FALSE;
        if(!(a->flags&LEVEL_ASSET_BIT(f))) {if(fputs("default",file)==EOF)return FALSE;}
        else if(f>=3&&f<6) {for(const unsigned char *p=(const unsigned char *)a->names[f-3];*p;p++)if(fprintf(file,"%02X",*p)<0)return FALSE;}
        else if(fprintf(file,"%d",f<3?a->images[f]:a->briefingStage)<0)return FALSE;
        if(fputc('\n',file)==EOF)return FALSE;
    }
    return TRUE;
}
static void ApplyAssets(RomFile *rom,const LevelSlotTable *table,const LevelSlotOverrides *overrides)
{
    if(!table->assetsAvailable)return;
    for(DWORD e=0;e<rom->info.entrycount;e++)if(rom->info.entries[e].kind==LEVEL_ASSETS_KIND)
        for(unsigned i=0;i<32;i++) {
            LevelMenuAsset a;unsigned char *p=rom->data+rom->info.entries[e].romstart+4+i*116;
            LevelAssetsGet(table,overrides,i,&a);Write32(p,a.flags);for(unsigned f=0;f<3;f++)Write32(p+4+4*f,a.images[f]);
            Write32(p+16,a.briefingStage);memcpy(p+20,a.names,96);
        }
}

/* Validate the final packed resources, after image/model/text replacements.
 * This also runs during rebase staging; source files are never modified. */
#include "briefing.h"
#include "texload.h"
#include "../../../src/game/levelmenuwallet.h"
BOOL LevelAssetsValidateRom(const RomFile *rom,const LevelSlotTable *table,const LevelSlotOverrides *overrides,const char **why)
{
    LevelMenuAsset assets[32];BOOL artwork=FALSE;DWORD need[4096]={0},offset,span;
    const RomManifestEntry *images=NULL;
    if(!LevelSlotsValidate(table,overrides,rom->info.levels,rom->info.levelcount,why))return FALSE;
    for(unsigned i=0;i<32;i++) {
        LevelAssetsGet(table,overrides,i,&assets[i]);
        for(unsigned f=0;f<3;f++)if(assets[i].flags&LEVEL_ASSET_BIT(f)) {
            need[assets[i].images[f]]|=1u<<f;if(i<20)artwork=TRUE;
        }
        if((assets[i].flags&LEVEL_ASSET_BIT(6))&&assets[i].briefingStage!=-1) {
            const char *bank,*brief;
            if(!BriefingNames(assets[i].briefingStage,&bank,&brief)||!RomFindFile(rom,brief,&offset,&span,why)||span<48
                ||!RomFindFile(rom,bank,&offset,&span,why))return Fail(why,"A selected briefing or its language bank is missing from the exported ROM.");
        }
    }
    for(DWORD i=0;i<rom->info.entrycount;i++)if(rom->info.entries[i].kind==0x494d4753u)images=&rom->info.entries[i];
    if(images) {
        DWORD at=images->romstart,id=0;
        if(images->romstart>images->romend||images->romend>rom->size)return Fail(why,"Invalid image bank.");
        while(at<images->romend&&id<4096) {
            const unsigned char *p=rom->data+at;TexInfoRecord record;
            if(!TexInfoReadRecord(p,images->romend-at,&record))break;
            if(need[id])for(unsigned f=0;f<3;f++)if(need[id]&(1u<<f))
                if(record.info.mipmaps||!LEVEL_ASSET_IMAGE_COMPATIBLE(record.info.format,p[17],p[18],f))
                    return Fail(why,"A menu image is incompatible. Icons need 68x44 IA8/I8/IA4/I4/CI4; briefing halves need 128x64 IA4/I4, without mipmaps.");
            need[id]=0;id++;at+=record.size;
        }
    }
    for(unsigned i=0;i<4096;i++)if(need[i])return Fail(why,"A selected menu image is missing or deleted.");
    if(artwork) {
        unsigned char *copy;int order[20];
        if(!RomFindFile(rom,"PwalletbondZ",&offset,&span,why))return FALSE;
        copy=malloc(span);if(!copy)return Fail(why,"Out of memory checking menu artwork.");
        memcpy(copy,rom->data+offset,span);for(int i=0;i<20;i++)order[i]=i;
        BOOL ok=levelWalletPatch(copy,span,43,84,assets,order,NULL);free(copy);
        if(!ok)return Fail(why,"The wallet model's menu picture layout is incompatible with these artwork assignments.");
    }
    return TRUE;
}
