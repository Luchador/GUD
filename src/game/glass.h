#ifndef _UNK_0A1DA0_H_
#define _UNK_0A1DA0_H_
#include <ultra64.h>
#include <bondtypes.h>

typedef struct s_shattered_window_piece {
    s32 active;
    coord3d pos;        /* 0x04 */
    coord3d rot;        /* 0x10 */
    coord3d velocity;   /* 0x1c */
    coord3d angvel;     /* 0x28 */
    u32 field_0x34;

    /**
     * 3 inline 16-byte vertices start here 
     */
    s16 v1x;
    s16 v1y;
    s16 v1z;
    s16 v1flag;
    s16 v1s;
    s16 v1t;
    u8  v1r;
    u8  v1g;
    u8  v1b;
    u8  v1a;

    s16 v2x;
    s16 v2y;
    s16 v2z;
    s16 v2flag;
    s16 v2s;
    s16 v2t;
    u8  v2r;
    u8  v2g;
    u8  v2b;
    u8  v2a;

    s16 v3x;
    s16 v3y;
    s16 v3z;
    s16 v3flag;
    s16 v3s;
    s16 v3t;
    u8  v3r;
    u8  v3g;
    u8  v3b;
    u8  v3a;
} s_shattered_window_piece;

extern s32 SHATTERED_WINDOW_PIECES_BUFFER_LEN;
extern s_shattered_window_piece* ptr_shattered_window_pieces;
extern s32 g_NextShardNum;

extern u32 D_80040984;
extern u32 D_80040988;
extern u32 D_8004098C;
extern u32 D_80040990;
extern u32 watch_screen_index;
extern u32 controller_options_index;

void sub_GAME_7F0A47FC(void);
void update_broken_windows(void);

Gfx * glassRenderShards(Gfx *arg0);
void glassCreateShard(coord3d * pos, f32 rotX, f32 shard_size);

#endif
