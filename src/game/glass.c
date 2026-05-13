#include <ultra64.h>
#include <limits.h>
#include "math_atan2f.h"
#include "glass.h"
#include "random.h"
#include "lv.h"
#include "objective_status.h"
#include "image_bank.h"

#ifndef VERSION_EU
#define SHARD_HORIZ_VEL_SCALE 1.5f
#define SHARD_VERT_VEL_SCALE 3.0f
#define SHARD_ANGVEL_SCALE 0.1f
#else
#define SHARD_HORIZ_VEL_SCALE 1.8f
#define SHARD_VERT_VEL_SCALE 3.6f
#define SHARD_ANGVEL_SCALE 0.12f
#endif

#define UNK_8007A170_MAX 20

// bss
//CODE.bss:8007A160
s32 SHATTERED_WINDOW_PIECES_BUFFER_LEN;
//CODE.bss:8007A164
s_shattered_window_piece* ptr_shattered_window_pieces;
//CODE.bss:8007A168
u32 dword_CODE_bss_8007A168;
//CODE.bss:8007A16C
u32 dword_CODE_bss_8007A16C;



// data
//D:80040940
s32 g_NextShardNum = 0;
u32 D_80040944 = 0;
u32 D_80040948 = 0;
u32 D_8004094C = 0;
u32 D_80040950 = 0;
u32 D_80040954 = 0;
u32 D_80040958 = 0;
u32 D_8004095C = 0;







// rodata



