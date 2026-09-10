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

BOOL TexRomAppendImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout)
{
    DWORD total=bank->imagebytes,target,end,newsize,i,cursor;
    unsigned char *grown;
    *reasonout="The imported images exceed GUD's image-ID or ROM-size limits.";
    if(count==0) { *reasonout="";return TRUE; }
    if(count>bank->capacity-bank->count) { return FALSE; }
    for(i=0;i<count;i++)
    {
        TexInfoRecord info;
        if(!TexInfoReadRecord(records[i],sizes[i],&info) || info.size!=sizes[i]
            || (surfaces[i]>>4)>12 || (surfaces[i]&15)>12)
        { *reasonout="An imported image contains invalid native texture data.";return FALSE; }
        if(sizes[i]>0xffffffu-total) { return FALSE; }
        total+=sizes[i];
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
    end=target+total;newsize=rom->size;
    if(newsize<end)
    {
        newsize=1024u*1024u;
        while(newsize<end) { newsize*=2; }
        grown=realloc(rom->data,newsize);
        if(!grown) { *reasonout="Out of memory growing the ROM for imported images.";return FALSE; }
        rom->data=grown;memset(grown+rom->size,0,newsize-rom->size);
        rom->size=rom->info.size=newsize;
    }
    memmove(rom->data+target,rom->data+bank->images,bank->imagebytes);
    cursor=target+bank->imagebytes;
    for(i=0;i<count;i++)
    {
        unsigned char *entry=rom->data+bank->table+(bank->count+i)*8;
        memcpy(rom->data+cursor,records[i],sizes[i]);cursor+=sizes[i];
        Write32(entry,((DWORD)surfaces[i]<<24)|sizes[i]);Write32(entry+4,0);
    }
    Write32(rom->data+bank->table+(bank->count+count)*8,0xffffu);
    Write32(rom->data+bank->table+(bank->count+count)*8+4,0);
    Write32(rom->data+bank->config,target);Write32(rom->data+bank->config+4,bank->count+count);
    Write32(rom->data+bank->manifestentry+4,target);Write32(rom->data+bank->manifestentry+8,end);
    for(i=0;i<rom->info.entrycount;i++) if(rom->info.entries[i].kind==0x494d4753u)
    { rom->info.entries[i].romstart=target;rom->info.entries[i].romend=end; }
    *reasonout="";return TRUE;
}
