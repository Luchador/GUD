#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texrom.h"
#include "texencode.h"

#define MB (1024u*1024u)
#define CMAP 0x1640000u
#define MANIFEST (CMAP+0x80)
#define CONFIG (CMAP+0x40)
#define TABLE (CMAP+0x1000)
#define FIRST 0x1100000u
#define SECOND 0x1680000u
#define THIRD 0x1bc0000u
#define PROPS 0x1bb0000u
#define COUNT 1280u
static const char *why;
#define OK(test) do { if(!(test)) { fprintf(stderr,"%s:%d: %s: %s\n",__FILE__,__LINE__,#test,why ? why : "");abort(); } } while(0)
static DWORD R32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void W32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Entry(RomFile *rom,DWORD index,DWORD kind,DWORD start,DWORD end,DWORD flags)
{
    unsigned char *p=rom->data+rom->info.manifestoffset+24+index*16;
    rom->info.entries[index]=(RomManifestEntry){kind,start,end,flags};
    W32(p,kind);W32(p+4,start);W32(p+8,end);W32(p+12,flags);
    if(index>=rom->info.entrycount) rom->info.entrycount=index+1;
    W32(rom->data+rom->info.manifestoffset+20,rom->info.entrycount);
}
static unsigned char *Record(int width,int height,DWORD *size)
{
    TexPixel pixels[32*32];TexImportOptions o={0,0,1,2};unsigned char *data;
    for(int i=0;i<width*height;i++) pixels[i]=(TexPixel){17,34,51,255};
    OK(TexEncodeRecord(pixels,width,height,&o,&data,size,&why));return data;
}
static RomFile Fixture(BOOL legacy)
{
    RomFile rom={0};DWORD bytes;unsigned char *record=Record(32,32,&bytes);
    rom.size=rom.info.size=legacy ? 64*MB : 32*MB;rom.data=calloc(rom.size,1);OK(rom.data);
    rom.info.manifestoffset=MANIFEST;DWORD active=legacy ? THIRD : FIRST;
    memcpy(rom.data+MANIFEST,"GUDGEDITORMANIF",16);W32(rom.data+MANIFEST+16,2);
    Entry(&rom,0,0x494d4753,active,active+COUNT*bytes,0);
    Entry(&rom,1,0x4f425347,0x101000,0x10c0000,0); /* Includes authored zero data. */
    Entry(&rom,2,0x434d4150,CMAP,SECOND,0);
    Entry(&rom,3,0x54585442,TABLE,TABLE+(TEX_IMAGE_CAPACITY+1)*8,TEX_IMAGE_CAPACITY);
    Entry(&rom,4,0x54584346,CONFIG,CONFIG+8,1);
    Entry(&rom,5,0x4e504d44,PROPS,PROPS+0x3000,0);
    Entry(&rom,6,0x4654424c,CMAP+0x900,0,0); /* Self-terminating, inside CMAP. */
    W32(rom.data+CONFIG,active);W32(rom.data+CONFIG+4,COUNT);
    for(DWORD i=0;i<COUNT;i++)
    {
        memcpy(rom.data+FIRST+i*bytes,record,bytes);
        if(legacy) { memcpy(rom.data+SECOND+i*bytes,record,bytes);memcpy(rom.data+THIRD+i*bytes,record,bytes); }
        W32(rom.data+TABLE+i*8,bytes|0xfa000000u);W32(rom.data+TABLE+i*8+4,0x38d20000u);
    }
    W32(rom.data+TABLE+COUNT*8,0xffff);
    memset(rom.data+PROPS,0x59,0x3000);free(record);return rom;
}
static DWORD Records(const RomFile *rom)
{
    DWORD count=0;
    for(DWORD at=0;at+100<=rom->size;)
    {
        TexInfoRecord record;
        if(!memcmp(rom->data+at,"GUTX",4)&&TexInfoReadRecord(rom->data+at,rom->size-at,&record))
        { count++;at+=record.size; }
        else at+=4;
    }
    return count;
}
static void Single(const RomFile *rom)
{
    TexRomBank bank;OK(TexRomReadBank(rom,&bank,&why));OK(Records(rom)==bank.count);
}
static void LegacyCleanup(void)
{
    RomFile rom=Fixture(TRUE);TexRomBank before,after;
    OK(TexRomReadBank(&rom,&before,&why));OK(Records(&rom)==3*COUNT);
    DWORD hash=before.hash,props=TexDataHash(rom.data+PROPS,0x3000);
    OK(TexRomCompactImages(&rom,&why));OK(TexRomReadBank(&rom,&after,&why));Single(&rom);
    OK(rom.size==32*MB&&after.hash==hash&&after.images<FIRST);
    OK(TexDataHash(rom.data+PROPS,0x3000)==props);
    /* Cleanup is idempotent; ordinary rebuilds do not move the bank again. */
    hash=TexDataHash(rom.data,rom.size);
    for(int i=0;i<3;i++) { OK(TexRomCompactImages(&rom,&why));OK(TexDataHash(rom.data,rom.size)==hash); }
    free(rom.data);
    puts("PASS: three legacy banks -> one bank, 64 -> 32 MiB, exact image/settings preservation and idempotence.");
}
static void RepeatedChanges(void)
{
    RomFile rom=Fixture(FALSE);TexRomBank bank,after;DWORD sizes[COUNT+3]={0},small,big;
    unsigned char *a=Record(1,1,&small),*b=Record(32,32,&big),surfaces[COUNT+3]={0};
    const unsigned char *records[COUNT+3]={0};DWORD count=COUNT;
    for(int pass=0;pass<12;pass++)
    {
        OK(TexRomReadBank(&rom,&bank,&why));
        /* Alternate shrinking and growing an existing slot, and add IDs. */
        records[7]=pass%2 ? b : a;sizes[7]=pass%2 ? big : small;surfaces[7]=0x12;
        if(pass==2||pass==6||pass==10) { records[count]=a;sizes[count]=small;surfaces[count]=0x34;count++; }
        /* Replacement input may point into the bank being moved/cleared. */
        records[0]=rom.data+bank.images;sizes[0]=big;surfaces[0]=0x56;
        OK(TexRomUpdateImages(&rom,&bank,records,sizes,surfaces,count,&why));Single(&rom);
        OK(TexRomReadBank(&rom,&after,&why)&&after.count==count&&rom.size==32*MB);
        OK(!memcmp(rom.data+after.images,b,big));
        DWORD cursor=after.images;
        for(DWORD i=0;i<count;i++)
        {
            DWORD size=R32(rom.data+after.table+i*8)&0xffffffu;
            if(i>0&&i<COUNT&&i!=7)
            { OK(size==big&&!memcmp(rom.data+cursor,b,big));OK(R32(rom.data+after.table+i*8+4)==0x38d20000u); }
            if(i==7) OK(size==sizes[7]&&!memcmp(rom.data+cursor,records[7],size));
            cursor+=size;
        }
        memset(records,0,sizeof(records));
    }
    free(a);free(b);free(rom.data);
    puts("PASS: repeated size changes, appended IDs, aliased inputs, stable IDs and untouched surface/detail flags.");
}
static void ProtectedData(void)
{
    RomFile rom=Fixture(TRUE);DWORD size;unsigned char *record=Record(1,1,&size);
    /* A fully valid texture-shaped object inside live metadata must survive. */
    memcpy(rom.data+PROPS+0x100,record,size);
    DWORD props=TexDataHash(rom.data+PROPS,0x3000);
    /* A corrupt header in unclaimed space must not be erased on magic alone. */
    memcpy(rom.data+30*MB,record,size);W32(rom.data+30*MB+12,64*MB);
    DWORD unknown=TexDataHash(rom.data+30*MB,size);
    Entry(&rom,7,0x54455354,34*MB,35*MB,0); /* Zero-filled but live. */
    rom.data[36*MB]=0x89;
    OK(TexRomCompactImages(&rom,&why));OK(rom.size==64*MB);
    OK(Records(&rom)==COUNT+1&&TexDataHash(rom.data+PROPS,0x3000)==props);
    OK(TexDataHash(rom.data+30*MB,size)==unknown&&rom.data[36*MB]==0x89);
    rom.data[36*MB]=0;
    OK(TexRomCompactImages(&rom,&why)&&rom.size==64*MB); /* Live zero tail still prevents shrink. */
    free(record);free(rom.data);
    /* An unbounded section outside CMAP also protects its unknown tail. */
    rom=Fixture(FALSE);Entry(&rom,7,0x554e4b4e,30*MB,0,0);
    OK(TexRomCompactImages(&rom,&why)&&rom.size==32*MB);Single(&rom);free(rom.data);
    puts("PASS: protected texture-shaped data, malformed headers, unknown bytes, zero-filled live ranges and unbounded sections.");
}
static void GrowthAndFailure(void)
{
    RomFile rom=Fixture(FALSE);TexRomBank bank,after;DWORD big;unsigned char *record=Record(32,32,&big);
    const unsigned char *records[COUNT+1]={0};DWORD sizes[COUNT+1]={0};unsigned char surfaces[COUNT+1]={0};
    OK(TexRomReadBank(&rom,&bank,&why));
    /* Block every possible gap so expansion is really necessary. */
    memset(rom.data+0x10c0000,0x66,FIRST-0x10c0000);
    memset(rom.data+bank.images+bank.imagebytes,0x67,CMAP-bank.images-bank.imagebytes);
    memset(rom.data+SECOND,0x68,rom.size-SECOND);
    records[COUNT]=record;sizes[COUNT]=big;surfaces[COUNT]=0x12;
    OK(TexRomUpdateImages(&rom,&bank,records,sizes,surfaces,COUNT+1,&why));
    OK(rom.size==64*MB&&TexRomReadBank(&rom,&after,&why)&&after.images>=32*MB);Single(&rom);
    /* A stale caller and a genuinely full ROM both fail without mutation. */
    DWORD hash=TexDataHash(rom.data,rom.size);
    OK(!TexRomUpdateImages(&rom,&bank,records,sizes,surfaces,COUNT+1,&why));
    OK(TexDataHash(rom.data,rom.size)==hash);free(rom.data);
    rom=Fixture(FALSE);OK(TexRomReadBank(&rom,&bank,&why));
    rom.data=realloc(rom.data,64*MB);OK(rom.data);
    memset(rom.data+0x10c0000,0x66,FIRST-0x10c0000);
    memset(rom.data+bank.images+bank.imagebytes,0x67,CMAP-bank.images-bank.imagebytes);
    memset(rom.data+SECOND,0x68,64*MB-SECOND);rom.size=rom.info.size=64*MB;
    hash=TexDataHash(rom.data,rom.size);
    OK(!TexRomUpdateImages(&rom,&bank,records,sizes,surfaces,COUNT+1,&why));
    OK(strstr(why,"64 MB")&&TexDataHash(rom.data,rom.size)==hash);Single(&rom);
    /* Declared overlap with the active bank cannot be safely reclaimed. */
    Entry(&rom,7,0x54455354,bank.images,bank.images+16,0);
    hash=TexDataHash(rom.data,rom.size);
    OK(!TexRomCompactImages(&rom,&why)&&TexDataHash(rom.data,rom.size)==hash);
    free(record);free(rom.data);
    puts("PASS: necessary growth to 64 MiB, full-ROM rejection, stale inputs and overlapping-range failure atomicity.");
}
static void LocalRom(const char *path)
{
    FILE *file=fopen(path,"rb");RomFile rom={0};TexRomBank before,after;
    OK(file&&!fseek(file,0,SEEK_END));long length=ftell(file);OK(length>0&&length<=64*MB);rewind(file);
    rom.size=rom.info.size=(DWORD)length;rom.data=malloc(rom.size);OK(rom.data);
    OK(fread(rom.data,1,rom.size,file)==rom.size&&!fclose(file));
    for(DWORD i=0;i+24<=rom.size;i+=4)
        if(!memcmp(rom.data+i,"GUDGEDITORMANIF",16)) { OK(!rom.info.manifestoffset);rom.info.manifestoffset=i; }
    OK(rom.info.manifestoffset);
    rom.info.entrycount=R32(rom.data+rom.info.manifestoffset+20);OK(rom.info.entrycount<=ROM_MAX_ENTRIES);
    for(DWORD i=0;i<rom.info.entrycount;i++)
    {
        const unsigned char *p=rom.data+rom.info.manifestoffset+24+i*16;
        rom.info.entries[i]=(RomManifestEntry){R32(p),R32(p+4),R32(p+8),R32(p+12)};
    }
    OK(TexRomReadBank(&rom,&before,&why));DWORD records=Records(&rom),oldsize=rom.size;
    unsigned char *original=malloc(rom.size);OK(original);memcpy(original,rom.data,rom.size);
    OK(TexRomCompactImages(&rom,&why));OK(TexRomReadBank(&rom,&after,&why));Single(&rom);
    OK(before.count==after.count&&before.hash==after.hash&&before.imagebytes==after.imagebytes);
    OK(!memcmp(original+before.images,rom.data+after.images,before.imagebytes));
    OK(!memcmp(original+before.table,rom.data+after.table,(before.count+1)*8));
    /* Independently construct the expected ROM by removing old records,
       installing the active bank, and updating only its location fields. */
    unsigned char *expected=malloc(oldsize);OK(expected);memcpy(expected,original,oldsize);
    for(DWORD at=0;at<oldsize;)
    {
        TexInfoRecord record;
        if(TexInfoReadRecord(original+at,oldsize-at,&record))
        {
            BOOL live=FALSE;
            for(DWORD i=0;i<rom.info.entrycount;i++)
            {
                const RomManifestEntry *e=rom.info.entries+i;
                if(e->kind!=0x494d4753&&e->romstart<at+record.size&&at<e->romend) live=TRUE;
            }
            if(!live) memset(expected+at,0,record.size);
            at+=record.size;
        }
        else at+=4;
    }
    OK(rom.size<=oldsize);
    memcpy(expected+after.images,original+before.images,before.imagebytes);
    W32(expected+before.config,after.images);
    W32(expected+before.manifestentry+4,after.images);
    W32(expected+before.manifestentry+8,after.images+after.imagebytes);
    OK(!memcmp(expected,rom.data,rom.size));
    for(DWORD at=rom.size;at<oldsize;at++) OK(!expected[at]);
    printf("PASS: local ROM %lu -> %lu MiB, %lu -> %lu records; exact bank/table and all other ROM bytes verified.\n",
        (unsigned long)(oldsize/MB),(unsigned long)(rom.size/MB),(unsigned long)records,(unsigned long)after.count);
    free(expected);free(original);free(rom.data);
}
int main(int argc,char **argv)
{
    LegacyCleanup();RepeatedChanges();ProtectedData();GrowthAndFailure();
    if(argc>1) LocalRom(argv[1]);
    return 0;
}
