#include <stdlib.h>
#include <string.h>
#include "texrom.h"

#define TEX_ROM_LIMIT (64u*1024u*1024u)
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Write32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
DWORD TexDataHash(const unsigned char *data, DWORD size)
{
    DWORD hash=2166136261u,i;
    for(i=0;i<size;i++) { hash=(hash^data[i])*16777619u; }
    return hash;
}
static const RomManifestEntry *Find(const RomFile *rom,DWORD kind)
{
    const RomManifestEntry *found=NULL;DWORD i;
    for(i=0;i<rom->info.entrycount;i++)
    { if(rom->info.entries[i].kind==kind) { if(found) { return NULL; } found=&rom->info.entries[i]; } }
    return found;
}
static BOOL Range(const RomManifestEntry *e,DWORD start,DWORD end)
{ return e && e->romstart>=start && e->romstart<e->romend && e->romend<=end; }

BOOL TexRomReadBank(const RomFile *rom, TexRomBank *bank, const char **reasonout)
{
    const RomManifestEntry *images,*table,*config,*cmap;DWORD i,offset;
    ZeroMemory(bank,sizeof(*bank));
    *reasonout="This project's base ROM does not support adding images. Rebuild GUD with the image-import patch, then create a project from that ROM.";
    if(!rom || !rom->data || rom->info.entrycount>ROM_MAX_ENTRIES) { return FALSE; }
    images=Find(rom,0x494d4753u);table=Find(rom,0x54585442u);
    config=Find(rom,0x54584346u);cmap=Find(rom,0x434d4150u);
    if(!table || !config) { return FALSE; }
    *reasonout="The ROM's image table or texture configuration is invalid.";
    if(!Range(cmap,0,rom->size) || !Range(images,0,rom->size)
        || !Range(table,cmap->romstart,cmap->romend) || !Range(config,cmap->romstart,cmap->romend)
        || config->flags!=1 || config->romend-config->romstart!=8
        || table->flags==0 || table->flags>TEX_IMAGE_CAPACITY
        || table->romend-table->romstart!=(table->flags+1)*8
        || (table->romstart<config->romend && config->romstart<table->romend)
        || (images->romstart<cmap->romend && cmap->romstart<images->romend)) { return FALSE; }
    bank->count=Read32(rom->data+config->romstart+4);bank->capacity=table->flags;
    if(bank->count==0 || bank->count>bank->capacity
        || Read32(rom->data+config->romstart)!=images->romstart) { return FALSE; }
    offset=images->romstart;
    for(i=0;i<bank->count;i++)
    {
        TexInfoRecord record;
        if(!TexInfoReadRecord(rom->data+offset,images->romend-offset,&record)
            || (Read32(rom->data+table->romstart+i*8)&0xffffffu)!=record.size) { return FALSE; }
        offset+=record.size;
    }
    if(Read32(rom->data+table->romstart+bank->count*8)!=0xffffu
        || Read32(rom->data+table->romstart+bank->count*8+4)!=0) { return FALSE; }
    bank->imagebytes=offset-images->romstart;
    if(bank->imagebytes>0xffffffu) { return FALSE; }
    for(;offset<images->romend;offset++) { if(rom->data[offset]) { return FALSE; } }
    bank->manifestentry=rom->info.manifestoffset+24+(DWORD)(images-rom->info.entries)*16;
    if(bank->manifestentry>rom->size || rom->size-bank->manifestentry<16) { return FALSE; }
    bank->images=images->romstart;bank->table=table->romstart;bank->config=config->romstart;
    bank->hash=TexDataHash(rom->data+bank->images,bank->imagebytes)
        ^ TexDataHash(rom->data+bank->table,(bank->count+1)*8);
    *reasonout="";return TRUE;
}

/* One slot per final image ID. NULL keeps an original record and all its
 * flags. Replacements reset all flags to the new import settings. */
