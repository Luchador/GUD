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

/* Every live non-image section stays at its authored address. Older editors
 * left complete, self-describing GUTX banks outside those sections. Validate
 * their records before reclaiming them; never erase a magic-string match in
 * code, models, audio, metadata, or an unknown nonzero allocation. */
typedef struct TexRomSpan { DWORD start,end; } TexRomSpan;
typedef struct TexRomSpans { TexRomSpan *items; DWORD count,capacity; } TexRomSpans;
static int CompareSpans(const void *a,const void *b)
{
    const TexRomSpan *x=a,*y=b;
    return x->start<y->start ? -1 : x->start>y->start ? 1 : 0;
}
static DWORD MergeSpans(TexRomSpan *items,DWORD count)
{
    DWORD used=0;
    qsort(items,count,sizeof(*items),CompareSpans);
    for(DWORD i=0;i<count;i++)
    {
        if(used && items[i].start<=items[used-1].end)
        { if(items[i].end>items[used-1].end) items[used-1].end=items[i].end; }
        else items[used++]=items[i];
    }
    return used;
}
static BOOL AddSpan(TexRomSpans *spans,DWORD start,DWORD end)
{
    if(spans->count==spans->capacity)
    {
        DWORD capacity=spans->capacity ? spans->capacity*2 : 16;
        TexRomSpan *grown=realloc(spans->items,(size_t)capacity*sizeof(*grown));
        if(!grown) return FALSE;
        spans->items=grown;spans->capacity=capacity;
    }
    spans->items[spans->count++]=(TexRomSpan){start,end};return TRUE;
}
static BOOL ProtectedSpans(const RomFile *rom,TexRomSpan spans[ROM_MAX_ENTRIES+2],DWORD *count)
{
    DWORD used=0,manifestbytes=24+rom->info.entrycount*16,manifestend;
    if(rom->size<0x101000u || rom->size>TEX_ROM_LIMIT
        || rom->info.manifestoffset>rom->size || manifestbytes>rom->size-rom->info.manifestoffset) return FALSE;
    manifestend=rom->info.manifestoffset+manifestbytes;
    spans[used++]=(TexRomSpan){0,0x101000u}; /* Boot/code and CIC checksum input. */
    spans[used++]=(TexRomSpan){rom->info.manifestoffset,manifestend};
    for(DWORD i=0;i<rom->info.entrycount;i++)
    {
        const RomManifestEntry *e=rom->info.entries+i;
        if(e->romstart>rom->size || e->romend>rom->size || (e->romend && e->romend<e->romstart)) return FALSE;
        if(e->kind!=0x494d4753u && e->romend>e->romstart)
            spans[used++]=(TexRomSpan){e->romstart,e->romend};
    }
    /* FTBL/ENVT are normally inside CMAP. An unbounded section outside a
     * declared range protects its whole remaining tail conservatively. */
    for(DWORD i=0;i<rom->info.entrycount;i++)
    {
        const RomManifestEntry *e=rom->info.entries+i;
        if(e->romstart && !e->romend)
        {
            BOOL covered=FALSE;
            for(DWORD j=0;j<used;j++) covered|=spans[j].start<=e->romstart && e->romstart<spans[j].end;
            if(!covered) spans[used++]=(TexRomSpan){e->romstart,rom->size};
        }
    }
    *count=MergeSpans(spans,used);return TRUE;
}
static BOOL LegacyRecord(const unsigned char *data,DWORD available,DWORD *size)
{
    TexInfoRecord record;DWORD payload;
    if(!TexInfoReadRecord(data,available,&record)) return FALSE;
    payload=(DWORD)data[10]*256+data[11];
    for(DWORD i=0;i<data[6];i++) payload+=Read32(data+16+i*12+8);
    /* A length field must not claim unrelated bytes beyond its mip payload. */
    if(((payload+15)&~15u)!=record.size) return FALSE;
    for(DWORD i=payload;i<record.size;i++) if(data[i]) return FALSE;
    *size=record.size;return TRUE;
}
static BOOL ReclaimableSpans(const RomFile *rom,const TexRomBank *bank,
    const TexRomSpan *protected,DWORD protectedcount,TexRomSpans *clear)
{
    const RomManifestEntry *images=Find(rom,0x494d4753u);
    if(!AddSpan(clear,bank->images,images->romend)) return FALSE;
    for(DWORD i=0;i<protectedcount;i++)
        if(images->romstart<protected[i].end && protected[i].start<images->romend) return FALSE;
    DWORD start=0;
    for(DWORD i=0;i<=protectedcount;i++)
    {
        DWORD end=i<protectedcount ? protected[i].start : rom->size;
        for(DWORD at=(start+3)&~3u;at<end;)
        {
            if(at>=images->romstart && at<images->romend) { at=(images->romend+3)&~3u;continue; }
            DWORD size,limit=end;
            if(at<images->romstart && images->romstart<limit) limit=images->romstart;
            if(limit-at<100 || memcmp(rom->data+at,"GUTX",4) || !LegacyRecord(rom->data+at,limit-at,&size))
            { at+=4;continue; }
            DWORD first=at;
            do { at+=size; } while(at<limit && LegacyRecord(rom->data+at,limit-at,&size));
            if(!AddSpan(clear,first,at)) return FALSE;
        }
        if(i<protectedcount) start=protected[i].end;
    }
    clear->count=MergeSpans(clear->items,clear->count);return TRUE;
}
static DWORD LastOccupied(const RomFile *rom,const TexRomSpan *protected,DWORD protectedcount,
    const TexRomSpans *clear)
{
    DWORD floor=protected[protectedcount-1].end,at=rom->size,index=clear->count;
    while(at>floor)
    {
        while(index && clear->items[index-1].start>=at) index--;
        if(index && clear->items[index-1].end>=at) { at=clear->items[index-1].start;continue; }
        if(rom->data[at-1]) return at;
        at--;
    }
    return floor;
}
/* Find a contiguous span of zero padding/reclaimed records. Fixed ranges
 * remain occupied even when their contents happen to be entirely zero. */
