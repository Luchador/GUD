#include "levelslots.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>

static BOOL Fail(const char **why,const char *text) { if(why)*why=text;return FALSE; }
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Write32(unsigned char *p,DWORD value) { p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value; }
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
    if(!table->available)
        return !overrides->mask || Fail(why,"Rebuild GUD and rebase this project to enable Level Manager assignments.");
    if(table->value.version!=LEVEL_MENU_VERSION)return Fail(why,"Unsupported level menu version.");
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
    return TRUE;
}
BOOL LevelSlotsApplyRom(RomFile *rom,const LevelSlotOverrides *overrides,const char **why)
{
    LevelSlotTable table;
    if(!LevelSlotsReadRom(rom,&table,why)
        || !LevelSlotsValidate(&table,overrides,rom->info.levels,rom->info.levelcount,why))return FALSE;
    if(!overrides->mask)return TRUE;
    for(DWORD e=0;e<rom->info.entrycount;e++)if(rom->info.entries[e].kind==LEVEL_MENU_KIND)
    {
        unsigned char *p=rom->data+rom->info.entries[e].romstart+4;
        for(unsigned i=0;i<LEVEL_MENU_SLOT_COUNT;i++)Write32(p+i*4,(DWORD)LevelSlotsGet(&table,overrides,i));
        return TRUE;
    }
    return Fail(why,"The ROM has no editable level menu table.");
}