BOOL TexRomUpdateImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout)
{
    DWORD total=0,target,end,newsize,i,cursor,original=bank->images;
    unsigned char *grown,*packed=NULL,*table=NULL;
    *reasonout="The edited images exceed GUD's image-ID or ROM-size limits.";
    if(count<bank->count || count>bank->capacity) { return FALSE; }
    for(i=0;i<count;i++)
    {
        DWORD size;
        if(records[i])
        {
            TexInfoRecord info;
            if(!TexInfoReadRecord(records[i],sizes[i],&info) || info.size!=sizes[i]
                || (surfaces[i]>>4)>12 || (surfaces[i]&15)>12)
            { *reasonout="An edited image contains invalid native texture data.";return FALSE; }
            size=sizes[i];
        }
        else if(i<bank->count) { size=Read32(rom->data+bank->table+i*8)&0xffffffu; }
        else { *reasonout="An appended image record is missing.";return FALSE; }
        if(size>0xffffffu-total) { return FALSE; }
        total+=size;
    }
    /* Reuse only a verified zero-filled tail beyond every manifest range.
     * This lets OBSG and IMGS share the same expanded 32/64 MB output. */
    target=rom->info.manifestoffset+24+rom->info.entrycount*16;
    if(target<0x101000u) { target=0x101000u; }
    for(i=0;i<rom->info.entrycount;i++)
    {
        DWORD used=rom->info.entries[i].romend;
        if(used==0) { used=rom->info.entries[i].romstart+1; }
        if(used>target) { target=used; }
    }
    if(target>rom->size) { return FALSE; }
    for(i=rom->size;i>target;i--)
    { if(rom->data[i-1]!=0) { target=i;break; } }
    target=(target+15)&~15u;
    if(target>TEX_ROM_LIMIT || total>TEX_ROM_LIMIT-target) { return FALSE; }
    /* Pack before reallocating or modifying the ROM, so originals remain
     * readable and allocation/validation failures leave the ROM unchanged. */
    packed=malloc(total);table=calloc((size_t)count+1,8);
    if(!packed || !table) { goto memory; }
    cursor=0;
    for(i=0;i<count;i++)
    {
        DWORD oldsize=i<bank->count ? Read32(rom->data+bank->table+i*8)&0xffffffu : 0;
        DWORD size=records[i] ? sizes[i] : oldsize;
        memcpy(packed+cursor,records[i] ? records[i] : rom->data+original,size);
        if(records[i]) { Write32(table+i*8,((DWORD)surfaces[i]<<24)|size); }
        else { memcpy(table+i*8,rom->data+bank->table+i*8,8); }
        cursor+=size;original+=oldsize;
    }
    Write32(table+count*8,0xffffu);
    end=target+total;newsize=rom->size;
    if(newsize<end)
    {
        newsize=1024u*1024u;
        while(newsize<end) { newsize*=2; }
        grown=realloc(rom->data,newsize);
        if(!grown) { goto memory; }
        rom->data=grown;memset(grown+rom->size,0,newsize-rom->size);
        rom->size=rom->info.size=newsize;
    }
    memcpy(rom->data+target,packed,total);
    memcpy(rom->data+bank->table,table,((size_t)count+1)*8);
    free(packed);free(table);
    Write32(rom->data+bank->config,target);Write32(rom->data+bank->config+4,count);
    Write32(rom->data+bank->manifestentry+4,target);Write32(rom->data+bank->manifestentry+8,end);
    for(i=0;i<rom->info.entrycount;i++) if(rom->info.entries[i].kind==0x494d4753u)
    { rom->info.entries[i].romstart=target;rom->info.entries[i].romend=end; }
    *reasonout="";return TRUE;
memory:
    free(packed);free(table);*reasonout="Out of memory rebuilding the ROM's images.";return FALSE;
}

BOOL TexRomAppendImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout)
{
    const unsigned char **all;DWORD *lengths,total;unsigned char *settings;BOOL ok;
    if(!count) { *reasonout="";return TRUE; }
    if(count>bank->capacity-bank->count)
    { *reasonout="The imported images exceed GUD's image-ID limit.";return FALSE; }
    total=bank->count+count;
    all=calloc(total,sizeof(*all));lengths=calloc(total,sizeof(*lengths));settings=calloc(total,1);
    if(!all || !lengths || !settings)
    { free(all);free(lengths);free(settings);*reasonout="Out of memory rebuilding images.";return FALSE; }
    memcpy(all+bank->count,records,count*sizeof(*records));
    memcpy(lengths+bank->count,sizes,count*sizeof(*sizes));memcpy(settings+bank->count,surfaces,count);
    ok=TexRomUpdateImages(rom,bank,all,lengths,settings,total,reasonout);
    free(all);free(lengths);free(settings);return ok;
}