#ifdef NONMATCHING
void sub_GAME_7F0A1DA0(f32*, f32*, f32*, f32*, f32, f32, f32, f32, f32, f32) {

}
#else
void sub_GAME_7F0A1DA0(f32*, f32*, f32*, f32*, f32, f32, f32, f32, f32, f32);
GLOBAL_ASM(
.late_rodata
glabel D_80057710
.word 0x3dcccccd /*0.1*/
glabel D_80057714
.word 0x3f333333 /*0.69999999*/
.text
glabel sub_GAME_7F0A1DA0
/* 0D68D0 7F0A1DA0 27BDFF20 */  addiu $sp, $sp, -0xe0
/* 0D68D4 7F0A1DA4 AFBF005C */  sw    $ra, 0x5c($sp)
/* 0D68D8 7F0A1DA8 AFB50058 */  sw    $s5, 0x58($sp)
/* 0D68DC 7F0A1DAC AFB40054 */  sw    $s4, 0x54($sp)
/* 0D68E0 7F0A1DB0 AFB30050 */  sw    $s3, 0x50($sp)
/* 0D68E4 7F0A1DB4 AFB2004C */  sw    $s2, 0x4c($sp)
/* 0D68E8 7F0A1DB8 AFB10048 */  sw    $s1, 0x48($sp)
/* 0D68EC 7F0A1DBC AFB00044 */  sw    $s0, 0x44($sp)
/* 0D68F0 7F0A1DC0 F7BE0038 */  sdc1  $f30, 0x38($sp)
/* 0D68F4 7F0A1DC4 F7BC0030 */  sdc1  $f28, 0x30($sp)
/* 0D68F8 7F0A1DC8 F7BA0028 */  sdc1  $f26, 0x28($sp)
/* 0D68FC 7F0A1DCC F7B80020 */  sdc1  $f24, 0x20($sp)
/* 0D6900 7F0A1DD0 F7B60018 */  sdc1  $f22, 0x18($sp)
/* 0D6904 7F0A1DD4 F7B40010 */  sdc1  $f20, 0x10($sp)
/* 0D6908 7F0A1DD8 C4A40000 */  lwc1  $f4, ($a1)
/* 0D690C 7F0A1DDC 00C08025 */  move  $s0, $a2
/* 0D6910 7F0A1DE0 00808825 */  move  $s1, $a0
/* 0D6914 7F0A1DE4 E7A40090 */  swc1  $f4, 0x90($sp)
/* 0D6918 7F0A1DE8 C4A60004 */  lwc1  $f6, 4($a1)
/* 0D691C 7F0A1DEC C7AA0090 */  lwc1  $f10, 0x90($sp)
/* 0D6920 7F0A1DF0 00E09025 */  move  $s2, $a3
/* 0D6924 7F0A1DF4 E7A60094 */  swc1  $f6, 0x94($sp)
/* 0D6928 7F0A1DF8 460A5402 */  mul.s $f16, $f10, $f10
/* 0D692C 7F0A1DFC C4A80008 */  lwc1  $f8, 8($a1)
/* 0D6930 7F0A1E00 C7B20094 */  lwc1  $f18, 0x94($sp)
/* 0D6934 7F0A1E04 E7A80098 */  swc1  $f8, 0x98($sp)
/* 0D6938 7F0A1E08 46129102 */  mul.s $f4, $f18, $f18
/* 0D693C 7F0A1E0C C7A80098 */  lwc1  $f8, 0x98($sp)
/* 0D6940 7F0A1E10 46084282 */  mul.s $f10, $f8, $f8
/* 0D6944 7F0A1E14 46048180 */  add.s $f6, $f16, $f4
/* 0D6948 7F0A1E18 0C007DF8 */  jal   sqrtf
/* 0D694C 7F0A1E1C 46065300 */   add.s $f12, $f10, $f6
/* 0D6950 7F0A1E20 3C013F80 */  li    $at, 0x3F800000 # 1.000000
/* 0D6954 7F0A1E24 4481D000 */  mtc1  $at, $f26
/* 0D6958 7F0A1E28 C7B20090 */  lwc1  $f18, 0x90($sp)
/* 0D695C 7F0A1E2C C7A40094 */  lwc1  $f4, 0x94($sp)
/* 0D6960 7F0A1E30 4600D503 */  div.s $f20, $f26, $f0
/* 0D6964 7F0A1E34 C7AA0098 */  lwc1  $f10, 0x98($sp)
/* 0D6968 7F0A1E38 C7B600F0 */  lwc1  $f22, 0xf0($sp)
/* 0D696C 7F0A1E3C C7BC00F4 */  lwc1  $f28, 0xf4($sp)
/* 0D6970 7F0A1E40 46149402 */  mul.s $f16, $f18, $f20
/* 0D6974 7F0A1E44 00000000 */  nop
/* 0D6978 7F0A1E48 46142202 */  mul.s $f8, $f4, $f20
/* 0D697C 7F0A1E4C 00000000 */  nop
/* 0D6980 7F0A1E50 46145182 */  mul.s $f6, $f10, $f20
/* 0D6984 7F0A1E54 E7B00090 */  swc1  $f16, 0x90($sp)
/* 0D6988 7F0A1E58 4600B582 */  mul.s $f22, $f22, $f0
/* 0D698C 7F0A1E5C E7A80094 */  swc1  $f8, 0x94($sp)
/* 0D6990 7F0A1E60 4600E702 */  mul.s $f28, $f28, $f0
/* 0D6994 7F0A1E64 E7A60098 */  swc1  $f6, 0x98($sp)
/* 0D6998 7F0A1E68 C6120000 */  lwc1  $f18, ($s0)
/* 0D699C 7F0A1E6C E7B20084 */  swc1  $f18, 0x84($sp)
/* 0D69A0 7F0A1E70 C6100004 */  lwc1  $f16, 4($s0)
/* 0D69A4 7F0A1E74 C7A80084 */  lwc1  $f8, 0x84($sp)
/* 0D69A8 7F0A1E78 E7B00088 */  swc1  $f16, 0x88($sp)
/* 0D69AC 7F0A1E7C 46084282 */  mul.s $f10, $f8, $f8
/* 0D69B0 7F0A1E80 C6040008 */  lwc1  $f4, 8($s0)
/* 0D69B4 7F0A1E84 C7A60088 */  lwc1  $f6, 0x88($sp)
/* 0D69B8 7F0A1E88 E7A4008C */  swc1  $f4, 0x8c($sp)
/* 0D69BC 7F0A1E8C 46063482 */  mul.s $f18, $f6, $f6
/* 0D69C0 7F0A1E90 C7A4008C */  lwc1  $f4, 0x8c($sp)
/* 0D69C4 7F0A1E94 46042202 */  mul.s $f8, $f4, $f4
/* 0D69C8 7F0A1E98 46125400 */  add.s $f16, $f10, $f18
/* 0D69CC 7F0A1E9C 0C007DF8 */  jal   sqrtf
/* 0D69D0 7F0A1EA0 46104300 */   add.s $f12, $f8, $f16
/* 0D69D4 7F0A1EA4 4600D503 */  div.s $f20, $f26, $f0
/* 0D69D8 7F0A1EA8 C7A60084 */  lwc1  $f6, 0x84($sp)
/* 0D69DC 7F0A1EAC C7B20088 */  lwc1  $f18, 0x88($sp)
/* 0D69E0 7F0A1EB0 C7A8008C */  lwc1  $f8, 0x8c($sp)
/* 0D69E4 7F0A1EB4 C7B800F8 */  lwc1  $f24, 0xf8($sp)
/* 0D69E8 7F0A1EB8 46143282 */  mul.s $f10, $f6, $f20
/* 0D69EC 7F0A1EBC C7A600FC */  lwc1  $f6, 0xfc($sp)
/* 0D69F0 7F0A1EC0 46149102 */  mul.s $f4, $f18, $f20
/* 0D69F4 7F0A1EC4 00000000 */  nop
/* 0D69F8 7F0A1EC8 46144402 */  mul.s $f16, $f8, $f20
/* 0D69FC 7F0A1ECC E7AA0084 */  swc1  $f10, 0x84($sp)
/* 0D6A00 7F0A1ED0 4600C602 */  mul.s $f24, $f24, $f0
/* 0D6A04 7F0A1ED4 E7A40088 */  swc1  $f4, 0x88($sp)
/* 0D6A08 7F0A1ED8 46003282 */  mul.s $f10, $f6, $f0
/* 0D6A0C 7F0A1EDC E7B0008C */  swc1  $f16, 0x8c($sp)
/* 0D6A10 7F0A1EE0 E7AA00FC */  swc1  $f10, 0xfc($sp)
/* 0D6A14 7F0A1EE4 C64E0008 */  lwc1  $f14, 8($s2)
/* 0D6A18 7F0A1EE8 0FC16A8C */  jal   atan2f
/* 0D6A1C 7F0A1EEC C64C0000 */   lwc1  $f12, ($s2)
/* 0D6A20 7F0A1EF0 C7B200FC */  lwc1  $f18, 0xfc($sp)
/* 0D6A24 7F0A1EF4 3C0E8008 */  lui   $t6, %hi(SHATTERED_WINDOW_PIECES_BUFFER_LEN)
/* 0D6A28 7F0A1EF8 8DCEA160 */  lw    $t6, %lo(SHATTERED_WINDOW_PIECES_BUFFER_LEN)($t6)
/* 0D6A2C 7F0A1EFC 4616E501 */  sub.s $f20, $f28, $f22
/* 0D6A30 7F0A1F00 46000786 */  mov.s $f30, $f0
/* 0D6A34 7F0A1F04 46189681 */  sub.s $f26, $f18, $f24
/* 0D6A38 7F0A1F08 461AA102 */  mul.s $f4, $f20, $f26
/* 0D6A3C 7F0A1F0C 05C10003 */  bgez  $t6, .L7F0A1F1C
/* 0D6A40 7F0A1F10 000E7843 */   sra   $t7, $t6, 1
/* 0D6A44 7F0A1F14 25C10001 */  addiu $at, $t6, 1
/* 0D6A48 7F0A1F18 00017843 */  sra   $t7, $at, 1
.L7F0A1F1C:
/* 0D6A4C 7F0A1F1C 448F4000 */  mtc1  $t7, $f8
/* 0D6A50 7F0A1F20 00000000 */  nop
/* 0D6A54 7F0A1F24 46804420 */  cvt.s.w $f16, $f8
/* 0D6A58 7F0A1F28 0C007DF8 */  jal   sqrtf
/* 0D6A5C 7F0A1F2C 46102303 */   div.s $f12, $f4, $f16
/* 0D6A60 7F0A1F30 4600018D */  trunc.w.s $f6, $f0
/* 0D6A64 7F0A1F34 C7A80090 */  lwc1  $f8, 0x90($sp)
/* 0D6A68 7F0A1F38 E7A000B0 */  swc1  $f0, 0xb0($sp)
/* 0D6A6C 7F0A1F3C C6320000 */  lwc1  $f18, ($s1)
/* 0D6A70 7F0A1F40 44023000 */  mfc1  $v0, $f6
/* 0D6A74 7F0A1F44 C7A60084 */  lwc1  $f6, 0x84($sp)
/* 0D6A78 7F0A1F48 3C048006 */  lui   $a0, %hi(g_musicSfxBufferPtr)
/* 0D6A7C 7F0A1F4C 0002C843 */  sra   $t9, $v0, 1
/* 0D6A80 7F0A1F50 44995000 */  mtc1  $t9, $f10
/* 0D6A84 7F0A1F54 8C843720 */  lw    $a0, %lo(g_musicSfxBufferPtr)($a0)
/* 0D6A88 7F0A1F58 24050047 */  li    $a1, 71
/* 0D6A8C 7F0A1F5C 468053A0 */  cvt.s.w $f14, $f10
/* 0D6A90 7F0A1F60 00003025 */  move  $a2, $zero
/* 0D6A94 7F0A1F64 00408025 */  move  $s0, $v0
/* 0D6A98 7F0A1F68 00409025 */  move  $s2, $v0
/* 0D6A9C 7F0A1F6C 460EB300 */  add.s $f12, $f22, $f14
/* 0D6AA0 7F0A1F70 460EC080 */  add.s $f2, $f24, $f14
/* 0D6AA4 7F0A1F74 46086102 */  mul.s $f4, $f12, $f8
/* 0D6AA8 7F0A1F78 46049400 */  add.s $f16, $f18, $f4
/* 0D6AAC 7F0A1F7C 46023282 */  mul.s $f10, $f6, $f2
/* 0D6AB0 7F0A1F80 C7A40094 */  lwc1  $f4, 0x94($sp)
/* 0D6AB4 7F0A1F84 46046182 */  mul.s $f6, $f12, $f4
/* 0D6AB8 7F0A1F88 46105200 */  add.s $f8, $f10, $f16
/* 0D6ABC 7F0A1F8C C7B00088 */  lwc1  $f16, 0x88($sp)
/* 0D6AC0 7F0A1F90 E7A8009C */  swc1  $f8, 0x9c($sp)
/* 0D6AC4 7F0A1F94 46028202 */  mul.s $f8, $f16, $f2
/* 0D6AC8 7F0A1F98 C6320004 */  lwc1  $f18, 4($s1)
/* 0D6ACC 7F0A1F9C 46069280 */  add.s $f10, $f18, $f6
/* 0D6AD0 7F0A1FA0 C7A60098 */  lwc1  $f6, 0x98($sp)
/* 0D6AD4 7F0A1FA4 460A4100 */  add.s $f4, $f8, $f10
/* 0D6AD8 7F0A1FA8 46066402 */  mul.s $f16, $f12, $f6
/* 0D6ADC 7F0A1FAC C7AA008C */  lwc1  $f10, 0x8c($sp)
/* 0D6AE0 7F0A1FB0 E7A400A0 */  swc1  $f4, 0xa0($sp)
/* 0D6AE4 7F0A1FB4 46025102 */  mul.s $f4, $f10, $f2
/* 0D6AE8 7F0A1FB8 C6320008 */  lwc1  $f18, 8($s1)
/* 0D6AEC 7F0A1FBC 46109200 */  add.s $f8, $f18, $f16
/* 0D6AF0 7F0A1FC0 46082180 */  add.s $f6, $f4, $f8
/* 0D6AF4 7F0A1FC4 0C002382 */  jal   sndPlaySfx
/* 0D6AF8 7F0A1FC8 E7A600A4 */   swc1  $f6, 0xa4($sp)
/* 0D6AFC 7F0A1FCC 00402025 */  move  $a0, $v0
/* 0D6B00 7F0A1FD0 0FC14E84 */  jal   chrobjSndCreatePostEventDefault
/* 0D6B04 7F0A1FD4 02202825 */   move  $a1, $s1
/* 0D6B08 7F0A1FD8 44922000 */  mtc1  $s2, $f4
/* 0D6B0C 7F0A1FDC 44909000 */  mtc1  $s0, $f18
/* 0D6B10 7F0A1FE0 00009825 */  move  $s3, $zero
/* 0D6B14 7F0A1FE4 46802220 */  cvt.s.w $f8, $f4
/* 0D6B18 7F0A1FE8 3C018005 */  lui   $at, %hi(D_80057710)
/* 0D6B1C 7F0A1FEC 468095A0 */  cvt.s.w $f22, $f18
/* 0D6B20 7F0A1FF0 E7A80068 */  swc1  $f8, 0x68($sp)
/* 0D6B24 7F0A1FF4 C7A60068 */  lwc1  $f6, 0x68($sp)
/* 0D6B28 7F0A1FF8 4606D483 */  div.s $f18, $f26, $f6
/* 0D6B2C 7F0A1FFC 4616A403 */  div.s $f16, $f20, $f22
/* 0D6B30 7F0A2000 4600828D */  trunc.w.s $f10, $f16
/* 0D6B34 7F0A2004 4600940D */  trunc.w.s $f16, $f18
/* 0D6B38 7F0A2008 44145000 */  mfc1  $s4, $f10
/* 0D6B3C 7F0A200C 44158000 */  mfc1  $s5, $f16
/* 0D6B40 7F0A2010 02808825 */  move  $s1, $s4
/* 0D6B44 7F0A2014 5AA00044 */  blezl $s5, .L7F0A2128
/* 0D6B48 7F0A2018 8FBF005C */   lw    $ra, 0x5c($sp)
/* 0D6B4C 7F0A201C C43C7710 */  lwc1  $f28, %lo(D_80057710)($at)
/* 0D6B50 7F0A2020 3C018005 */  lui   $at, %hi(D_80057714)
/* 0D6B54 7F0A2024 C43A7714 */  lwc1  $f26, %lo(D_80057714)($at)
/* 0D6B58 7F0A2028 3C012F80 */  li    $at, 0x2F800000 # 0.000000
/* 0D6B5C 7F0A202C 4481C000 */  mtc1  $at, $f24
/* 0D6B60 7F0A2030 27B200C0 */  addiu $s2, $sp, 0xc0
.L7F0A2034:
/* 0D6B64 7F0A2034 1A800037 */  blez  $s4, .L7F0A2114
/* 0D6B68 7F0A2038 00008025 */   move  $s0, $zero
/* 0D6B6C 7F0A203C 44935000 */  mtc1  $s3, $f10
/* 0D6B70 7F0A2040 C7A80068 */  lwc1  $f8, 0x68($sp)
/* 0D6B74 7F0A2044 46805120 */  cvt.s.w $f4, $f10
/* 0D6B78 7F0A2048 46082502 */  mul.s $f20, $f4, $f8
/* 0D6B7C 7F0A204C 00000000 */  nop
/* 0D6B80 7F0A2050 44903000 */  mtc1  $s0, $f6
.L7F0A2054:
/* 0D6B84 7F0A2054 C7AA0090 */  lwc1  $f10, 0x90($sp)
/* 0D6B88 7F0A2058 C7B0009C */  lwc1  $f16, 0x9c($sp)
/* 0D6B8C 7F0A205C 468034A0 */  cvt.s.w $f18, $f6
/* 0D6B90 7F0A2060 C7A60084 */  lwc1  $f6, 0x84($sp)
/* 0D6B94 7F0A2064 46169002 */  mul.s $f0, $f18, $f22
/* 0D6B98 7F0A2068 00000000 */  nop
/* 0D6B9C 7F0A206C 460A0102 */  mul.s $f4, $f0, $f10
/* 0D6BA0 7F0A2070 46048200 */  add.s $f8, $f16, $f4
/* 0D6BA4 7F0A2074 46143482 */  mul.s $f18, $f6, $f20
/* 0D6BA8 7F0A2078 C7A40094 */  lwc1  $f4, 0x94($sp)
/* 0D6BAC 7F0A207C C7B000A0 */  lwc1  $f16, 0xa0($sp)
/* 0D6BB0 7F0A2080 46040182 */  mul.s $f6, $f0, $f4
/* 0D6BB4 7F0A2084 46089280 */  add.s $f10, $f18, $f8
/* 0D6BB8 7F0A2088 C7A80088 */  lwc1  $f8, 0x88($sp)
/* 0D6BBC 7F0A208C 46068480 */  add.s $f18, $f16, $f6
/* 0D6BC0 7F0A2090 E7AA00C0 */  swc1  $f10, 0xc0($sp)
/* 0D6BC4 7F0A2094 46144282 */  mul.s $f10, $f8, $f20
/* 0D6BC8 7F0A2098 C7A60098 */  lwc1  $f6, 0x98($sp)
/* 0D6BCC 7F0A209C C7B000A4 */  lwc1  $f16, 0xa4($sp)
/* 0D6BD0 7F0A20A0 46060202 */  mul.s $f8, $f0, $f6
/* 0D6BD4 7F0A20A4 46125100 */  add.s $f4, $f10, $f18
/* 0D6BD8 7F0A20A8 C7B2008C */  lwc1  $f18, 0x8c($sp)
/* 0D6BDC 7F0A20AC 46088280 */  add.s $f10, $f16, $f8
/* 0D6BE0 7F0A20B0 E7A400C4 */  swc1  $f4, 0xc4($sp)
/* 0D6BE4 7F0A20B4 46149102 */  mul.s $f4, $f18, $f20
/* 0D6BE8 7F0A20B8 460A2180 */  add.s $f6, $f4, $f10
/* 0D6BEC 7F0A20BC 0C002914 */  jal   randomGetNext
/* 0D6BF0 7F0A20C0 E7A600C8 */   swc1  $f6, 0xc8($sp)
/* 0D6BF4 7F0A20C4 44828000 */  mtc1  $v0, $f16
/* 0D6BF8 7F0A20C8 4405F000 */  mfc1  $a1, $f30
/* 0D6BFC 7F0A20CC 02402025 */  move  $a0, $s2
/* 0D6C00 7F0A20D0 04410005 */  bgez  $v0, .L7F0A20E8
/* 0D6C04 7F0A20D4 46808220 */   cvt.s.w $f8, $f16
/* 0D6C08 7F0A20D8 3C014F80 */  li    $at, 0x4F800000 # 4294967296.000000
/* 0D6C0C 7F0A20DC 44819000 */  mtc1  $at, $f18
/* 0D6C10 7F0A20E0 00000000 */  nop
/* 0D6C14 7F0A20E4 46124200 */  add.s $f8, $f8, $f18
.L7F0A20E8:
/* 0D6C18 7F0A20E8 46184102 */  mul.s $f4, $f8, $f24
/* 0D6C1C 7F0A20EC C7B000B0 */  lwc1  $f16, 0xb0($sp)
/* 0D6C20 7F0A20F0 461A2282 */  mul.s $f10, $f4, $f26
/* 0D6C24 7F0A20F4 461C5180 */  add.s $f6, $f10, $f28
/* 0D6C28 7F0A20F8 46103482 */  mul.s $f18, $f6, $f16
/* 0D6C2C 7F0A20FC 44069000 */  mfc1  $a2, $f18
/* 0D6C30 7F0A2100 0FC28858 */  jal   glassCreateShard
/* 0D6C34 7F0A2104 00000000 */   nop
/* 0D6C38 7F0A2108 26100001 */  addiu $s0, $s0, 1
/* 0D6C3C 7F0A210C 5611FFD1 */  bnel  $s0, $s1, .L7F0A2054
/* 0D6C40 7F0A2110 44903000 */   mtc1  $s0, $f6
.L7F0A2114:
/* 0D6C44 7F0A2114 26730001 */  addiu $s3, $s3, 1
/* 0D6C48 7F0A2118 0275082A */  slt   $at, $s3, $s5
/* 0D6C4C 7F0A211C 1420FFC5 */  bnez  $at, .L7F0A2034
/* 0D6C50 7F0A2120 00000000 */   nop
/* 0D6C54 7F0A2124 8FBF005C */  lw    $ra, 0x5c($sp)
.L7F0A2128:
/* 0D6C58 7F0A2128 D7B40010 */  ldc1  $f20, 0x10($sp)
/* 0D6C5C 7F0A212C D7B60018 */  ldc1  $f22, 0x18($sp)
/* 0D6C60 7F0A2130 D7B80020 */  ldc1  $f24, 0x20($sp)
/* 0D6C64 7F0A2134 D7BA0028 */  ldc1  $f26, 0x28($sp)
/* 0D6C68 7F0A2138 D7BC0030 */  ldc1  $f28, 0x30($sp)
/* 0D6C6C 7F0A213C D7BE0038 */  ldc1  $f30, 0x38($sp)
/* 0D6C70 7F0A2140 8FB00044 */  lw    $s0, 0x44($sp)
/* 0D6C74 7F0A2144 8FB10048 */  lw    $s1, 0x48($sp)
/* 0D6C78 7F0A2148 8FB2004C */  lw    $s2, 0x4c($sp)
/* 0D6C7C 7F0A214C 8FB30050 */  lw    $s3, 0x50($sp)
/* 0D6C80 7F0A2150 8FB40054 */  lw    $s4, 0x54($sp)
/* 0D6C84 7F0A2154 8FB50058 */  lw    $s5, 0x58($sp)
/* 0D6C88 7F0A2158 03E00008 */  jr    $ra
/* 0D6C8C 7F0A215C 27BD00E0 */   addiu $sp, $sp, 0xe0
)
#endif

