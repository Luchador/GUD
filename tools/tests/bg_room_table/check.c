#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int32_t s32;
typedef uint32_t u32;
typedef int16_t s16;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int bool;
#define MAXROOMCOUNT 139
typedef union { struct { float x,y,z; }; float f[3]; } coord3d;
typedef union { u8 raw[16]; s16 values[8]; } Vtx;
typedef struct { u32 words[2]; } Gfx;
typedef struct { void *pPointTableBin,*primaryGraphics,*secondaryGraphics; coord3d pos; } BgRoomData;
typedef struct { union { struct { s16 minX,minY,minZ; }; s16 min[3]; };
    union { struct { s16 maxX,maxY,maxZ; }; s16 max[3]; }; } StanRoomBounds;
#include "roomtypes.inc"
static RoomInfo g_BgRoomInfo[MAXROOMCOUNT];
static BgRoomData table[MAXROOMCOUNT+1],*ptr_bgdata_room_fileposition_list=table;
static s32 g_MaxNumRooms,g_StanRoomIndexLimit;
static void *g_StanFirstTileByRoom[MAXROOMCOUNT];
static StanRoomBounds g_StanRoomBounds[MAXROOMCOUNT];
static struct { const char *bg_seg_filename; } level;
#define g_CurrentBgLevel (&level)
static unsigned char *filebytes;
static size_t filelength;
static int copycalls,zerocopies,dmastarts,waits;
static u32 lengthregister;
enum { PI_DRAM_ADDR_REG,PI_CART_ADDR_REG,PI_WR_LEN_REG,PI_RD_LEN_REG,
    OS_READ=10,OS_WRITE,OS_MESG_PRI_NORMAL,OS_MESG_BLOCK };
static int memoryMesgMB,memoryMesgQueue;
static u32 osRomBase;
#define WAIT_ON_IOBUSY(stat) ((stat)=0,(void)(stat))
#define K1_TO_PHYS(n) (n)
#define osVirtualToPhysical(p) ((u32)(uintptr_t)(p))
#define IO_WRITE(reg,val) do { if ((reg)==PI_WR_LEN_REG || (reg)==PI_RD_LEN_REG) lengthregister=(val); } while (0)
static void osInvalDCache(void *p,u32 n) { (void)p;(void)n; }
static s32 osPiRawStartDma(s32 direction,u32 devAddr,void *dramAddr,u32 size);
static void osPiStartDma(int *mb,int priority,int direction,u32 source,void *target,u32 size,int *queue)
{ (void)mb;(void)priority;(void)queue;dmastarts++;osPiRawStartDma(direction,source,target,size); }
static void osRecvMesg(int *queue,void *message,int block)
{ (void)queue;(void)message;(void)block;waits++; }
#include "dmalogic.inc"
static u32 Read(const unsigned char *p)
{ return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3]; }
static int bgGetRoomStreamSize(int offset)
{ assert(offset>=4 && (size_t)offset<=filelength);return Read(filebytes+offset-4); }
static void obLoadBGFileBytesAtOffset(const char *name,u8 *dst,int offset,int length)
{
    (void)name;copycalls++;zerocopies+=!length;
    assert(length>0 && offset>=0 && (size_t)offset+length<=filelength+15);
    romCopy(dst,(void *)(uintptr_t)offset,length);
    assert(lengthregister==(u32)length-1);
    memcpy(dst,filebytes+offset,length);
}
static int texLoadFromGdl(Gfx *src,int size,Gfx *dst,void *pool)
{ (void)pool;memmove(dst,src,size);return size; }
static void clear_light_fixturetable_in_room(int room) { (void)room; }
static void doorShadowExpandRoomBounds(int room) { (void)room; }
static void bgLoadRoomModelData(int room);
static void bgFreeRoomData(int room);
#include "roomlogic.inc"

/* Bounds exercise the real vertex loader and production bounds arithmetic;
 * texture expansion and rendering are covered by the room_cache suite. */