static DWORD FreeSpace(const RomFile *rom,const TexRomSpan *protected,DWORD protectedcount,
    const TexRomSpans *clear,DWORD from,DWORD bytes)
{
    DWORD at=from,run=(from+15)&~15u,p=0,c=0;
    while(at<rom->size)
    {
        while(p<protectedcount && protected[p].end<=at) p++;
        while(c<clear->count && clear->items[c].end<=at) c++;
        if(p<protectedcount && protected[p].start<=at)
        { at=protected[p].end;run=(at+15)&~15u;continue; }
        if(c<clear->count && clear->items[c].start<=at) at=clear->items[c].end;
        else if(!rom->data[at]) at++;
        else { at++;run=(at+15)&~15u; }
        if(at>=run && at-run>=bytes) return run;
    }
    return run<=TEX_ROM_LIMIT && bytes<=TEX_ROM_LIMIT-run ? run : TEX_ROM_LIMIT;
}
static DWORD RomSize(DWORD used)
{
    DWORD size=2u*1024u*1024u;
    while(size<used && size<TEX_ROM_LIMIT) size*=2;
    return size;
}

/* One slot per final image ID. NULL keeps an original record and all its
 * flags. Replacements reset all flags to the new import settings. */
BOOL TexRomUpdateImages(RomFile *rom, const TexRomBank *bank,
    const unsigned char *const *records, const DWORD *sizes,
    const unsigned char *surfaces, DWORD count, const char **reasonout)
{
    DWORD total=0,target,end,newsize,i,cursor,original,protectedcount;
    unsigned char *grown,*packed=NULL,*table=NULL;
    TexRomBank current;TexRomSpan protected[ROM_MAX_ENTRIES+2];TexRomSpans clear={0};
    BOOL ok=FALSE;
    if(!TexRomReadBank(rom,&current,reasonout)) return FALSE;
    if(!bank || memcmp(bank,&current,sizeof(current)))
    { *reasonout="The texture bank changed. Reload it before rebuilding images.";return FALSE; }
    original=bank->images;
    *reasonout="The edited images exceed GUD's image-ID or ROM-size limits.";
    if(count<bank->count || count>bank->capacity) { return FALSE; }
    for(i=0;i<count;i++)
    {
        DWORD size;
        if(records && records[i])
        {
            TexInfoRecord info;
            if(!sizes || !surfaces || !TexInfoReadRecord(records[i],sizes[i],&info) || info.size!=sizes[i]
                || (surfaces[i]>>4)>12 || (surfaces[i]&15)>12)
            { *reasonout="An edited image contains invalid native texture data.";return FALSE; }
            size=sizes[i];
        }
        else if(i<bank->count) { size=Read32(rom->data+bank->table+i*8)&0xffffffu; }
        else { *reasonout="An appended image record is missing.";return FALSE; }
        if(size>0xffffffu-total) { return FALSE; }
        total+=size;
    }
    /* Pack before reallocating or modifying the ROM, so originals remain
     * readable and allocation/validation failures leave the ROM unchanged. */
    packed=malloc(total);table=calloc((size_t)count+1,8);
    if(!packed || !table) { goto memory; }
    cursor=0;
    for(i=0;i<count;i++)
    {
        DWORD oldsize=i<bank->count ? Read32(rom->data+bank->table+i*8)&0xffffffu : 0;
        DWORD size=records && records[i] ? sizes[i] : oldsize;
        memcpy(packed+cursor,records && records[i] ? records[i] : rom->data+original,size);
        if(records && records[i]) { Write32(table+i*8,((DWORD)surfaces[i]<<24)|size); }
        else { memcpy(table+i*8,rom->data+bank->table+i*8,8); }
        cursor+=size;original+=oldsize;
    }
    Write32(table+count*8,0xffffu);
    *reasonout="The ROM has overlapping or invalid live ranges; its texture bank cannot be compacted safely.";
    if(!ProtectedSpans(rom,protected,&protectedcount)
        || !ReclaimableSpans(rom,bank,protected,protectedcount,&clear)) goto done;
    target=FreeSpace(rom,protected,protectedcount,&clear,0x101000u,total);
    if(target==TEX_ROM_LIMIT) { *reasonout="The rebuilt texture bank cannot fit within the 64 MB ROM limit.";goto done; }
    DWORD occupied=LastOccupied(rom,protected,protectedcount,&clear);
    end=target+total;newsize=RomSize(end>occupied ? end : occupied);
    /* Keep the current address unless moving it reduces the final ROM size
     * or its growth would overwrite another allocation. */
    if(FreeSpace(rom,protected,protectedcount,&clear,bank->images,total)==bank->images)
    {
        DWORD preferred=bank->images+total;
        if(RomSize(preferred>occupied ? preferred : occupied)<=newsize)
        { target=bank->images;end=preferred; }
    }
    if(newsize>rom->size)
    {
        grown=realloc(rom->data,newsize);
        if(!grown) goto memory;
        rom->data=grown;memset(grown+rom->size,0,newsize-rom->size);
    }
    /* All fallible work is finished. Inputs may alias these old banks, so the
     * complete replacement and table were copied before clearing anything. */
    for(i=0;i<clear.count;i++) memset(rom->data+clear.items[i].start,0,clear.items[i].end-clear.items[i].start);
    memcpy(rom->data+target,packed,total);
    memcpy(rom->data+bank->table,table,((size_t)count+1)*8);
    Write32(rom->data+bank->config,target);Write32(rom->data+bank->config+4,count);
    Write32(rom->data+bank->manifestentry+4,target);Write32(rom->data+bank->manifestentry+8,end);
    for(i=0;i<rom->info.entrycount;i++) if(rom->info.entries[i].kind==0x494d4753u)
    { rom->info.entries[i].romstart=target;rom->info.entries[i].romend=end; }
    /* A failed shrinking realloc may keep the allocation; its logical size
     * still safely excludes only verified unused padding. */
    if(newsize<rom->size) { grown=realloc(rom->data,newsize);if(grown) rom->data=grown; }
    rom->size=rom->info.size=newsize;
    *reasonout="";ok=TRUE;goto done;
memory:
    *reasonout="Out of memory rebuilding the ROM's images.";
done:
    free(packed);free(table);free(clear.items);return ok;
}

BOOL TexRomCompactImages(RomFile *rom,const char **reasonout)
{
    TexRomBank bank;
    return TexRomReadBank(rom,&bank,reasonout)
        && TexRomUpdateImages(rom,&bank,NULL,NULL,NULL,bank.count,reasonout);
}