/**
 * Creates a triangular shard of glass.
 */
void glassCreateShard(coord3d* pos, f32 rotX, f32 shard_size)
{
    /**
     * Horizontal X velocity component with randomness. Range: [-1.0, +1.0]
     */
    f32 randSymmetricX = (2.0f * (randomGetNext() * (1.0f / (f32)UINT_MAX))) - 1.0f;

    /**
     * Vertical velocity component. Range: [ -0.12, +1.0]
     * Biased so that most shards get an upward push but some have a chance of getting a small downward push.
     */
    f32 randBiasedY = (randomGetNext() * (1.0f / (f32)UINT_MAX) * 1.12f) - .12f;
    
    /**
     * Horizontal Z velocity component with randomness. Range: [-1.0, +1.0]
     */
    f32 randSymmetricZ = (2.0f * (randomGetNext() * (1.0f / (f32)UINT_MAX))) - 1.0f;

    u8 alpha; // Shard opacity

    ptr_shattered_window_pieces[g_NextShardNum].active = 1;
    ptr_shattered_window_pieces[g_NextShardNum].pos.x = pos->x;
    ptr_shattered_window_pieces[g_NextShardNum].pos.y = pos->y;
    ptr_shattered_window_pieces[g_NextShardNum].pos.z = pos->z;

    ptr_shattered_window_pieces[g_NextShardNum].velocity.x= randSymmetricX * SHARD_HORIZ_VEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].velocity.y = randBiasedY * SHARD_VERT_VEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].velocity.z = randSymmetricZ * SHARD_HORIZ_VEL_SCALE;

    ptr_shattered_window_pieces[g_NextShardNum].v1x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v1y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v1z = 0;

    ptr_shattered_window_pieces[g_NextShardNum].v2x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v2y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v2z = 0;

    ptr_shattered_window_pieces[g_NextShardNum].v3x = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v3y = ((randomGetNext() * (1.0f / (f32)UINT_MAX) * 0.5f) + 1.0f) * -shard_size;
    ptr_shattered_window_pieces[g_NextShardNum].v3z = 0;
    
    ptr_shattered_window_pieces[g_NextShardNum].v1s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v1t = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v2s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v2t = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v3s = 0;
    ptr_shattered_window_pieces[g_NextShardNum].v3t = 0;

    /**
     * Create a colored gradient over the verts for environment mapping.
     */
    ptr_shattered_window_pieces[g_NextShardNum].v1r = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v1g = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v1b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v2r = 5;
    ptr_shattered_window_pieces[g_NextShardNum].v2g = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v2b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v3r = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v3g = 0xFB;
    ptr_shattered_window_pieces[g_NextShardNum].v3b = 0x7E;
    ptr_shattered_window_pieces[g_NextShardNum].v3a = 0xFF;

    alpha = ptr_shattered_window_pieces[g_NextShardNum].v3a;
    ptr_shattered_window_pieces[g_NextShardNum].v2a = alpha;
    ptr_shattered_window_pieces[g_NextShardNum].v1a = alpha;

    ptr_shattered_window_pieces[g_NextShardNum].rot.x = rotX;
    ptr_shattered_window_pieces[g_NextShardNum].rot.y = 0.0f;
    ptr_shattered_window_pieces[g_NextShardNum].rot.z = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * M_TAU_F;

    /**
     * Impart a random angular velocity on each shard piece.
     */
    ptr_shattered_window_pieces[g_NextShardNum].angvel.x = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].angvel.y = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;
    ptr_shattered_window_pieces[g_NextShardNum].angvel.z = (randomGetNext() * (1.0f / (f32)UINT_MAX)) * SHARD_ANGVEL_SCALE;

    g_NextShardNum++;
    if (g_NextShardNum >= SHATTERED_WINDOW_PIECES_BUFFER_LEN) {
        g_NextShardNum = 0;
    }
}


