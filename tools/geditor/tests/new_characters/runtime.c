/* Runtime registration on the host. ROM word order and the scratch-pointer
 * cast are adapted to the host; production functions otherwise run intact. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <src/custompropformat.h>
typedef uint32_t u32;typedef uint8_t u8;typedef int32_t s32;
typedef struct ModelFileHeader { void *RootNode,*Skeleton;u32 numSwitches,numMatrices;float BoundingVolumeRadius;u32 numTextures; } ModelFileHeader;
typedef struct ItemModelFileRecord { ModelFileHeader *header;char *filename;float scale; } ItemModelFileRecord;
typedef struct ChrModelFileRecord { ModelFileHeader *header;char *filename;float scale,pov;u8 isMale,hasHead,pad1,pad2; } ChrModelFileRecord;
typedef struct fileentry { int index;char *filename;u8 *hw_address; } fileentry;
typedef struct resource_lookup_data_entry { u32 rom_size; } resource_lookup_data_entry;
typedef struct CustomPropRomConfig { u32 version,romStart,romSize,features; } CustomPropRomConfig;
#define SKELETON(x) skeleton_##x
#define ARRAYCOUNT(x) ((s32)(sizeof(x)/sizeof(*(x))))
#define ALIGN16_a(x) (((x)+15)&~(uintptr_t)15)
#define MEMPOOL_STAGE 1
#define OBJ_INDEX_MAX 1000
#define bzero(p,n) memset(p,0,n)
static int skeleton_guard,skeleton_standard_object;
static ModelFileHeader body={NULL,&skeleton_guard,7,21,1285,13},head={NULL,NULL,2,1,190,5};
static ChrModelFileRecord CitemZ_entries[129];
static ItemModelFileRecord PitemZ_entries[2];
static unsigned char bank[1024];static void *allocation;
static void *mempAllocBytesInBank(size_t size,int pool) { assert(!(size&15));free(allocation);return allocation=malloc(size); }
static void romCopy(void *dest,void *source,u32 size)
{ uintptr_t offset=(uintptr_t)source-0x1000;assert(offset+size<=sizeof(bank));memcpy(dest,bank+offset,size); }
#include "runtime.inc"
static void Word(u32 offset,u32 value) { memcpy(bank+offset,&value,4); }
static void Fixture(void)
{
    memset(bank,0,sizeof(bank));Word(0,CUSTOM_PROP_MAGIC);Word(4,3);Word(8,96);Word(12,512);
    for(u32 i=0;i<3;i++) {
        u32 row=16+i*96;strcpy((char *)bank+row,i==0 ? "PstaticZ" : i==1 ? "CbodyZ" : "CactorZ");
        Word(row+64,304+i*176);Word(row+68,176);Word(row+72,0x3f800000);Word(row+80,0x3f800000);
        if(i) { Word(row+84,i==1 ? 1 : 2);Word(row+88,i==1 ? 5 : 78);Word(row+92,79+i); }
    }
    CitemZ_entries[5]=(ChrModelFileRecord){&body,"CdjbondZ",1.0f,1.0446f,1,0,0,0};
    CitemZ_entries[78]=(ChrModelFileRecord){&head,"CheadbrosnanZ",1.0f,1.0f,1,1,0,0};
    g_CustomPropRomConfig=(CustomPropRomConfig){1,0x1000,832,3};
}
int main(void)
{
    Fixture();customPropsInit();assert(g_CustomPropCount==3);
    assert(CitemZ_entries[80].header==&g_CustomProps[1].header);
    assert(CitemZ_entries[80].header->Skeleton==&skeleton_guard);
    assert(CitemZ_entries[80].header->numMatrices==21 && CitemZ_entries[80].header->numTextures==13);
    assert(CitemZ_entries[80].pov==CitemZ_entries[5].pov && !CitemZ_entries[80].hasHead);
    assert(CitemZ_entries[81].header->numSwitches==2 && !CitemZ_entries[81].header->Skeleton);
    assert(!strcmp(CitemZ_entries[81].filename,"CactorZ"));
    assert(customCharacterTemplate(81)==78 && customCharacterTemplate(5)==5);
    assert(propModelGet(512) && !propModelGet(513) && !propModelGet(514));
    assert(g_CustomProps[2].file.index==1002 && g_CustomProps[2].info.rom_size==176);
    /* Shared SP/MP table entries remain in the signed-byte range. */
    assert(!CitemZ_entries[82].header && !CitemZ_entries[128].header);
    CitemZ_entries[80].header->RootNode=(void *)1;customPropsReset();
    assert(!CitemZ_entries[80].header && CitemZ_entries[5].header==&body && !g_CustomProps);
    Fixture();Word(16+2*96+92,128);customPropsInit();assert(!g_CustomProps && !CitemZ_entries[80].header);
    Fixture();Word(16+2*96+92,80);customPropsInit();assert(!g_CustomProps && !CitemZ_entries[80].header);
    Fixture();Word(16+96+88,78);customPropsInit();assert(!g_CustomProps);
    Fixture();Word(16+84,0);Word(16+88,5);customPropsInit();assert(!g_CustomProps);
    Fixture();customPropsInit();assert(g_CustomPropCount==3 && CitemZ_entries[80].header->RootNode==NULL);
    customPropsReset();free(allocation);
    puts("PASS runtime body/head headers, SP/MP table registration, prop exclusion, stage reset, and atomic invalid-bank rejection.");
}
