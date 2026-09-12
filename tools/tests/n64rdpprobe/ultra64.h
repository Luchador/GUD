/* Host-only type shim; use the repository's actual GBI encoders with 32-bit
 * command words, as on N64, rather than the PC port's 128-bit Gfx layout. */
#ifndef PROBE_TEST_ULTRA64_H
#define PROBE_TEST_ULTRA64_H
#include <stdint.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef float f32;
typedef double f64;
typedef struct { int unused; } OSThread;
#define TRUE 1
#define FALSE 0
#define _ULTRA64_TYPES_H_
#define _LANGUAGE_C
#define uintptr_t u32
#include <PR/mbi.h>
#undef uintptr_t
#endif
