#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "repack.inc"
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Save(const char *path,const void *p,DWORD size)
{ FILE *f=fopen(path,"wb");assert(f && fwrite(p,1,size,f)==size);assert(!fclose(f)); }
static void Repack(void)
{
    RomFile rom={0};RomExportSlot slots[2]={0};const char *why="";
    RomManifestEntry *obsg=&rom.info.entries[0],*ftbl=&rom.info.entries[1];
    rom.size=8192;rom.data=calloc(rom.size,1);rom.info.manifestoffset=64;rom.info.entrycount=2;
    obsg->romstart=2048;obsg->romend=2560;ftbl->romstart=256;
    Put(rom.data+260,1);Put(rom.data+264,2048);
    Put(rom.data+272,2);Put(rom.data+276,2304);
    /* A second file-table name aliases the following resource. */
    Put(rom.data+284,3);Put(rom.data+288,2304);
    slots[0].offset=2048;slots[0].length=256;slots[0].setup=TRUE;
    slots[0].replacementlength=48;slots[0].replacement=calloc(48,1);
    slots[1].offset=2304;slots[1].length=256;memset(rom.data+2304,0x5a,256);
    assert(RomExportRepackResources(&rom,obsg,ftbl,slots,2,&why));
    assert(obsg->romstart==2048 && obsg->romend==2352);
    assert(Get(rom.data+64+24+8)==2352);
    assert(Get(rom.data+276)==2096 && Get(rom.data+288)==2096);
    for(DWORD i=2096;i<2352;i++) { assert(rom.data[i]==0x5a); }
    for(DWORD i=2352;i<2560;i++) { assert(!rom.data[i]); }
    /* Later edits reuse the padding reclaimed by an earlier export. */
    slots[0].offset=slots[0].newoffset;slots[0].length=48;
    slots[1].offset=slots[1].newoffset;
    free(slots[0].replacement);slots[0].replacement=calloc(64,1);slots[0].replacementlength=64;
    assert(RomExportRepackResources(&rom,obsg,ftbl,slots,2,&why));
    assert(obsg->romstart==2048&&obsg->romend==2368&&rom.size==8192);
    /* A neighboring segment is protected even if its payload is all zero. */
    rom.info.entrycount=3;rom.info.entries[2].romstart=2368;rom.info.entries[2].romend=2384;
    slots[0].offset=slots[0].newoffset;slots[0].length=64;slots[1].offset=slots[1].newoffset;
    free(slots[0].replacement);slots[0].replacement=calloc(80,1);slots[0].replacementlength=80;
    assert(RomExportRepackResources(&rom,obsg,ftbl,slots,2,&why));
    assert(obsg->romstart==8192&&obsg->romend==8528);
    for(DWORD i=2368;i<2384;i++) { assert(!rom.data[i]); }
    free(slots[0].replacement);free(rom.data);
}
int main(int argc,char **argv)
{
    FILE *f;long size;unsigned char *data,*packed,*again,*original;DWORD outsize,asize;const char *why="";
    assert(argc==3);Repack();f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);size=ftell(f);rewind(f);
    data=malloc(size);original=malloc(size);assert(fread(data,1,size,f)==(size_t)size);fclose(f);memcpy(original,data,size);
    if(!SetupCompactNative(data,size,&packed,&outsize,&why)) { fprintf(stderr,"%s\n",why);return 1; }
    assert(!memcmp(original,data,size));
    assert(SetupCompactNative(packed,outsize,&again,&asize,&why));assert(asize==outsize && !memcmp(packed,again,outsize));free(again);
    /* Invalid root pointers fail atomically with no result. */
    Put(data,size+16);again=(void *)1;asize=1;
    assert(!SetupCompactNative(data,size,&again,&asize,&why) && !again && !asize);
    Save(argv[2],packed,outsize);free(data);free(original);free(packed);return 0;
}
