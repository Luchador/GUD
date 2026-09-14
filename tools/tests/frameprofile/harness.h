#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef u32 OSIntMask;
#define TRUE 1
#define FALSE 0
#define OS_IM_NONE 1u
#define bzero(p, n) memset(p, 0, n)
static u32 testNow, testMask = 0x123401u;
static u32 counters[4], resets, reads;
static u32 osGetCount(void) { return testNow; }
static OSIntMask osSetIntMask(OSIntMask mask)
{ OSIntMask old = testMask; testMask = mask; return old; }
enum { DPC_CLOCK_REG, DPC_BUFBUSY_REG, DPC_PIPEBUSY_REG, DPC_TMEM_REG };
static u32 ReadCounter(unsigned reg) { assert(reg < 4); reads++; return counters[reg]; }
#define IO_READ(reg) ReadCounter(reg)
#define DPC_CLR_TMEM_CTR 0x40
#define DPC_CLR_PIPE_CTR 0x80
#define DPC_CLR_CMD_CTR 0x100
#define DPC_CLR_CLOCK_CTR 0x200

typedef void *OSMesg;
typedef struct { int unused; } OSMesgQueue;
typedef struct { int unused; } OSThread;
typedef struct { int unused; } OSViMode;
typedef float f32;
typedef union OSTask { struct { int type; void *output_buff; u64 *output_buff_size; } t; } OSTask;
enum { M_GFXTASK = 1, M_AUDTASK = 2 };
static s32 yielded;
static void osWritebackDCacheAll(void) {}
static void osSpTaskLoad(OSTask *task) {}
static void osSpTaskStartGo(OSTask *task) {}
static void osSpTaskYield(void) {}
static s32 osSpTaskYielded(OSTask *task) { return yielded; }
static void osDpSetStatus(u32 bits)
{
    assert(bits == (DPC_CLR_TMEM_CTR | DPC_CLR_PIPE_CTR | DPC_CLR_CMD_CTR | DPC_CLR_CLOCK_CTR));
    memset(counters, 0, sizeof(counters)); resets++;
}
static int osDpSetNextBuffer(void *buffer, u64 size) { return 0; }
