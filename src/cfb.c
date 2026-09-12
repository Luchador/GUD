#include <ultra64.h>
#include <bondgame.h>

#include "fr.h"

/* ge007.ld aligns the allocation to 64 bytes. Each frame's stride must
 * preserve that alignment when the video code switches between buffers. */
u8 cfb_16[NUM_VIDEO_FRAME_BUFFERS][SCREEN_WIDTH * SCREEN_HEIGHT * 2];
typedef char CfbStrideMustBe64ByteAligned[(sizeof(cfb_16[0]) & 63) ? -1 : 1];

