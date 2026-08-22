#ifndef _DECOMPRESS_H_
#define _DECOMPRESS_H_
#include <ultra64.h>
#include <inflate/inflate.h>


/* huffman table workspace for decompressdata */
#define INFLATE_SCRATCH_BYTES 0x2100 

u32 decompressdata(u8 *src, u8 *dst, struct huft *hlist);
s32 rzipGetSomething(void);

#endif