static void bgLoadRoomModelData(int room)
{
    unsigned char *data=malloc(filelength+64);assert(data);
    int length=bgLoadRoomVtxData(room,data,(int)filelength+64);assert(length>=0);
    for (int at=0;at<length;at+=16) for (int axis=0;axis<3;axis++) {
        s16 value=(s16)((unsigned)data[at+axis*2]<<8|data[at+axis*2+1]);
        memcpy(data+at+axis*2,&value,2);
    }
    if (!length) free(data);
    g_BgRoomInfo[room].unloadAge=1;
}
static void bgFreeRoomData(int room)
{ free(g_BgRoomInfo[room].vertices);g_BgRoomInfo[room].vertices=NULL;g_BgRoomInfo[room].unloadAge=0; }
static void CountAndDma(void)
{
    for (int count=1;count<MAXROOMCOUNT;count++) {
        memset(table,0,sizeof(table));
        for (int r=1;r<=count;r++) table[r].primaryGraphics=(void *)1;
        g_MaxNumRooms=bgCountRooms();assert(g_MaxNumRooms==count+1);
        memset(g_BgRoomInfo,0xaa,sizeof(g_BgRoomInfo));bgInitRoomStreams();
        for (int r=1;r<=count;r++) assert(g_BgRoomInfo[r].hasPrimaryStream
            && !g_BgRoomInfo[r].hasVertexStream && !g_BgRoomInfo[r].hasSecondaryStream
            && !g_BgRoomInfo[r].primaryGdl && g_BgRoomInfo[r].cur_room_totalsize==-1);
    }
    /* Reproduce the hardware encoding without touching real DMA hardware. */
    osPiRawStartDma(OS_READ,0,NULL,0);assert(lengthregister==0xffffffffu);
    romCopy(NULL,NULL,0);assert(!dmastarts && !waits);
    romCopy(NULL,NULL,16);assert(dmastarts==1 && waits==1 && lengthregister==15);
    puts("PASS room counts 1..138 include the final authored room; empty ROM copies neither start DMA nor wait; PI zero-size encoding is 0xFFFFFFFF");
}
static void Corpus(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);filelength=ftell(f);rewind(f);
    filebytes=calloc(filelength+16,1);assert(filebytes && fread(filebytes,1,filelength,f)==filelength);fclose(f);
    u32 at=Read(filebytes+4)&0xffffffu;int count=0;
    memset(table,0,sizeof(table));memset(g_BgRoomInfo,0,sizeof(g_BgRoomInfo));
    for (int r=1;r<MAXROOMCOUNT;r++) {
        assert(at+r*24+24<=filelength);const u8 *record=filebytes+at+r*24;
        if (!Read(record+4)) break;
        count=r;table[r].pPointTableBin=(void *)(uintptr_t)Read(record);
        table[r].primaryGraphics=(void *)(uintptr_t)Read(record+4);
        table[r].secondaryGraphics=(void *)(uintptr_t)Read(record+8);
        for (int a=0;a<3;a++) { u32 bits=Read(record+12+a*4);memcpy(&table[r].pos.f[a],&bits,4); }
    }
    g_MaxNumRooms=bgCountRooms();assert(g_MaxNumRooms==count+1);bgInitRoomStreams();
    unsigned char *buffer=malloc(filelength+64);assert(buffer);int empty=0;
    copycalls=zerocopies=0;
    for (int r=1;r<=count;r++) {
        if (table[r].pPointTableBin) {
            int bytes=bgGetRoomStreamSize((u32)(uintptr_t)table[r].pPointTableBin&0xffffffu);
            int before=copycalls;
            assert(bgLoadRoomVtxData(r,buffer,filelength+64)==bytes);
            if (!bytes) { empty++;assert(copycalls==before && !g_BgRoomInfo[r].vertices); }
        }
        if (table[r].primaryGraphics) assert(bgLoadRoomPrimaryGdl(r,buffer,filelength+64)>=0);
        if (table[r].secondaryGraphics) assert(bgLoadRoomSecondaryGdl(r,buffer,filelength+64)>=0);
        g_BgRoomInfo[r].unloadAge=0;
        bgRoomCalcBB(r);
        for (int a=0;a<3;a++) assert(isfinite(g_BgRoomInfo[r].minbounds.f[a])
            && isfinite(g_BgRoomInfo[r].maxbounds.f[a])
            && g_BgRoomInfo[r].minbounds.f[a]<=g_BgRoomInfo[r].maxbounds.f[a]);
    }
    assert(!zerocopies);
    if (count==60) {
        assert(!bgGetRoomStreamSize((u32)(uintptr_t)table[59].pPointTableBin&0xffffffu));
        assert(bgGetRoomStreamSize((u32)(uintptr_t)table[60].pPointTableBin&0xffffffu)==1280);
        assert(g_BgRoomInfo[60].minbounds.y<g_BgRoomInfo[60].maxbounds.y);
    }
    /* An empty room can retain collision bounds instead of its origin. */
    g_StanRoomIndexLimit=2;g_StanFirstTileByRoom[1]=(void *)1;
    for (int a=0;a<3;a++) { g_StanRoomBounds[1].min[a]=-10;g_StanRoomBounds[1].max[a]=20; }
    bgRoomSetEmptyBounds(1);assert(g_BgRoomInfo[1].minbounds.x==-10 && table[1].pos.x==5);
    g_StanRoomIndexLimit=0;g_StanFirstTileByRoom[1]=NULL;
    printf("PASS %s: %d rooms, %d empty vertex streams, final room loads, finite room bounds, no zero-byte DMA\n",path,count,empty);
    free(buffer);free(filebytes);
}
int main(int argc,char **argv)
{
    CountAndDma();for (int i=1;i<argc;i++) Corpus(argv[i]);return 0;
}