void update_broken_windows(void) {
    f32 var_f0;
    s32 i;
    s32 j;

    if (g_ClockTimer < 15) {
        var_f0 = (f32)g_ClockTimer;
    } else {
        var_f0 = 15.0f;
    }
    if (SHATTERED_WINDOW_PIECES_BUFFER_LEN > 0) {
        i = 0;
        do {
            if (ptr_shattered_window_pieces[i].active > 0) {
                ptr_shattered_window_pieces[i].active += (s32)var_f0;
                j = 0;
                ptr_shattered_window_pieces[i].rot.x += ptr_shattered_window_pieces[i].angvel.x * var_f0;
                ptr_shattered_window_pieces[i].rot.y += ptr_shattered_window_pieces[i].angvel.y * var_f0;
                ptr_shattered_window_pieces[i].rot.z += ptr_shattered_window_pieces[i].angvel.z * var_f0;
                ptr_shattered_window_pieces[i].pos.x += ptr_shattered_window_pieces[i].velocity.x * var_f0;
                ptr_shattered_window_pieces[i].pos.z += ptr_shattered_window_pieces[i].velocity.z * var_f0;
                if ((s32)var_f0 > 0) {
                    do {
                        ptr_shattered_window_pieces[i].pos.y += ptr_shattered_window_pieces[i].velocity.y;
                        ptr_shattered_window_pieces[i].velocity.y -= 0.1f;
                        j++;
                    } while (j < (s32)var_f0);
                }
                if (ptr_shattered_window_pieces[i].active >= 0x96) {
                    ptr_shattered_window_pieces[i].active = 0;
                }
                if (ptr_shattered_window_pieces[i].pos.y < -30000.0f ||
                    ptr_shattered_window_pieces[i].pos.y > 30000.0f) {
                    ptr_shattered_window_pieces[i].active = 0;
                }
            }
            i++;
        } while (i < SHATTERED_WINDOW_PIECES_BUFFER_LEN);
    }
}






