/* Host-only shim for the accounting tests. Never used by the ROM build. */
#ifndef TEST_ULTRA64_H
#define TEST_ULTRA64_H
#include <stddef.h>
#include <stdint.h>
#include <strings.h>
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t s32;
typedef u32 OSIntMask;
#define TRUE 1
#define FALSE 0
#define M_GFXTASK 1
#define M_AUDTASK 2
#define OS_IM_NONE 1
OSIntMask osSetIntMask(OSIntMask mask);
#endif