#ifdef NONMATCHING
Gfx * sub_GAME_7F0A2C44(Gfx *arg0) {

}
#else
#if defined(LEFTOVERDEBUG)
GLOBAL_ASM(
.text
glabel sub_GAME_7F0A2C44
/* 0D7774 7F0A2C44 27BDFF30 */  addiu $sp, $sp, -0xd0
/* 0D7778 7F0A2C48 3C058009 */  lui   $a1, %hi(glassoverlayimage)
/* 0D777C 7F0A2C4C 8CA5D11C */  lw    $a1, %lo(glassoverlayimage)($a1)
/* 0D7780 7F0A2C50 AFBF0044 */  sw    $ra, 0x44($sp)
/* 0D7784 7F0A2C54 AFA400D0 */  sw    $a0, 0xd0($sp)
/* 0D7788 7F0A2C58 240E0002 */  li    $t6, 2
/* 0D778C 7F0A2C5C AFBE0040 */  sw    $fp, 0x40($sp)
/* 0D7790 7F0A2C60 AFB7003C */  sw    $s7, 0x3c($sp)
/* 0D7794 7F0A2C64 AFB60038 */  sw    $s6, 0x38($sp)
/* 0D7798 7F0A2C68 AFB50034 */  sw    $s5, 0x34($sp)
/* 0D779C 7F0A2C6C AFB40030 */  sw    $s4, 0x30($sp)
/* 0D77A0 7F0A2C70 AFB3002C */  sw    $s3, 0x2c($sp)
/* 0D77A4 7F0A2C74 AFB20028 */  sw    $s2, 0x28($sp)
/* 0D77A8 7F0A2C78 AFB10024 */  sw    $s1, 0x24($sp)
/* 0D77AC 7F0A2C7C AFB00020 */  sw    $s0, 0x20($sp)
/* 0D77B0 7F0A2C80 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0D77B4 7F0A2C84 27A400D0 */  addiu $a0, $sp, 0xd0
/* 0D77B8 7F0A2C88 24060002 */  li    $a2, 2
/* 0D77BC 7F0A2C8C 24070001 */  li    $a3, 1
/* 0D77C0 7F0A2C90 0FC1DB5A */  jal   texSelect
/* 0D77C4 7F0A2C94 24A5000C */   addiu $a1, $a1, 0xc
/* 0D77C8 7F0A2C98 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D77CC 7F0A2C9C 3C19BB00 */  lui   $t9, (0xBB001001 >> 16) # lui $t9, 0xbb00
/* 0D77D0 7F0A2CA0 3C080D80 */  lui   $t0, (0x0D800D80 >> 16) # lui $t0, 0xd80
/* 0D77D4 7F0A2CA4 25F80008 */  addiu $t8, $t7, 8
/* 0D77D8 7F0A2CA8 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D77DC 7F0A2CAC 35080D80 */  ori   $t0, (0x0D800D80 & 0xFFFF) # ori $t0, $t0, 0xd80
/* 0D77E0 7F0A2CB0 37391001 */  ori   $t9, (0xBB001001 & 0xFFFF) # ori $t9, $t9, 0x1001
/* 0D77E4 7F0A2CB4 ADF90000 */  sw    $t9, ($t7)
/* 0D77E8 7F0A2CB8 ADE80004 */  sw    $t0, 4($t7)
/* 0D77EC 7F0A2CBC 8FA900D0 */  lw    $t1, 0xd0($sp)
/* 0D77F0 7F0A2CC0 3C0BBA00 */  lui   $t3, (0xBA001402 >> 16) # lui $t3, 0xba00
/* 0D77F4 7F0A2CC4 356B1402 */  ori   $t3, (0xBA001402 & 0xFFFF) # ori $t3, $t3, 0x1402
/* 0D77F8 7F0A2CC8 252A0008 */  addiu $t2, $t1, 8
/* 0D77FC 7F0A2CCC AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7800 7F0A2CD0 3C0C0010 */  lui   $t4, 0x10
/* 0D7804 7F0A2CD4 AD2C0004 */  sw    $t4, 4($t1)
/* 0D7808 7F0A2CD8 AD2B0000 */  sw    $t3, ($t1)
/* 0D780C 7F0A2CDC 8FAD00D0 */  lw    $t5, 0xd0($sp)
/* 0D7810 7F0A2CE0 3C0FBA00 */  lui   $t7, (0xBA001001 >> 16) # lui $t7, 0xba00
/* 0D7814 7F0A2CE4 35EF1001 */  ori   $t7, (0xBA001001 & 0xFFFF) # ori $t7, $t7, 0x1001
/* 0D7818 7F0A2CE8 25AE0008 */  addiu $t6, $t5, 8
/* 0D781C 7F0A2CEC AFAE00D0 */  sw    $t6, 0xd0($sp)
/* 0D7820 7F0A2CF0 3C180001 */  lui   $t8, 1
/* 0D7824 7F0A2CF4 ADB80004 */  sw    $t8, 4($t5)
/* 0D7828 7F0A2CF8 ADAF0000 */  sw    $t7, ($t5)
/* 0D782C 7F0A2CFC 8FB900D0 */  lw    $t9, 0xd0($sp)
/* 0D7830 7F0A2D00 3C09B600 */  lui   $t1, 0xb600
/* 0D7834 7F0A2D04 240A3000 */  li    $t2, 12288
/* 0D7838 7F0A2D08 27280008 */  addiu $t0, $t9, 8
/* 0D783C 7F0A2D0C AFA800D0 */  sw    $t0, 0xd0($sp)
/* 0D7840 7F0A2D10 AF2A0004 */  sw    $t2, 4($t9)
/* 0D7844 7F0A2D14 AF290000 */  sw    $t1, ($t9)
/* 0D7848 7F0A2D18 8FAB00D0 */  lw    $t3, 0xd0($sp)
/* 0D784C 7F0A2D1C 3C0DBA00 */  lui   $t5, (0xBA000C02 >> 16) # lui $t5, 0xba00
/* 0D7850 7F0A2D20 35AD0C02 */  ori   $t5, (0xBA000C02 & 0xFFFF) # ori $t5, $t5, 0xc02
/* 0D7854 7F0A2D24 256C0008 */  addiu $t4, $t3, 8
/* 0D7858 7F0A2D28 AFAC00D0 */  sw    $t4, 0xd0($sp)
/* 0D785C 7F0A2D2C 240E2000 */  li    $t6, 8192
/* 0D7860 7F0A2D30 AD6E0004 */  sw    $t6, 4($t3)
/* 0D7864 7F0A2D34 AD6D0000 */  sw    $t5, ($t3)
/* 0D7868 7F0A2D38 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D786C 7F0A2D3C 3C19B700 */  lui   $t9, 0xb700
/* 0D7870 7F0A2D40 3C080006 */  lui   $t0, 6
/* 0D7874 7F0A2D44 25F80008 */  addiu $t8, $t7, 8
/* 0D7878 7F0A2D48 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D787C 7F0A2D4C ADE80004 */  sw    $t0, 4($t7)
/* 0D7880 7F0A2D50 ADF90000 */  sw    $t9, ($t7)
/* 0D7884 7F0A2D54 8FB000D0 */  lw    $s0, 0xd0($sp)
/* 0D7888 7F0A2D58 3C0B0103 */  lui   $t3, (0x01030040 >> 16) # lui $t3, 0x103
/* 0D788C 7F0A2D5C 356B0040 */  ori   $t3, (0x01030040 & 0xFFFF) # ori $t3, $t3, 0x40
/* 0D7890 7F0A2D60 260A0008 */  addiu $t2, $s0, 8
/* 0D7894 7F0A2D64 AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7898 7F0A2D68 0FC1E0E5 */  jal   get_BONDdata_field_10E0
/* 0D789C 7F0A2D6C AE0B0000 */   sw    $t3, ($s0)
/* 0D78A0 7F0A2D70 0C003A2C */  jal   osVirtualToPhysical
/* 0D78A4 7F0A2D74 00402025 */   move  $a0, $v0
/* 0D78A8 7F0A2D78 AE020004 */  sw    $v0, 4($s0)
/* 0D78AC 7F0A2D7C 3C048008 */  lui   $a0, %hi(SHATTERED_WINDOW_PIECES_BUFFER_LEN)
/* 0D78B0 7F0A2D80 8C84A160 */  lw    $a0, %lo(SHATTERED_WINDOW_PIECES_BUFFER_LEN)($a0)
/* 0D78B4 7F0A2D84 00008025 */  move  $s0, $zero
/* 0D78B8 7F0A2D88 0000A025 */  move  $s4, $zero
/* 0D78BC 7F0A2D8C 18800044 */  blez  $a0, .L7F0A2EA0
/* 0D78C0 7F0A2D90 3C170420 */   lui   $s7, (0x04200030 >> 16) # lui $s7, 0x420
/* 0D78C4 7F0A2D94 3C168008 */  lui   $s6, %hi(g_CurrentPlayer)
/* 0D78C8 7F0A2D98 3C138008 */  lui   $s3, %hi(ptr_shattered_window_pieces)
/* 0D78CC 7F0A2D9C 2673A164 */  addiu $s3, %lo(ptr_shattered_window_pieces) # addiu $s3, $s3, -0x5e9c
/* 0D78D0 7F0A2DA0 26D6A0B0 */  addiu $s6, %lo(g_CurrentPlayer) # addiu $s6, $s6, -0x5f50
/* 0D78D4 7F0A2DA4 3C1EBF00 */  lui   $fp, 0xbf00
/* 0D78D8 7F0A2DA8 36F70030 */  ori   $s7, (0x04200030 & 0xFFFF) # ori $s7, $s7, 0x30
/* 0D78DC 7F0A2DAC 27B50090 */  addiu $s5, $sp, 0x90
.L7F0A2DB0:
/* 0D78E0 7F0A2DB0 8E6C0000 */  lw    $t4, ($s3)
/* 0D78E4 7F0A2DB4 01906821 */  addu  $t5, $t4, $s0
/* 0D78E8 7F0A2DB8 8DAE0000 */  lw    $t6, ($t5)
/* 0D78EC 7F0A2DBC 59C00035 */  blezl $t6, .L7F0A2E94
/* 0D78F0 7F0A2DC0 26940001 */   addiu $s4, $s4, 1
/* 0D78F4 7F0A2DC4 0FC2F5B8 */  jal   dynAllocateMatrix
/* 0D78F8 7F0A2DC8 00000000 */   nop
/* 0D78FC 7F0A2DCC 8E6F0000 */  lw    $t7, ($s3)
/* 0D7900 7F0A2DD0 00408825 */  move  $s1, $v0
/* 0D7904 7F0A2DD4 02A03025 */  move  $a2, $s5
/* 0D7908 7F0A2DD8 01F01821 */  addu  $v1, $t7, $s0
/* 0D790C 7F0A2DDC 24640004 */  addiu $a0, $v1, 4
/* 0D7910 7F0A2DE0 0FC1624B */  jal   matrix_4x4_set_position_and_rotation_around_xyz
/* 0D7914 7F0A2DE4 24650010 */   addiu $a1, $v1, 0x10
/* 0D7918 7F0A2DE8 8EC20000 */  lw    $v0, ($s6)
/* 0D791C 7F0A2DEC C7A400C0 */  lwc1  $f4, 0xc0($sp)
/* 0D7920 7F0A2DF0 C7AA00C4 */  lwc1  $f10, 0xc4($sp)
/* 0D7924 7F0A2DF4 C4460038 */  lwc1  $f6, 0x38($v0)
/* 0D7928 7F0A2DF8 02A02025 */  move  $a0, $s5
/* 0D792C 7F0A2DFC 02202825 */  move  $a1, $s1
/* 0D7930 7F0A2E00 46062201 */  sub.s $f8, $f4, $f6
/* 0D7934 7F0A2E04 C7A400C8 */  lwc1  $f4, 0xc8($sp)
/* 0D7938 7F0A2E08 E7A800C0 */  swc1  $f8, 0xc0($sp)
/* 0D793C 7F0A2E0C C450003C */  lwc1  $f16, 0x3c($v0)
/* 0D7940 7F0A2E10 46105481 */  sub.s $f18, $f10, $f16
/* 0D7944 7F0A2E14 E7B200C4 */  swc1  $f18, 0xc4($sp)
/* 0D7948 7F0A2E18 C4460040 */  lwc1  $f6, 0x40($v0)
/* 0D794C 7F0A2E1C 46062201 */  sub.s $f8, $f4, $f6
/* 0D7950 7F0A2E20 0FC16327 */  jal   matrix_4x4_f32_to_s32
/* 0D7954 7F0A2E24 E7A800C8 */   swc1  $f8, 0xc8($sp)
/* 0D7958 7F0A2E28 8FB200D0 */  lw    $s2, 0xd0($sp)
/* 0D795C 7F0A2E2C 3C080102 */  lui   $t0, (0x01020040 >> 16) # lui $t0, 0x102
/* 0D7960 7F0A2E30 35080040 */  ori   $t0, (0x01020040 & 0xFFFF) # ori $t0, $t0, 0x40
/* 0D7964 7F0A2E34 26590008 */  addiu $t9, $s2, 8
/* 0D7968 7F0A2E38 AFB900D0 */  sw    $t9, 0xd0($sp)
/* 0D796C 7F0A2E3C 02202025 */  move  $a0, $s1
/* 0D7970 7F0A2E40 0C003A2C */  jal   osVirtualToPhysical
/* 0D7974 7F0A2E44 AE480000 */   sw    $t0, ($s2)
/* 0D7978 7F0A2E48 AE420004 */  sw    $v0, 4($s2)
/* 0D797C 7F0A2E4C 8FB100D0 */  lw    $s1, 0xd0($sp)
/* 0D7980 7F0A2E50 262A0008 */  addiu $t2, $s1, 8
/* 0D7984 7F0A2E54 AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7988 7F0A2E58 AE370000 */  sw    $s7, ($s1)
/* 0D798C 7F0A2E5C 8E6B0000 */  lw    $t3, ($s3)
/* 0D7990 7F0A2E60 01702021 */  addu  $a0, $t3, $s0
/* 0D7994 7F0A2E64 0C003A2C */  jal   osVirtualToPhysical
/* 0D7998 7F0A2E68 24840038 */   addiu $a0, $a0, 0x38
/* 0D799C 7F0A2E6C AE220004 */  sw    $v0, 4($s1)
/* 0D79A0 7F0A2E70 8FAC00D0 */  lw    $t4, 0xd0($sp)
/* 0D79A4 7F0A2E74 240E0A14 */  li    $t6, 2580
/* 0D79A8 7F0A2E78 3C048008 */  lui   $a0, %hi(SHATTERED_WINDOW_PIECES_BUFFER_LEN)
/* 0D79AC 7F0A2E7C 258D0008 */  addiu $t5, $t4, 8
/* 0D79B0 7F0A2E80 AFAD00D0 */  sw    $t5, 0xd0($sp)
/* 0D79B4 7F0A2E84 AD8E0004 */  sw    $t6, 4($t4)
/* 0D79B8 7F0A2E88 AD9E0000 */  sw    $fp, ($t4)
/* 0D79BC 7F0A2E8C 8C84A160 */  lw    $a0, %lo(SHATTERED_WINDOW_PIECES_BUFFER_LEN)($a0)
/* 0D79C0 7F0A2E90 26940001 */  addiu $s4, $s4, 1
.L7F0A2E94:
/* 0D79C4 7F0A2E94 0284082A */  slt   $at, $s4, $a0
/* 0D79C8 7F0A2E98 1420FFC5 */  bnez  $at, .L7F0A2DB0
/* 0D79CC 7F0A2E9C 26100068 */   addiu $s0, $s0, 0x68
.L7F0A2EA0:
/* 0D79D0 7F0A2EA0 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D79D4 7F0A2EA4 3C19B600 */  lui   $t9, 0xb600
/* 0D79D8 7F0A2EA8 3C080006 */  lui   $t0, 6
/* 0D79DC 7F0A2EAC 25F80008 */  addiu $t8, $t7, 8
/* 0D79E0 7F0A2EB0 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D79E4 7F0A2EB4 ADE80004 */  sw    $t0, 4($t7)
/* 0D79E8 7F0A2EB8 ADF90000 */  sw    $t9, ($t7)
/* 0D79EC 7F0A2EBC 8FB000D0 */  lw    $s0, 0xd0($sp)
/* 0D79F0 7F0A2EC0 3C0B0103 */  lui   $t3, (0x01030040 >> 16) # lui $t3, 0x103
/* 0D79F4 7F0A2EC4 356B0040 */  ori   $t3, (0x01030040 & 0xFFFF) # ori $t3, $t3, 0x40
/* 0D79F8 7F0A2EC8 260A0008 */  addiu $t2, $s0, 8
/* 0D79FC 7F0A2ECC AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7A00 7F0A2ED0 0FC1E0DD */  jal   currentPlayerGetProjectionMatrix
/* 0D7A04 7F0A2ED4 AE0B0000 */   sw    $t3, ($s0)
/* 0D7A08 7F0A2ED8 AE020004 */  sw    $v0, 4($s0)
/* 0D7A0C 7F0A2EDC 8FB100D0 */  lw    $s1, 0xd0($sp)
/* 0D7A10 7F0A2EE0 3C0E0102 */  lui   $t6, (0x01020040 >> 16) # lui $t6, 0x102
/* 0D7A14 7F0A2EE4 35CE0040 */  ori   $t6, (0x01020040 & 0xFFFF) # ori $t6, $t6, 0x40
/* 0D7A18 7F0A2EE8 262D0008 */  addiu $t5, $s1, 8
/* 0D7A1C 7F0A2EEC AFAD00D0 */  sw    $t5, 0xd0($sp)
/* 0D7A20 7F0A2EF0 0FC1E0D5 */  jal   currentPlayerGetMatrix10C8
/* 0D7A24 7F0A2EF4 AE2E0000 */   sw    $t6, ($s1)
/* 0D7A28 7F0A2EF8 AE220004 */  sw    $v0, 4($s1)
/* 0D7A2C 7F0A2EFC 8FBF0044 */  lw    $ra, 0x44($sp)
/* 0D7A30 7F0A2F00 8FBE0040 */  lw    $fp, 0x40($sp)
/* 0D7A34 7F0A2F04 8FB7003C */  lw    $s7, 0x3c($sp)
/* 0D7A38 7F0A2F08 8FB60038 */  lw    $s6, 0x38($sp)
/* 0D7A3C 7F0A2F0C 8FB50034 */  lw    $s5, 0x34($sp)
/* 0D7A40 7F0A2F10 8FB40030 */  lw    $s4, 0x30($sp)
/* 0D7A44 7F0A2F14 8FB3002C */  lw    $s3, 0x2c($sp)
/* 0D7A48 7F0A2F18 8FB20028 */  lw    $s2, 0x28($sp)
/* 0D7A4C 7F0A2F1C 8FB10024 */  lw    $s1, 0x24($sp)
/* 0D7A50 7F0A2F20 8FB00020 */  lw    $s0, 0x20($sp)
/* 0D7A54 7F0A2F24 8FA200D0 */  lw    $v0, 0xd0($sp)
/* 0D7A58 7F0A2F28 03E00008 */  jr    $ra
/* 0D7A5C 7F0A2F2C 27BD00D0 */   addiu $sp, $sp, 0xd0
)
#else /* !LEFTOVERDEBUG */
GLOBAL_ASM(
.text
glabel sub_GAME_7F0A2C44
/* 0D7774 7F0A2C44 27BDFF30 */  addiu $sp, $sp, -0xd0
/* 0D7778 7F0A2C48 3C058009 */  lui   $a1, %hi(glassoverlayimage)
/* 0D777C 7F0A2C4C 8CA5D11C */  lw    $a1, %lo(glassoverlayimage)($a1)
/* 0D7780 7F0A2C50 AFBF0044 */  sw    $ra, 0x44($sp)
/* 0D7784 7F0A2C54 AFA400D0 */  sw    $a0, 0xd0($sp)
/* 0D7788 7F0A2C58 240E0002 */  li    $t6, 2
/* 0D778C 7F0A2C5C AFBE0040 */  sw    $fp, 0x40($sp)
/* 0D7790 7F0A2C60 AFB7003C */  sw    $s7, 0x3c($sp)
/* 0D7794 7F0A2C64 AFB60038 */  sw    $s6, 0x38($sp)
/* 0D7798 7F0A2C68 AFB50034 */  sw    $s5, 0x34($sp)
/* 0D779C 7F0A2C6C AFB40030 */  sw    $s4, 0x30($sp)
/* 0D77A0 7F0A2C70 AFB3002C */  sw    $s3, 0x2c($sp)
/* 0D77A4 7F0A2C74 AFB20028 */  sw    $s2, 0x28($sp)
/* 0D77A8 7F0A2C78 AFB10024 */  sw    $s1, 0x24($sp)
/* 0D77AC 7F0A2C7C AFB00020 */  sw    $s0, 0x20($sp)
/* 0D77B0 7F0A2C80 AFAE0010 */  sw    $t6, 0x10($sp)
/* 0D77B4 7F0A2C84 27A400D0 */  addiu $a0, $sp, 0xd0
/* 0D77B8 7F0A2C88 24060002 */  li    $a2, 2
/* 0D77BC 7F0A2C8C 24070001 */  li    $a3, 1
/* 0D77C0 7F0A2C90 0FC1DB5A */  jal   texSelect
/* 0D77C4 7F0A2C94 24A5000C */   addiu $a1, $a1, 0xc
/* 0D77C8 7F0A2C98 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D77CC 7F0A2C9C 3C19BB00 */  lui   $t9, (0xBB001001 >> 16) # lui $t9, 0xbb00
/* 0D77D0 7F0A2CA0 3C080D80 */  lui   $t0, (0x0D800D80 >> 16) # lui $t0, 0xd80
/* 0D77D4 7F0A2CA4 25F80008 */  addiu $t8, $t7, 8
/* 0D77D8 7F0A2CA8 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D77DC 7F0A2CAC 35080D80 */  ori   $t0, (0x0D800D80 & 0xFFFF) # ori $t0, $t0, 0xd80
/* 0D77E0 7F0A2CB0 37391001 */  ori   $t9, (0xBB001001 & 0xFFFF) # ori $t9, $t9, 0x1001
/* 0D77E4 7F0A2CB4 ADF90000 */  sw    $t9, ($t7)
/* 0D77E8 7F0A2CB8 ADE80004 */  sw    $t0, 4($t7)
/* 0D77EC 7F0A2CBC 8FA900D0 */  lw    $t1, 0xd0($sp)
/* 0D77F0 7F0A2CC0 3C0BBA00 */  lui   $t3, (0xBA001402 >> 16) # lui $t3, 0xba00
/* 0D77F4 7F0A2CC4 356B1402 */  ori   $t3, (0xBA001402 & 0xFFFF) # ori $t3, $t3, 0x1402
/* 0D77F8 7F0A2CC8 252A0008 */  addiu $t2, $t1, 8
/* 0D77FC 7F0A2CCC AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7800 7F0A2CD0 3C0C0010 */  lui   $t4, 0x10
/* 0D7804 7F0A2CD4 AD2C0004 */  sw    $t4, 4($t1)
/* 0D7808 7F0A2CD8 AD2B0000 */  sw    $t3, ($t1)
/* 0D780C 7F0A2CDC 8FAD00D0 */  lw    $t5, 0xd0($sp)
/* 0D7810 7F0A2CE0 3C0FBA00 */  lui   $t7, (0xBA001001 >> 16) # lui $t7, 0xba00
/* 0D7814 7F0A2CE4 35EF1001 */  ori   $t7, (0xBA001001 & 0xFFFF) # ori $t7, $t7, 0x1001
/* 0D7818 7F0A2CE8 25AE0008 */  addiu $t6, $t5, 8
/* 0D781C 7F0A2CEC AFAE00D0 */  sw    $t6, 0xd0($sp)
/* 0D7820 7F0A2CF0 3C180001 */  lui   $t8, 1
/* 0D7824 7F0A2CF4 ADB80004 */  sw    $t8, 4($t5)
/* 0D7828 7F0A2CF8 ADAF0000 */  sw    $t7, ($t5)
/* 0D782C 7F0A2CFC 8FB900D0 */  lw    $t9, 0xd0($sp)
/* 0D7830 7F0A2D00 3C09B600 */  lui   $t1, 0xb600
/* 0D7834 7F0A2D04 240A3000 */  li    $t2, 12288
/* 0D7838 7F0A2D08 27280008 */  addiu $t0, $t9, 8
/* 0D783C 7F0A2D0C AFA800D0 */  sw    $t0, 0xd0($sp)
/* 0D7840 7F0A2D10 AF2A0004 */  sw    $t2, 4($t9)
/* 0D7844 7F0A2D14 AF290000 */  sw    $t1, ($t9)
/* 0D7848 7F0A2D18 8FAB00D0 */  lw    $t3, 0xd0($sp)
/* 0D784C 7F0A2D1C 3C0DBA00 */  lui   $t5, (0xBA000C02 >> 16) # lui $t5, 0xba00
/* 0D7850 7F0A2D20 35AD0C02 */  ori   $t5, (0xBA000C02 & 0xFFFF) # ori $t5, $t5, 0xc02
/* 0D7854 7F0A2D24 256C0008 */  addiu $t4, $t3, 8
/* 0D7858 7F0A2D28 AFAC00D0 */  sw    $t4, 0xd0($sp)
/* 0D785C 7F0A2D2C 240E2000 */  li    $t6, 8192
/* 0D7860 7F0A2D30 AD6E0004 */  sw    $t6, 4($t3)
/* 0D7864 7F0A2D34 AD6D0000 */  sw    $t5, ($t3)
/* 0D7868 7F0A2D38 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D786C 7F0A2D3C 3C19B700 */  lui   $t9, 0xb700
/* 0D7870 7F0A2D40 3C080006 */  lui   $t0, 6
/* 0D7874 7F0A2D44 25F80008 */  addiu $t8, $t7, 8
/* 0D7878 7F0A2D48 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D787C 7F0A2D4C ADE80004 */  sw    $t0, 4($t7)
/* 0D7880 7F0A2D50 ADF90000 */  sw    $t9, ($t7)
/* 0D7884 7F0A2D54 8FB000D0 */  lw    $s0, 0xd0($sp)
/* 0D7888 7F0A2D58 3C0B0103 */  lui   $t3, (0x01030040 >> 16) # lui $t3, 0x103
/* 0D788C 7F0A2D5C 356B0040 */  ori   $t3, (0x01030040 & 0xFFFF) # ori $t3, $t3, 0x40
/* 0D7890 7F0A2D60 260A0008 */  addiu $t2, $s0, 8
/* 0D7894 7F0A2D64 AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7898 7F0A2D68 0FC1E0E5 */  jal   get_BONDdata_field_10E0
/* 0D789C 7F0A2D6C AE0B0000 */   sw    $t3, ($s0)
/* 0D78A0 7F0A2D70 0C003A2C */  jal   osVirtualToPhysical
/* 0D78A4 7F0A2D74 00402025 */   move  $a0, $v0
/* 0D78A8 7F0A2D78 AE020004 */  sw    $v0, 4($s0)
/* 0D78AC 7F0A2D7C 3C048008 */  lui   $a0, %hi(SHATTERED_WINDOW_PIECES_BUFFER_LEN)
/* 0D78B0 7F0A2D80 8C84A160 */  lw    $a0, %lo(SHATTERED_WINDOW_PIECES_BUFFER_LEN)($a0)
/* 0D78B4 7F0A2D84 00008025 */  move  $s0, $zero
/* 0D78B8 7F0A2D88 0000A025 */  move  $s4, $zero
/* 0D78BC 7F0A2D8C 18800044 */  blez  $a0, .L7F0A2EA0
/* 0D78C0 7F0A2D90 3C170420 */   lui   $s7, (0x04200030 >> 16) # lui $s7, 0x420
/* 0D78C4 7F0A2D94 3C168008 */  lui   $s6, %hi(g_CurrentPlayer)
/* 0D78C8 7F0A2D98 3C138008 */  lui   $s3, %hi(ptr_shattered_window_pieces)
/* 0D78CC 7F0A2D9C 2673A164 */  addiu $s3, %lo(ptr_shattered_window_pieces) # addiu $s3, $s3, -0x5e9c
/* 0D78D0 7F0A2DA0 26D6A0B0 */  addiu $s6, %lo(g_CurrentPlayer) # addiu $s6, $s6, -0x5f50
/* 0D78D4 7F0A2DA4 3C1EBF00 */  lui   $fp, 0xbf00
/* 0D78D8 7F0A2DA8 36F70030 */  ori   $s7, (0x04200030 & 0xFFFF) # ori $s7, $s7, 0x30
/* 0D78DC 7F0A2DAC 27B50090 */  addiu $s5, $sp, 0x90
.L7F0A2DB0:
/* 0D78E0 7F0A2DB0 8E6C0000 */  lw    $t4, ($s3)
/* 0D78E4 7F0A2DB4 01906821 */  addu  $t5, $t4, $s0
/* 0D78E8 7F0A2DB8 8DAE0000 */  lw    $t6, ($t5)
/* 0D78EC 7F0A2DBC 59C00035 */  blezl $t6, .L7F0A2E94
/* 0D78F0 7F0A2DC0 26940001 */   addiu $s4, $s4, 1
/* 0D78F4 7F0A2DC4 0FC2F5B8 */  jal   dynAllocateMatrix
/* 0D78F8 7F0A2DC8 00000000 */   nop
/* 0D78FC 7F0A2DCC 8E6F0000 */  lw    $t7, ($s3)
/* 0D7900 7F0A2DD0 00408825 */  move  $s1, $v0
/* 0D7904 7F0A2DD4 02A03025 */  move  $a2, $s5
/* 0D7908 7F0A2DD8 01F01821 */  addu  $v1, $t7, $s0
/* 0D790C 7F0A2DDC 24640004 */  addiu $a0, $v1, 4
/* 0D7910 7F0A2DE0 0FC1624B */  jal   matrix_4x4_set_position_and_rotation_around_xyz
/* 0D7914 7F0A2DE4 24650010 */   addiu $a1, $v1, 0x10
/* 0D7918 7F0A2DE8 8EC20000 */  lw    $v0, ($s6)
/* 0D791C 7F0A2DEC C7A400C0 */  lwc1  $f4, 0xc0($sp)
/* 0D7920 7F0A2DF0 C7AA00C4 */  lwc1  $f10, 0xc4($sp)
/* 0D7924 7F0A2DF4 C4460038 */  lwc1  $f6, 0x38($v0)
/* 0D7928 7F0A2DF8 02A02025 */  move  $a0, $s5
/* 0D792C 7F0A2DFC 02202825 */  move  $a1, $s1
/* 0D7930 7F0A2E00 46062201 */  sub.s $f8, $f4, $f6
/* 0D7934 7F0A2E04 C7A400C8 */  lwc1  $f4, 0xc8($sp)
/* 0D7938 7F0A2E08 E7A800C0 */  swc1  $f8, 0xc0($sp)
/* 0D793C 7F0A2E0C C450003C */  lwc1  $f16, 0x3c($v0)
/* 0D7940 7F0A2E10 46105481 */  sub.s $f18, $f10, $f16
/* 0D7944 7F0A2E14 E7B200C4 */  swc1  $f18, 0xc4($sp)
/* 0D7948 7F0A2E18 C4460040 */  lwc1  $f6, 0x40($v0)
/* 0D794C 7F0A2E1C 46062201 */  sub.s $f8, $f4, $f6
/* 0D7950 7F0A2E20 0FC16327 */  jal   matrix_4x4_f32_to_s32
/* 0D7954 7F0A2E24 E7A800C8 */   swc1  $f8, 0xc8($sp)
/* 0D7958 7F0A2E28 8FB200D0 */  lw    $s2, 0xd0($sp)
/* 0D795C 7F0A2E2C 3C080102 */  lui   $t0, (0x01020040 >> 16) # lui $t0, 0x102
/* 0D7960 7F0A2E30 35080040 */  ori   $t0, (0x01020040 & 0xFFFF) # ori $t0, $t0, 0x40
/* 0D7964 7F0A2E34 26590008 */  addiu $t9, $s2, 8
/* 0D7968 7F0A2E38 AFB900D0 */  sw    $t9, 0xd0($sp)
/* 0D796C 7F0A2E3C 02202025 */  move  $a0, $s1
/* 0D7970 7F0A2E40 0C003A2C */  jal   osVirtualToPhysical
/* 0D7974 7F0A2E44 AE480000 */   sw    $t0, ($s2)
/* 0D7978 7F0A2E48 AE420004 */  sw    $v0, 4($s2)
/* 0D797C 7F0A2E4C 8FB100D0 */  lw    $s1, 0xd0($sp)
/* 0D7980 7F0A2E50 262A0008 */  addiu $t2, $s1, 8
/* 0D7984 7F0A2E54 AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7988 7F0A2E58 AE370000 */  sw    $s7, ($s1)
/* 0D798C 7F0A2E5C 8E6B0000 */  lw    $t3, ($s3)
/* 0D7990 7F0A2E60 01702021 */  addu  $a0, $t3, $s0
/* 0D7994 7F0A2E64 0C003A2C */  jal   osVirtualToPhysical
/* 0D7998 7F0A2E68 24840038 */   addiu $a0, $a0, 0x38
/* 0D799C 7F0A2E6C AE220004 */  sw    $v0, 4($s1)
/* 0D79A0 7F0A2E70 8FAC00D0 */  lw    $t4, 0xd0($sp)
/* 0D79A4 7F0A2E74 240E0A14 */  li    $t6, 2580
/* 0D79A8 7F0A2E78 3C048008 */  lui   $a0, %hi(SHATTERED_WINDOW_PIECES_BUFFER_LEN)
/* 0D79AC 7F0A2E7C 258D0008 */  addiu $t5, $t4, 8
/* 0D79B0 7F0A2E80 AFAD00D0 */  sw    $t5, 0xd0($sp)
/* 0D79B4 7F0A2E84 AD8E0004 */  sw    $t6, 4($t4)
/* 0D79B8 7F0A2E88 AD9E0000 */  sw    $fp, ($t4)
/* 0D79BC 7F0A2E8C 8C84A160 */  lw    $a0, %lo(SHATTERED_WINDOW_PIECES_BUFFER_LEN)($a0)
/* 0D79C0 7F0A2E90 26940001 */  addiu $s4, $s4, 1
.L7F0A2E94:
/* 0D79C4 7F0A2E94 0284082A */  slt   $at, $s4, $a0
/* 0D79C8 7F0A2E98 1420FFC5 */  bnez  $at, .L7F0A2DB0
/* 0D79CC 7F0A2E9C 26100068 */   addiu $s0, $s0, 0x68
.L7F0A2EA0:
/* 0D79D0 7F0A2EA0 8FAF00D0 */  lw    $t7, 0xd0($sp)
/* 0D79D4 7F0A2EA4 3C19B600 */  lui   $t9, 0xb600
/* 0D79D8 7F0A2EA8 3C080006 */  lui   $t0, 6
/* 0D79DC 7F0A2EAC 25F80008 */  addiu $t8, $t7, 8
/* 0D79E0 7F0A2EB0 AFB800D0 */  sw    $t8, 0xd0($sp)
/* 0D79E4 7F0A2EB4 ADE80004 */  sw    $t0, 4($t7)
/* 0D79E8 7F0A2EB8 ADF90000 */  sw    $t9, ($t7)
/* 0D79EC 7F0A2EBC 8FB000D0 */  lw    $s0, 0xd0($sp)
/* 0D79F0 7F0A2EC0 3C0B0103 */  lui   $t3, (0x01030040 >> 16) # lui $t3, 0x103
/* 0D79F4 7F0A2EC4 356B0040 */  ori   $t3, (0x01030040 & 0xFFFF) # ori $t3, $t3, 0x40
/* 0D79F8 7F0A2EC8 260A0008 */  addiu $t2, $s0, 8
/* 0D79FC 7F0A2ECC AFAA00D0 */  sw    $t2, 0xd0($sp)
/* 0D7A00 7F0A2ED0 0FC1E0DD */  jal   currentPlayerGetProjectionMatrix
/* 0D7A04 7F0A2ED4 AE0B0000 */   sw    $t3, ($s0)
/* 0D7A08 7F0A2ED8 AE020004 */  sw    $v0, 4($s0)
/* 0D7A0C 7F0A2EDC 8FB100D0 */  lw    $s1, 0xd0($sp)
/* 0D7A10 7F0A2EE0 3C0E0102 */  lui   $t6, (0x01020040 >> 16) # lui $t6, 0x102
/* 0D7A14 7F0A2EE4 35CE0040 */  ori   $t6, (0x01020040 & 0xFFFF) # ori $t6, $t6, 0x40
/* 0D7A18 7F0A2EE8 262D0008 */  addiu $t5, $s1, 8
/* 0D7A1C 7F0A2EEC AFAD00D0 */  sw    $t5, 0xd0($sp)
/* 0D7A20 7F0A2EF0 0FC1E0D5 */  jal   currentPlayerGetMatrix10C8
/* 0D7A24 7F0A2EF4 AE2E0000 */   sw    $t6, ($s1)
/* 0D7A28 7F0A2EF8 AE220004 */  sw    $v0, 4($s1)
/* 0D7A2C 7F0A2EFC 8FBF0044 */  lw    $ra, 0x44($sp)
/* 0D7A30 7F0A2F00 8FBE0040 */  lw    $fp, 0x40($sp)
/* 0D7A34 7F0A2F04 8FB7003C */  lw    $s7, 0x3c($sp)
/* 0D7A38 7F0A2F08 8FB60038 */  lw    $s6, 0x38($sp)
/* 0D7A3C 7F0A2F0C 8FB50034 */  lw    $s5, 0x34($sp)
/* 0D7A40 7F0A2F10 8FB40030 */  lw    $s4, 0x30($sp)
/* 0D7A44 7F0A2F14 8FB3002C */  lw    $s3, 0x2c($sp)
/* 0D7A48 7F0A2F18 8FB20028 */  lw    $s2, 0x28($sp)
/* 0D7A4C 7F0A2F1C 8FB10024 */  lw    $s1, 0x24($sp)
/* 0D7A50 7F0A2F20 8FB00020 */  lw    $s0, 0x20($sp)
/* 0D7A54 7F0A2F24 8FA200D0 */  lw    $v0, 0xd0($sp)
/* 0D7A58 7F0A2F28 03E00008 */  jr    $ra
/* 0D7A5C 7F0A2F2C 27BD00D0 */   addiu $sp, $sp, 0xd0
)
#endif /* LEFTOVERDEBUG */
#endif



