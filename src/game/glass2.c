#include <ultra64.h>
#include <limits.h>
#include "math_atan2f.h"
#include "gbi_extension.h"
#include "glass.h"
#include "random.h"
#include "image_bank.h"
#include "lv.h"
#include "objective_status.h"

#define BULLET_SPARKS_MAX 20
#define BULLET_MOVING_SPARKS_MAX 50
#define GAUGE_BAR_VERTEX_PAIR_STRIDE (2 * sizeof(struct WatchVertex))

//D:80040960
struct rgba_u8 g_BulletSparkColors[8] = {
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xC8, 0xFF },
    { 0xFF, 0x00, 0x00, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0xFF, 0xFF },
    { 0 },
    { 0 }
};
u32 D_80040980 = 0;


// something explosion related
// size of each item is 0x2c (see bullet_spark_create)
//CODE.bss:8007A170
s_bullet_spark g_BulletSparkArray[BULLET_SPARKS_MAX];

#ifndef VERSION_EU

//CODE.bss:8007A4E0
s_moving_bullet_spark g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX];

#endif


#if defined(LEFTOVERDEBUG)
/*
  Render Health Bars
  AI Comment: This function populates a radial array of HUD elements (HealthSegments) with position and color data
  based on a damage value (HealthValue) and a display mode (isArmour). It loops through 23 segments, calculating their
  screen-space coordinates and visual properties using trigonometric functions.
  @healthSegments : A pointer to an Array of 46 vertices.
  @isArmour : Armour/Health if positive/negative
  @numsegments : Not Used
  @HealthValue : amount of health/armour 0-10
  @Address: 7F0A2F30
*/
void hudMakeDamageSegments(struct damage_display_val *HealthSegments, s32 numSegments, s32 isArmour, f32 HealthValue)
{
	s32 unused;
    s32 i;
    s32 pairIndex;
    f32 angleRadians;

    HealthValue *= 8;


    //for 145.2 to 35.2 degrees, calculate health/armour
    for (i=0; i<23; i++)
    {
        //This line calculates an angle in radians, starting from 142° (cast truncated) and decreasing by 5° per iteration.
        angleRadians = ((f32) (s32)(142.5 - (i*5))* M_PI_F * 2) / 360;

        for (pairIndex = 0; pairIndex < 2; pairIndex++)
        {
            s16 radialOffsetX = (((sinf(angleRadians) * 4 * 130 * (6 - pairIndex)) / 5) * isArmour);
            s16 radialOffsetZ = (((cosf(angleRadians) * 4) * 130 * (6 - pairIndex)) / 5);

            HealthSegments->pos.x    = (radialOffsetX + 1);
            HealthSegments->pos.y    = 0;
            HealthSegments->pos.z    = -radialOffsetZ;
            HealthSegments->normal.x = 0;
            HealthSegments->normal.y = 0;
            HealthSegments->normal.z = 0;
            HealthSegments->colour.r = 255;
            HealthSegments->colour.g = 255;
            HealthSegments->colour.b = 255;

            if (isArmour >= TRUE) //armour shade
            {
                HealthSegments->colour.r = (int)(96 - (cosf(angleRadians) * 96));
                HealthSegments->colour.g = (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = 255;
            }
            else if (isArmour < FALSE) //health shade
            {
                HealthSegments->colour.g =  (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = (int)(32 - (cosf(angleRadians) * 32));
            }

            // segments 0-9 are single, 10-22 are doubled with single gaps
            // IF i < damage fill, if fractional, shade else no fill.
            if (i < 10) //145 to 95
            {
                //full shade
                if (i <= (((int)HealthValue * 2) - 1))
                {
                    HealthSegments->colour.a = 255;
                }
                // Fraction fill
                else if ((i < (int)(HealthValue * 2.0f)) && (i > (((int)HealthValue * 2) - 1))) //yes, it looks like one is float, the other is cast
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else //no fill
                {
                    HealthSegments->colour.a = 48;
                }
            }
            else if (i >= 10) //95 to 35
            {
                if ( i <= (9 + ((HealthValue - 5.0f) * 4)))
                {
                    HealthSegments->colour.a = 255;
                }
                else if (i <= (((int)(((HealthValue - 5.0f) * 4) + 0.5f) + 9)) && (i > (((int)(HealthValue - 5.0f) * 2) + 8)))
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else
                {
                    HealthSegments->colour.a = 48;
                }
            }
            HealthSegments++;
        }
    }
}
#endif

#if !defined(LEFTOVERDEBUG)
void hudMakeDamageSegments(struct damage_display_val *HealthSegments, s32 numSegments, s32 isArmour, f32 HealthValue)
{
	s32 new_var2;
	f32 new_var;
	s32 pairIndex;
	s32 i;
	s16 temp_s1;
	s32 sp80;
	f32 temp_f18;
	f32 temp_f4;
	f32 sp74;
	f32 angleRadians;
	HealthValue *= 8.0f;
	sp80 = 0;

	for (i = 0; i < 23; i++)
	{
		temp_f18 = (f32) ((s32) (142.5 - ((f64) sp80)));
		angleRadians = ((temp_f18 * M_PI_F) * ((f32) 2)) / 360.0f;
		for (pairIndex = 0; pairIndex < 2; pairIndex++)
		{
	        sp74 = (f32) isArmour;
			temp_s1 = (s16) ((s32) (((((sinf(angleRadians) * 4.0f) * 130.0f) * ((f32) (6 - pairIndex))) / 5.0f) * sp74));
			temp_f4 = cosf(angleRadians) * 4.0f;
			HealthSegments->pos.x = temp_s1 + 1;
			HealthSegments->pos.y = 0;
			HealthSegments->normal.x = 0;
			HealthSegments->normal.y = 0;
			new_var = HealthValue - 5.0f;
			HealthSegments->normal.z = 0;
			HealthSegments->colour.r = 0xFF;
			HealthSegments->colour.g = 0xFF;
			temp_s1 = ((temp_f4 * 130.0f) * ((f32) (6 - pairIndex))) / 5;
			HealthSegments->colour.b = 0xFF;
			HealthSegments->pos.z = (s16) (-((s32) temp_s1));
			if (isArmour > 0)
			{
				HealthSegments->colour.r = (s8) ((s32) (96.0f - (cosf(angleRadians) * 96.0f)));
				HealthSegments->colour.g = (s8) ((s32) (127.0f - (cosf(angleRadians) * 127.0f)));
				HealthSegments->colour.b = 0xFF;
			}
			else if (isArmour < 0)
			{
				HealthSegments->colour.g = (s8) ((s32) (127.0f - (cosf(angleRadians) * 127.0f)));
				HealthSegments->colour.b = (s8) ((s32) (32.0f - (cosf(angleRadians) * 32.0f)));
			}
			if (i < 10)
			{
				if (((((s32) HealthValue) * 2) - 1) >= i)
				{
					HealthSegments->colour.a = 0xFF;
				}
				else if ((i < ((s32) (2.0f * HealthValue))) && (((((s32) HealthValue) * 2) - 1) < i))
				{
					HealthSegments->colour.a = (s8) (((s32) ((HealthValue - ((f32) ((s32) HealthValue))) * 207.0f)) + 0x30);
				}
				else
				{
					HealthSegments->colour.a = 0x30;
				}
			}
			else if (i >= 10)
			{
				if (((f32) i) <= (9.0f + ((HealthValue - 5.0f) * 4.0f)))
				{
					HealthSegments->colour.a = 0xFF;
				}
				else
				{
					new_var2 = i;
					if (((((s32) ((new_var * 4.0f) + 0.5f)) + 9) >= new_var2) && (((((s32) (HealthValue - 5.0f)) * 2) + 8) < new_var2))
					{
						HealthSegments->colour.a = (s8) (((s32) ((HealthValue - ((f32) ((s32) HealthValue))) * 207.0f)) + 0x30);
					}
					else
					{
						HealthSegments->colour.a = 0x30;
					}
				}
			}
			HealthSegments += 1;
		}
		sp80 += 5;
	}
}
#endif


/**
 * Address: 7F0A3330
 *
 * Creates the display list for HUD and watch health and armor bars.
 */
Gfx *buildGaugeBarDL(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices)
{
    s8 i;

    for (i = 0; i <= (numvertices / 2 - 2); i++) 
    {
        gSPVertex(gdl++, vtxaddr, 4, 0);

        if (i >= 9) 
        {
            if ((i + 3) % 4) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        } 
        else if (i < 9) 
        {
            if ((i & 1) == 0) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        }

        vtxaddr += GAUGE_BAR_VERTEX_PAIR_STRIDE;
    }

    gSPEndDisplayList(gdl++);

    return gdl;
}


/**
 * Address: 7F0A33F8
 */
#if defined(LEFTOVERDEBUG)
void sub_GAME_7F0A33F8(struct WatchVertex *vtx, s32 numverts, f32 scale, s32 arg3)
{
    f32 angle;
    s32 i;
    s16 sinval;
    s16 cosval;

    if (arg3)
    {
        vtx->coord1.x = 1;
        vtx->coord1.y = 0;
        vtx->coord1.z = 0;
        vtx->coord2.x = 0;
        vtx->coord2.y = 0;
        vtx->coord2.z = 0;
        vtx->color.r = 0;
        vtx->color.g = 0x2c;
        vtx->color.b = 0;
        vtx->color.a = 0xb0;

        vtx++;
    }

    for (i = 7; i <= (numverts - 7); i += 2)
    {
        angle = ((f32)i * M_PI_F) / numverts;
        sinval = sinf(angle) * 520.0f * scale;
        cosval = cosf(angle) * 520.0f * scale;

        vtx->coord1.x = 1 + sinval;
        vtx->coord1.y = 0;
        vtx->coord1.z = -cosval;
        vtx->coord2.x = 0;
        vtx->coord2.y = 0;
        vtx->coord2.z = 0;
        vtx->color.r = 0 - (cosf(angle) * 0);
        vtx->color.g = 44.0f - (cosf(angle) * 20.0f);
        vtx->color.b = 0 - (cosf(angle) * 0);
        vtx->color.a = 0xb0;

        vtx++;

        if ((i != 0) && (i < numverts))
        {
            vtx->coord1.x = 1 + -sinval;
            vtx->coord1.y = 0;
            vtx->coord1.z = -cosval;
            vtx->coord2.x = 0;
            vtx->coord2.y = 0;
            vtx->coord2.z = 0;

            vtx->color.r = 0xFF;
            vtx->color.g = 0xFF;
            vtx->color.b = 0xFF;

            vtx->color.r = 0 - (cosf(angle) * 0);
            vtx->color.g = 44.0f - (cosf(angle) * 20.0f);
            vtx->color.b = 0 - (cosf(angle) * 0);
            vtx->color.a = 0xb0;

            vtx++;
        }
    }
}
#else
/* NOTE:
    D_8004CE1C and D_8004CE20 were originally in hudMakeDamageSegments's
    late_rodata (EU build). Now that function is C, these must live here.
 */
GLOBAL_ASM(
.late_rodata
glabel D_8004CE1C /* Originally in hudMakeDamageSegments's late_rodata (EU build) */
.word 0x40490fdb /* M_PI_F */
glabel D_8004CE20 /* Originally in hudMakeDamageSegments's late_rodata (EU build) */
.word 0x40c90fdb /* M_TAU_F */
glabel D_8005774C
.word 0x3fb501e2
.text
glabel sub_GAME_7F0A33F8
/* 0D5328 7F0A2938 27BDFF80 */  addiu $sp, $sp, -0x80
/* 0D532C 7F0A293C F7BE0038 */  sdc1  $f30, 0x38($sp)
/* 0D5330 7F0A2940 4486F000 */  mtc1  $a2, $f30
/* 0D5334 7F0A2944 AFB6005C */  sw    $s6, 0x5c($sp)
/* 0D5338 7F0A2948 AFB00044 */  sw    $s0, 0x44($sp)
/* 0D533C 7F0A294C 00808025 */  move  $s0, $a0
/* 0D5340 7F0A2950 00A0B025 */  move  $s6, $a1
/* 0D5344 7F0A2954 AFBF0064 */  sw    $ra, 0x64($sp)
/* 0D5348 7F0A2958 AFB70060 */  sw    $s7, 0x60($sp)
/* 0D534C 7F0A295C AFB50058 */  sw    $s5, 0x58($sp)
/* 0D5350 7F0A2960 AFB40054 */  sw    $s4, 0x54($sp)
/* 0D5354 7F0A2964 AFB30050 */  sw    $s3, 0x50($sp)
/* 0D5358 7F0A2968 AFB2004C */  sw    $s2, 0x4c($sp)
/* 0D535C 7F0A296C AFB10048 */  sw    $s1, 0x48($sp)
/* 0D5360 7F0A2970 F7BC0030 */  sdc1  $f28, 0x30($sp)
/* 0D5364 7F0A2974 F7BA0028 */  sdc1  $f26, 0x28($sp)
/* 0D5368 7F0A2978 F7B80020 */  sdc1  $f24, 0x20($sp)
/* 0D536C 7F0A297C F7B60018 */  sdc1  $f22, 0x18($sp)
/* 0D5370 7F0A2980 10E0000F */  beqz  $a3, .L7F0A29C0
/* 0D5374 7F0A2984 F7B40010 */   sdc1  $f20, 0x10($sp)
/* 0D5378 7F0A2988 241500B0 */  li    $s5, 176
/* 0D537C 7F0A298C 240E0001 */  li    $t6, 1
/* 0D5380 7F0A2990 240F002C */  li    $t7, 44
/* 0D5384 7F0A2994 A48E0000 */  sh    $t6, ($a0)
/* 0D5388 7F0A2998 A4800002 */  sh    $zero, 2($a0)
/* 0D538C 7F0A299C A4800004 */  sh    $zero, 4($a0)
/* 0D5390 7F0A29A0 A4800006 */  sh    $zero, 6($a0)
/* 0D5394 7F0A29A4 A4800008 */  sh    $zero, 8($a0)
/* 0D5398 7F0A29A8 A480000A */  sh    $zero, 0xa($a0)
/* 0D539C 7F0A29AC A080000C */  sb    $zero, 0xc($a0)
/* 0D53A0 7F0A29B0 A08F000D */  sb    $t7, 0xd($a0)
/* 0D53A4 7F0A29B4 A080000E */  sb    $zero, 0xe($a0)
/* 0D53A8 7F0A29B8 A095000F */  sb    $s5, 0xf($a0)
/* 0D53AC 7F0A29BC 24900010 */  addiu $s0, $a0, 0x10
.L7F0A29C0:
/* 0D53B0 7F0A29C0 26D7FFF9 */  addiu $s7, $s6, -7
/* 0D53B4 7F0A29C4 2AE10007 */  slti  $at, $s7, 7
/* 0D53B8 7F0A29C8 241500B0 */  li    $s5, 176
/* 0D53BC 7F0A29CC 14200129 */  bnez  $at, .L7F0A2E74
/* 0D53C0 7F0A29D0 24110007 */   li    $s1, 7
/* 0D53C4 7F0A29D4 44962000 */  mtc1  $s6, $f4
/* 0D53C8 7F0A29D8 3C0141A0 */  li    $at, 0x41A00000 # 20.000000
/* 0D53CC 7F0A29DC 4481E000 */  mtc1  $at, $f28
/* 0D53D0 7F0A29E0 46802020 */  cvt.s.w $f0, $f4
/* 0D53D4 7F0A29E4 3C014230 */  li    $at, 0x42300000 # 44.000000
/* 0D53D8 7F0A29E8 4481D000 */  mtc1  $at, $f26
/* 0D53DC 7F0A29EC 3C014402 */  li    $at, 0x44020000 # 520.000000
/* 0D53E0 7F0A29F0 4481C000 */  mtc1  $at, $f24
/* 0D53E4 7F0A29F4 4480B000 */  mtc1  $zero, $f22
/* 0D53E8 7F0A29F8 E7A0006C */  swc1  $f0, 0x6c($sp)
/* 0D53EC 7F0A29FC 241300FF */  li    $s3, 255
/* 0D53F0 7F0A2A00 44913000 */  mtc1  $s1, $f6
.L7F0A2A04:
/* 0D53F4 7F0A2A04 3C018005 */  lui   $at, %hi(D_8004CE1C) # $at, 0x8005
/* 0D53F8 7F0A2A08 C42ACE1C */  lwc1  $f10, %lo(D_8004CE1C)($at)
/* 0D53FC 7F0A2A0C 46803220 */  cvt.s.w $f8, $f6
/* 0D5400 7F0A2A10 C7A0006C */  lwc1  $f0, 0x6c($sp)
/* 0D5404 7F0A2A14 460A4402 */  mul.s $f16, $f8, $f10
/* 0D5408 7F0A2A18 46008503 */  div.s $f20, $f16, $f0
/* 0D540C 7F0A2A1C 0FC1606B */  jal   sinf
/* 0D5410 7F0A2A20 4600A306 */   mov.s $f12, $f20
/* 0D5414 7F0A2A24 46180482 */  mul.s $f18, $f0, $f24
/* 0D5418 7F0A2A28 4600A306 */  mov.s $f12, $f20
/* 0D541C 7F0A2A2C 461E9102 */  mul.s $f4, $f18, $f30
/* 0D5420 7F0A2A30 4600218D */  trunc.w.s $f6, $f4
/* 0D5424 7F0A2A34 44123000 */  mfc1  $s2, $f6
/* 0D5428 7F0A2A38 00000000 */  nop
/* 0D542C 7F0A2A3C 0012CC00 */  sll   $t9, $s2, 0x10
/* 0D5430 7F0A2A40 0FC16068 */  jal   cosf
/* 0D5434 7F0A2A44 00199403 */   sra   $s2, $t9, 0x10
/* 0D5438 7F0A2A48 46180202 */  mul.s $f8, $f0, $f24
/* 0D543C 7F0A2A4C 264B0001 */  addiu $t3, $s2, 1
/* 0D5440 7F0A2A50 A60B0000 */  sh    $t3, ($s0)
/* 0D5444 7F0A2A54 A6000002 */  sh    $zero, 2($s0)
/* 0D5448 7F0A2A58 A6000006 */  sh    $zero, 6($s0)
/* 0D544C 7F0A2A5C A6000008 */  sh    $zero, 8($s0)
/* 0D5450 7F0A2A60 A600000A */  sh    $zero, 0xa($s0)
/* 0D5454 7F0A2A64 461E4282 */  mul.s $f10, $f8, $f30
/* 0D5458 7F0A2A68 4600A306 */  mov.s $f12, $f20
/* 0D545C 7F0A2A6C 4600540D */  trunc.w.s $f16, $f10
/* 0D5460 7F0A2A70 440D8000 */  mfc1  $t5, $f16
/* 0D5464 7F0A2A74 00000000 */  nop
/* 0D5468 7F0A2A78 000DA400 */  sll   $s4, $t5, 0x10
/* 0D546C 7F0A2A7C 00145403 */  sra   $t2, $s4, 0x10
/* 0D5470 7F0A2A80 000D7023 */  negu  $t6, $t5
/* 0D5474 7F0A2A84 0140A025 */  move  $s4, $t2
/* 0D5478 7F0A2A88 0FC16068 */  jal   cosf
/* 0D547C 7F0A2A8C A60E0004 */   sh    $t6, 4($s0)
/* 0D5480 7F0A2A90 46160482 */  mul.s $f18, $f0, $f22
/* 0D5484 7F0A2A94 24180001 */  li    $t8, 1
/* 0D5488 7F0A2A98 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D548C 7F0A2A9C 4612B101 */  sub.s $f4, $f22, $f18
/* 0D5490 7F0A2AA0 444FF800 */  cfc1  $t7, $31
/* 0D5494 7F0A2AA4 44D8F800 */  ctc1  $t8, $31
/* 0D5498 7F0A2AA8 00000000 */  nop
/* 0D549C 7F0A2AAC 460021A4 */  cvt.w.s $f6, $f4
/* 0D54A0 7F0A2AB0 4458F800 */  cfc1  $t8, $31
/* 0D54A4 7F0A2AB4 00000000 */  nop
/* 0D54A8 7F0A2AB8 33180078 */  andi  $t8, $t8, 0x78
/* 0D54AC 7F0A2ABC 53000013 */  beql  $t8, $zero, .L7F0A2B0C
/* 0D54B0 7F0A2AC0 44183000 */   mfc1  $t8, $f6
/* 0D54B4 7F0A2AC4 44813000 */  mtc1  $at, $f6
/* 0D54B8 7F0A2AC8 24180001 */  li    $t8, 1
/* 0D54BC 7F0A2ACC 46062181 */  sub.s $f6, $f4, $f6
/* 0D54C0 7F0A2AD0 44D8F800 */  ctc1  $t8, $31
/* 0D54C4 7F0A2AD4 00000000 */  nop
/* 0D54C8 7F0A2AD8 460031A4 */  cvt.w.s $f6, $f6
/* 0D54CC 7F0A2ADC 4458F800 */  cfc1  $t8, $31
/* 0D54D0 7F0A2AE0 00000000 */  nop
/* 0D54D4 7F0A2AE4 33180078 */  andi  $t8, $t8, 0x78
/* 0D54D8 7F0A2AE8 17000005 */  bnez  $t8, .L7F0A2B00
/* 0D54DC 7F0A2AEC 00000000 */   nop
/* 0D54E0 7F0A2AF0 44183000 */  mfc1  $t8, $f6
/* 0D54E4 7F0A2AF4 3C018000 */  lui   $at, 0x8000
/* 0D54E8 7F0A2AF8 10000007 */  b     .L7F0A2B18
/* 0D54EC 7F0A2AFC 0301C025 */   or    $t8, $t8, $at
.L7F0A2B00:
/* 0D54F0 7F0A2B00 10000005 */  b     .L7F0A2B18
/* 0D54F4 7F0A2B04 2418FFFF */   li    $t8, -1
/* 0D54F8 7F0A2B08 44183000 */  mfc1  $t8, $f6
.L7F0A2B0C:
/* 0D54FC 7F0A2B0C 00000000 */  nop
/* 0D5500 7F0A2B10 0700FFFB */  bltz  $t8, .L7F0A2B00
/* 0D5504 7F0A2B14 00000000 */   nop
.L7F0A2B18:
/* 0D5508 7F0A2B18 44CFF800 */  ctc1  $t7, $31
/* 0D550C 7F0A2B1C A218000C */  sb    $t8, 0xc($s0)
/* 0D5510 7F0A2B20 0FC16068 */  jal   cosf
/* 0D5514 7F0A2B24 4600A306 */   mov.s $f12, $f20
/* 0D5518 7F0A2B28 461C0202 */  mul.s $f8, $f0, $f28
/* 0D551C 7F0A2B2C 24080001 */  li    $t0, 1
/* 0D5520 7F0A2B30 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D5524 7F0A2B34 4608D281 */  sub.s $f10, $f26, $f8
/* 0D5528 7F0A2B38 4459F800 */  cfc1  $t9, $31
/* 0D552C 7F0A2B3C 44C8F800 */  ctc1  $t0, $31
/* 0D5530 7F0A2B40 00000000 */  nop
/* 0D5534 7F0A2B44 46005424 */  cvt.w.s $f16, $f10
/* 0D5538 7F0A2B48 4448F800 */  cfc1  $t0, $31
/* 0D553C 7F0A2B4C 00000000 */  nop
/* 0D5540 7F0A2B50 31080078 */  andi  $t0, $t0, 0x78
/* 0D5544 7F0A2B54 51000013 */  beql  $t0, $zero, .L7F0A2BA4
/* 0D5548 7F0A2B58 44088000 */   mfc1  $t0, $f16
/* 0D554C 7F0A2B5C 44818000 */  mtc1  $at, $f16
/* 0D5550 7F0A2B60 24080001 */  li    $t0, 1
/* 0D5554 7F0A2B64 46105401 */  sub.s $f16, $f10, $f16
/* 0D5558 7F0A2B68 44C8F800 */  ctc1  $t0, $31
/* 0D555C 7F0A2B6C 00000000 */  nop
/* 0D5560 7F0A2B70 46008424 */  cvt.w.s $f16, $f16
/* 0D5564 7F0A2B74 4448F800 */  cfc1  $t0, $31
/* 0D5568 7F0A2B78 00000000 */  nop
/* 0D556C 7F0A2B7C 31080078 */  andi  $t0, $t0, 0x78
/* 0D5570 7F0A2B80 15000005 */  bnez  $t0, .L7F0A2B98
/* 0D5574 7F0A2B84 00000000 */   nop
/* 0D5578 7F0A2B88 44088000 */  mfc1  $t0, $f16
/* 0D557C 7F0A2B8C 3C018000 */  lui   $at, 0x8000
/* 0D5580 7F0A2B90 10000007 */  b     .L7F0A2BB0
/* 0D5584 7F0A2B94 01014025 */   or    $t0, $t0, $at
.L7F0A2B98:
/* 0D5588 7F0A2B98 10000005 */  b     .L7F0A2BB0
/* 0D558C 7F0A2B9C 2408FFFF */   li    $t0, -1
/* 0D5590 7F0A2BA0 44088000 */  mfc1  $t0, $f16
.L7F0A2BA4:
/* 0D5594 7F0A2BA4 00000000 */  nop
/* 0D5598 7F0A2BA8 0500FFFB */  bltz  $t0, .L7F0A2B98
/* 0D559C 7F0A2BAC 00000000 */   nop
.L7F0A2BB0:
/* 0D55A0 7F0A2BB0 44D9F800 */  ctc1  $t9, $31
/* 0D55A4 7F0A2BB4 A208000D */  sb    $t0, 0xd($s0)
/* 0D55A8 7F0A2BB8 0FC16068 */  jal   cosf
/* 0D55AC 7F0A2BBC 4600A306 */   mov.s $f12, $f20
/* 0D55B0 7F0A2BC0 46160482 */  mul.s $f18, $f0, $f22
/* 0D55B4 7F0A2BC4 240A0001 */  li    $t2, 1
/* 0D55B8 7F0A2BC8 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D55BC 7F0A2BCC 4612B101 */  sub.s $f4, $f22, $f18
/* 0D55C0 7F0A2BD0 4449F800 */  cfc1  $t1, $31
/* 0D55C4 7F0A2BD4 44CAF800 */  ctc1  $t2, $31
/* 0D55C8 7F0A2BD8 00000000 */  nop
/* 0D55CC 7F0A2BDC 460021A4 */  cvt.w.s $f6, $f4
/* 0D55D0 7F0A2BE0 444AF800 */  cfc1  $t2, $31
/* 0D55D4 7F0A2BE4 00000000 */  nop
/* 0D55D8 7F0A2BE8 314A0078 */  andi  $t2, $t2, 0x78
/* 0D55DC 7F0A2BEC 51400013 */  beql  $t2, $zero, .L7F0A2C3C
/* 0D55E0 7F0A2BF0 440A3000 */   mfc1  $t2, $f6
/* 0D55E4 7F0A2BF4 44813000 */  mtc1  $at, $f6
/* 0D55E8 7F0A2BF8 240A0001 */  li    $t2, 1
/* 0D55EC 7F0A2BFC 46062181 */  sub.s $f6, $f4, $f6
/* 0D55F0 7F0A2C00 44CAF800 */  ctc1  $t2, $31
/* 0D55F4 7F0A2C04 00000000 */  nop
/* 0D55F8 7F0A2C08 460031A4 */  cvt.w.s $f6, $f6
/* 0D55FC 7F0A2C0C 444AF800 */  cfc1  $t2, $31
/* 0D5600 7F0A2C10 00000000 */  nop
/* 0D5604 7F0A2C14 314A0078 */  andi  $t2, $t2, 0x78
/* 0D5608 7F0A2C18 15400005 */  bnez  $t2, .L7F0A2C30
/* 0D560C 7F0A2C1C 00000000 */   nop
/* 0D5610 7F0A2C20 440A3000 */  mfc1  $t2, $f6
/* 0D5614 7F0A2C24 3C018000 */  lui   $at, 0x8000
/* 0D5618 7F0A2C28 10000007 */  b     .L7F0A2C48
/* 0D561C 7F0A2C2C 01415025 */   or    $t2, $t2, $at
.L7F0A2C30:
/* 0D5620 7F0A2C30 10000005 */  b     .L7F0A2C48
/* 0D5624 7F0A2C34 240AFFFF */   li    $t2, -1
/* 0D5628 7F0A2C38 440A3000 */  mfc1  $t2, $f6
.L7F0A2C3C:
/* 0D562C 7F0A2C3C 00000000 */  nop
/* 0D5630 7F0A2C40 0540FFFB */  bltz  $t2, .L7F0A2C30
/* 0D5634 7F0A2C44 00000000 */   nop
.L7F0A2C48:
/* 0D5638 7F0A2C48 44C9F800 */  ctc1  $t1, $31
/* 0D563C 7F0A2C4C A20A000E */  sb    $t2, 0xe($s0)
/* 0D5640 7F0A2C50 A215000F */  sb    $s5, 0xf($s0)
/* 0D5644 7F0A2C54 12200083 */  beqz  $s1, .L7F0A2E64
/* 0D5648 7F0A2C58 26100010 */   addiu $s0, $s0, 0x10
/* 0D564C 7F0A2C5C 0236082A */  slt   $at, $s1, $s6
/* 0D5650 7F0A2C60 10200080 */  beqz  $at, .L7F0A2E64
/* 0D5654 7F0A2C64 240B0001 */   li    $t3, 1
/* 0D5658 7F0A2C68 01726023 */  subu  $t4, $t3, $s2
/* 0D565C 7F0A2C6C 00146823 */  negu  $t5, $s4
/* 0D5660 7F0A2C70 A60C0000 */  sh    $t4, ($s0)
/* 0D5664 7F0A2C74 A6000002 */  sh    $zero, 2($s0)
/* 0D5668 7F0A2C78 A60D0004 */  sh    $t5, 4($s0)
/* 0D566C 7F0A2C7C A6000006 */  sh    $zero, 6($s0)
/* 0D5670 7F0A2C80 A6000008 */  sh    $zero, 8($s0)
/* 0D5674 7F0A2C84 A600000A */  sh    $zero, 0xa($s0)
/* 0D5678 7F0A2C88 A213000C */  sb    $s3, 0xc($s0)
/* 0D567C 7F0A2C8C A213000D */  sb    $s3, 0xd($s0)
/* 0D5680 7F0A2C90 A213000E */  sb    $s3, 0xe($s0)
/* 0D5684 7F0A2C94 0FC16068 */  jal   cosf
/* 0D5688 7F0A2C98 4600A306 */   mov.s $f12, $f20
/* 0D568C 7F0A2C9C 46160202 */  mul.s $f8, $f0, $f22
/* 0D5690 7F0A2CA0 240F0001 */  li    $t7, 1
/* 0D5694 7F0A2CA4 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D5698 7F0A2CA8 4608B281 */  sub.s $f10, $f22, $f8
/* 0D569C 7F0A2CAC 444EF800 */  cfc1  $t6, $31
/* 0D56A0 7F0A2CB0 44CFF800 */  ctc1  $t7, $31
/* 0D56A4 7F0A2CB4 00000000 */  nop
/* 0D56A8 7F0A2CB8 46005424 */  cvt.w.s $f16, $f10
/* 0D56AC 7F0A2CBC 444FF800 */  cfc1  $t7, $31
/* 0D56B0 7F0A2CC0 00000000 */  nop
/* 0D56B4 7F0A2CC4 31EF0078 */  andi  $t7, $t7, 0x78
/* 0D56B8 7F0A2CC8 51E00013 */  beql  $t7, $zero, .L7F0A2D18
/* 0D56BC 7F0A2CCC 440F8000 */   mfc1  $t7, $f16
/* 0D56C0 7F0A2CD0 44818000 */  mtc1  $at, $f16
/* 0D56C4 7F0A2CD4 240F0001 */  li    $t7, 1
/* 0D56C8 7F0A2CD8 46105401 */  sub.s $f16, $f10, $f16
/* 0D56CC 7F0A2CDC 44CFF800 */  ctc1  $t7, $31
/* 0D56D0 7F0A2CE0 00000000 */  nop
/* 0D56D4 7F0A2CE4 46008424 */  cvt.w.s $f16, $f16
/* 0D56D8 7F0A2CE8 444FF800 */  cfc1  $t7, $31
/* 0D56DC 7F0A2CEC 00000000 */  nop
/* 0D56E0 7F0A2CF0 31EF0078 */  andi  $t7, $t7, 0x78
/* 0D56E4 7F0A2CF4 15E00005 */  bnez  $t7, .L7F0A2D0C
/* 0D56E8 7F0A2CF8 00000000 */   nop
/* 0D56EC 7F0A2CFC 440F8000 */  mfc1  $t7, $f16
/* 0D56F0 7F0A2D00 3C018000 */  lui   $at, 0x8000
/* 0D56F4 7F0A2D04 10000007 */  b     .L7F0A2D24
/* 0D56F8 7F0A2D08 01E17825 */   or    $t7, $t7, $at
.L7F0A2D0C:
/* 0D56FC 7F0A2D0C 10000005 */  b     .L7F0A2D24
/* 0D5700 7F0A2D10 240FFFFF */   li    $t7, -1
/* 0D5704 7F0A2D14 440F8000 */  mfc1  $t7, $f16
.L7F0A2D18:
/* 0D5708 7F0A2D18 00000000 */  nop
/* 0D570C 7F0A2D1C 05E0FFFB */  bltz  $t7, .L7F0A2D0C
/* 0D5710 7F0A2D20 00000000 */   nop
.L7F0A2D24:
/* 0D5714 7F0A2D24 44CEF800 */  ctc1  $t6, $31
/* 0D5718 7F0A2D28 A20F000C */  sb    $t7, 0xc($s0)
/* 0D571C 7F0A2D2C 0FC16068 */  jal   cosf
/* 0D5720 7F0A2D30 4600A306 */   mov.s $f12, $f20
/* 0D5724 7F0A2D34 461C0482 */  mul.s $f18, $f0, $f28
/* 0D5728 7F0A2D38 24190001 */  li    $t9, 1
/* 0D572C 7F0A2D3C 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D5730 7F0A2D40 4612D101 */  sub.s $f4, $f26, $f18
/* 0D5734 7F0A2D44 4458F800 */  cfc1  $t8, $31
/* 0D5738 7F0A2D48 44D9F800 */  ctc1  $t9, $31
/* 0D573C 7F0A2D4C 00000000 */  nop
/* 0D5740 7F0A2D50 460021A4 */  cvt.w.s $f6, $f4
/* 0D5744 7F0A2D54 4459F800 */  cfc1  $t9, $31
/* 0D5748 7F0A2D58 00000000 */  nop
/* 0D574C 7F0A2D5C 33390078 */  andi  $t9, $t9, 0x78
/* 0D5750 7F0A2D60 53200013 */  beql  $t9, $zero, .Leu7F0A2DB0
/* 0D5754 7F0A2D64 44193000 */   mfc1  $t9, $f6
/* 0D5758 7F0A2D68 44813000 */  mtc1  $at, $f6
/* 0D575C 7F0A2D6C 24190001 */  li    $t9, 1
/* 0D5760 7F0A2D70 46062181 */  sub.s $f6, $f4, $f6
/* 0D5764 7F0A2D74 44D9F800 */  ctc1  $t9, $31
/* 0D5768 7F0A2D78 00000000 */  nop
/* 0D576C 7F0A2D7C 460031A4 */  cvt.w.s $f6, $f6
/* 0D5770 7F0A2D80 4459F800 */  cfc1  $t9, $31
/* 0D5774 7F0A2D84 00000000 */  nop
/* 0D5778 7F0A2D88 33390078 */  andi  $t9, $t9, 0x78
/* 0D577C 7F0A2D8C 17200005 */  bnez  $t9, .L7F0A2DA4
/* 0D5780 7F0A2D90 00000000 */   nop
/* 0D5784 7F0A2D94 44193000 */  mfc1  $t9, $f6
/* 0D5788 7F0A2D98 3C018000 */  lui   $at, 0x8000
/* 0D578C 7F0A2D9C 10000007 */  b     .L7F0A2DBC
/* 0D5790 7F0A2DA0 0321C825 */   or    $t9, $t9, $at
.L7F0A2DA4:
/* 0D5794 7F0A2DA4 10000005 */  b     .L7F0A2DBC
/* 0D5798 7F0A2DA8 2419FFFF */   li    $t9, -1
/* 0D579C 7F0A2DAC 44193000 */  mfc1  $t9, $f6
.Leu7F0A2DB0:
/* 0D57A0 7F0A2DB0 00000000 */  nop
/* 0D57A4 7F0A2DB4 0720FFFB */  bltz  $t9, .L7F0A2DA4
/* 0D57A8 7F0A2DB8 00000000 */   nop
.L7F0A2DBC:
/* 0D57AC 7F0A2DBC 44D8F800 */  ctc1  $t8, $31
/* 0D57B0 7F0A2DC0 A219000D */  sb    $t9, 0xd($s0)
/* 0D57B4 7F0A2DC4 0FC16068 */  jal   cosf
/* 0D57B8 7F0A2DC8 4600A306 */   mov.s $f12, $f20
/* 0D57BC 7F0A2DCC 46160202 */  mul.s $f8, $f0, $f22
/* 0D57C0 7F0A2DD0 24090001 */  li    $t1, 1
/* 0D57C4 7F0A2DD4 3C014F00 */  li    $at, 0x4F000000 # 2147483648.000000
/* 0D57C8 7F0A2DD8 4608B281 */  sub.s $f10, $f22, $f8
/* 0D57CC 7F0A2DDC 4448F800 */  cfc1  $t0, $31
/* 0D57D0 7F0A2DE0 44C9F800 */  ctc1  $t1, $31
/* 0D57D4 7F0A2DE4 00000000 */  nop
/* 0D57D8 7F0A2DE8 46005424 */  cvt.w.s $f16, $f10
/* 0D57DC 7F0A2DEC 4449F800 */  cfc1  $t1, $31
/* 0D57E0 7F0A2DF0 00000000 */  nop
/* 0D57E4 7F0A2DF4 31290078 */  andi  $t1, $t1, 0x78
/* 0D57E8 7F0A2DF8 51200013 */  beql  $t1, $zero, .L7F0A2E48
/* 0D57EC 7F0A2DFC 44098000 */   mfc1  $t1, $f16
/* 0D57F0 7F0A2E00 44818000 */  mtc1  $at, $f16
/* 0D57F4 7F0A2E04 24090001 */  li    $t1, 1
/* 0D57F8 7F0A2E08 46105401 */  sub.s $f16, $f10, $f16
/* 0D57FC 7F0A2E0C 44C9F800 */  ctc1  $t1, $31
/* 0D5800 7F0A2E10 00000000 */  nop
/* 0D5804 7F0A2E14 46008424 */  cvt.w.s $f16, $f16
/* 0D5808 7F0A2E18 4449F800 */  cfc1  $t1, $31
/* 0D580C 7F0A2E1C 00000000 */  nop
/* 0D5810 7F0A2E20 31290078 */  andi  $t1, $t1, 0x78
/* 0D5814 7F0A2E24 15200005 */  bnez  $t1, .L7F0A2E3C
/* 0D5818 7F0A2E28 00000000 */   nop
/* 0D581C 7F0A2E2C 44098000 */  mfc1  $t1, $f16
/* 0D5820 7F0A2E30 3C018000 */  lui   $at, 0x8000
/* 0D5824 7F0A2E34 10000007 */  b     .L7F0A2E54
/* 0D5828 7F0A2E38 01214825 */   or    $t1, $t1, $at
.L7F0A2E3C:
/* 0D582C 7F0A2E3C 10000005 */  b     .L7F0A2E54
/* 0D5830 7F0A2E40 2409FFFF */   li    $t1, -1
/* 0D5834 7F0A2E44 44098000 */  mfc1  $t1, $f16
.L7F0A2E48:
/* 0D5838 7F0A2E48 00000000 */  nop
/* 0D583C 7F0A2E4C 0520FFFB */  bltz  $t1, .L7F0A2E3C
/* 0D5840 7F0A2E50 00000000 */   nop
.L7F0A2E54:
/* 0D5844 7F0A2E54 44C8F800 */  ctc1  $t0, $31
/* 0D5848 7F0A2E58 A209000E */  sb    $t1, 0xe($s0)
/* 0D584C 7F0A2E5C A215000F */  sb    $s5, 0xf($s0)
/* 0D5850 7F0A2E60 26100010 */  addiu $s0, $s0, 0x10
.L7F0A2E64:
/* 0D5854 7F0A2E64 26310002 */  addiu $s1, $s1, 2
/* 0D5858 7F0A2E68 02F1082A */  slt   $at, $s7, $s1
/* 0D585C 7F0A2E6C 5020FEE5 */  beql  $at, $zero, .L7F0A2A04
/* 0D5860 7F0A2E70 44913000 */   mtc1  $s1, $f6
.L7F0A2E74:
/* 0D5864 7F0A2E74 8FBF0064 */  lw    $ra, 0x64($sp)
/* 0D5868 7F0A2E78 D7B40010 */  ldc1  $f20, 0x10($sp)
/* 0D586C 7F0A2E7C D7B60018 */  ldc1  $f22, 0x18($sp)
/* 0D5870 7F0A2E80 D7B80020 */  ldc1  $f24, 0x20($sp)
/* 0D5874 7F0A2E84 D7BA0028 */  ldc1  $f26, 0x28($sp)
/* 0D5878 7F0A2E88 D7BC0030 */  ldc1  $f28, 0x30($sp)
/* 0D587C 7F0A2E8C D7BE0038 */  ldc1  $f30, 0x38($sp)
/* 0D5880 7F0A2E90 8FB00044 */  lw    $s0, 0x44($sp)
/* 0D5884 7F0A2E94 8FB10048 */  lw    $s1, 0x48($sp)
/* 0D5888 7F0A2E98 8FB2004C */  lw    $s2, 0x4c($sp)
/* 0D588C 7F0A2E9C 8FB30050 */  lw    $s3, 0x50($sp)
/* 0D5890 7F0A2EA0 8FB40054 */  lw    $s4, 0x54($sp)
/* 0D5894 7F0A2EA4 8FB50058 */  lw    $s5, 0x58($sp)
/* 0D5898 7F0A2EA8 8FB6005C */  lw    $s6, 0x5c($sp)
/* 0D589C 7F0A2EAC 8FB70060 */  lw    $s7, 0x60($sp)
/* 0D58A0 7F0A2EB0 03E00008 */  jr    $ra
/* 0D58A4 7F0A2EB4 27BD0080 */   addiu $sp, $sp, 0x80
)
#endif


/**
 * Address: 7F0A3978
 */
Gfx *draw_watch_background(Gfx *gdl, struct WatchVertex *watch_verts, s32 unused_arg2, s32 drawFan)
{
    s8 i;
    struct WatchVertex *orig;

    if (drawFan) 
    {
        struct WatchVertex *vtx;

        orig = watch_verts;
        watch_verts++;
        vtx = watch_verts;

        i = 7;

        gSPVertex(gdl++, &vtx[14], 4, 0);
        gSPVertex(gdl++, orig, 1, 4);
        gSP2Triangles(gdl++, 2, 4, 3, 0, 0, 0, 0, 0);

        for (; i >= 0; i--) {
            gSPVertex(gdl++, &vtx[2 * i], 4, 0);
            gSPVertex(gdl++, orig, 1, 4);
            gSP2Triangles(gdl++, 0, 4, 2, 0, 1, 3, 4, 0);
        }

        gSP2Triangles(gdl++, 0, 1, 4, 0, 0, 0, 0, 0);
    } 
    else 
    {
        for (i = 0; i < 8; i++)
        {
            gSPVertex(gdl++, watch_verts, 4, 0);
            gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            watch_verts += 2;
        }
    }

    gSPEndDisplayList(gdl++);

    return gdl;
}


#ifdef NONMATCHING
/**
 * Setup watch rectangles in the usual manner.
 * This is called to setup the screen select rectangles, but note
 * that the colors are overwritten in set_page_rectangle_colors.
 * Also used to initialize watch static.
 * @param vtx: Pointer to first vertex in a {@code struct WatchRectangle}.
 * @param startx:
 * @param startz:
 * @param width:
 * @param height:
 * @param horizontal_offset:
 * @param vertical_offset:
 *
 * decomp status:
 * - compiles: yes
 * - stack resize: ok
 * - identical instructions: no
 * - identical registers: fail
 *
 * Notes: Instruction order is just wrong until the loop starts.
 * There's an extra move instruction.
*/
struct WatchVertex *setup_watch_rectangles(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontal_offset, s32 vertical_offset)
{
    s32 i;
    s32 j;
    s32 xval;
    s32 zval;

    xval = startx + horizontal_offset;

    if(vtx); // seems to be needed to match return and last few lines.

    for (i=0; i<2; i++, xval += width)
    {
        zval = startz + vertical_offset;

        for (j=0; j<2; j++, zval += height)
        {
            vtx->coord1.AsArray[0] = xval;
            vtx->coord1.AsArray[1] = 0;
            vtx->coord1.AsArray[2] = zval;

            vtx->coord2.AsArray[0] = 0;
            vtx->coord2.AsArray[1] = 0;
            vtx->coord2.AsArray[2] = 0;

            vtx->color.rgba[0] = 0x20;
            vtx->color.rgba[1] = 0x70;
            vtx->color.rgba[2] = 0x20;
            vtx->color.rgba[3] = 0xF0;

            vtx++;
        }
    }

    return vtx;
}
#else
GLOBAL_ASM(
.text
glabel setup_watch_rectangles
/* 0D85E8 7F0A3AB8 27BDFFF8 */  addiu $sp, $sp, -8
/* 0D85EC 7F0A3ABC 8FAE001C */  lw    $t6, 0x1c($sp)
/* 0D85F0 7F0A3AC0 AFB00004 */  sw    $s0, 4($sp)
/* 0D85F4 7F0A3AC4 24100002 */  li    $s0, 2
/* 0D85F8 7F0A3AC8 00AE4021 */  addu  $t0, $a1, $t6
/* 0D85FC 7F0A3ACC 8FA50018 */  lw    $a1, 0x18($sp)
/* 0D8600 7F0A3AD0 8FA90020 */  lw    $t1, 0x20($sp)
/* 0D8604 7F0A3AD4 00001025 */  move  $v0, $zero
/* 0D8608 7F0A3AD8 240D00F0 */  li    $t5, 240
/* 0D860C 7F0A3ADC 240C0070 */  li    $t4, 112
/* 0D8610 7F0A3AE0 240B0020 */  li    $t3, 32
.L7F0A3AE4:
/* 0D8614 7F0A3AE4 00001825 */  move  $v1, $zero
/* 0D8618 7F0A3AE8 00C95021 */  addu  $t2, $a2, $t1
.L7F0A3AEC:
/* 0D861C 7F0A3AEC 24630001 */  addiu $v1, $v1, 1
/* 0D8620 7F0A3AF0 A48A0004 */  sh    $t2, 4($a0)
/* 0D8624 7F0A3AF4 01455021 */  addu  $t2, $t2, $a1
/* 0D8628 7F0A3AF8 A4880000 */  sh    $t0, ($a0)
/* 0D862C 7F0A3AFC A4800002 */  sh    $zero, 2($a0)
/* 0D8630 7F0A3B00 A4800006 */  sh    $zero, 6($a0)
/* 0D8634 7F0A3B04 A4800008 */  sh    $zero, 8($a0)
/* 0D8638 7F0A3B08 A480000A */  sh    $zero, 0xa($a0)
/* 0D863C 7F0A3B0C A08B000C */  sb    $t3, 0xc($a0)
/* 0D8640 7F0A3B10 A08C000D */  sb    $t4, 0xd($a0)
/* 0D8644 7F0A3B14 A08B000E */  sb    $t3, 0xe($a0)
/* 0D8648 7F0A3B18 A08D000F */  sb    $t5, 0xf($a0)
/* 0D864C 7F0A3B1C 1470FFF3 */  bne   $v1, $s0, .L7F0A3AEC
/* 0D8650 7F0A3B20 24840010 */   addiu $a0, $a0, 0x10
/* 0D8654 7F0A3B24 24420001 */  addiu $v0, $v0, 1
/* 0D8658 7F0A3B28 1450FFEE */  bne   $v0, $s0, .L7F0A3AE4
/* 0D865C 7F0A3B2C 01074021 */   addu  $t0, $t0, $a3
/* 0D8660 7F0A3B30 8FB00004 */  lw    $s0, 4($sp)
/* 0D8664 7F0A3B34 27BD0008 */  addiu $sp, $sp, 8
/* 0D8668 7F0A3B38 03E00008 */  jr    $ra
/* 0D866C 7F0A3B3C 00801025 */   move  $v0, $a0
)
#endif





Gfx *sub_GAME_7F0A3B40(Gfx *gdl, s32 *arg1)
{
    gSPVertex(gdl++, arg1, 4, 0);

    // gfxdis can't parse this, but maybe?: gSPModifyVertex(gdl++, 16, 0, 0x2110);
    // manual specification:
    {								\
        Gfx *_g = (Gfx *)(gdl++);		\
        _g->words.w0 = 0xB1000032;	\
        _g->words.w1 = 0x2110;		\
    }

    return gdl;
}



// unreferenced
void unused_7F0A3B70(s32 arg0, struct rgba_u8 *arg1)
{
    arg1->r = g_BulletSparkColors[arg0].r;
    arg1->g = g_BulletSparkColors[arg0].g;
    arg1->b = g_BulletSparkColors[arg0].b;
    arg1->a = g_BulletSparkColors[arg0].a;
}





// unreferenced
void unused_7F0A3BA4(s32 arg0, struct rgba_u8 *arg1)
{
    g_BulletSparkColors[arg0].r = arg1->r;
    g_BulletSparkColors[arg0].g = arg1->g;
    g_BulletSparkColors[arg0].b = arg1->b;
    g_BulletSparkColors[arg0].a = arg1->a;
}


/**
 * Address: 7F0A3BD8
 */
void bullet_sparks_reset(void)
{
    s32 i;
    s32 start_index;

    if (1) { start_index = 0; }

    for (i = start_index; (i < BULLET_SPARKS_MAX) ^ 0; i++)
    {
        g_BulletSparkArray[i].unk0C = 0;
        g_BulletSparkArray[i].lifetime = 0;
        g_BulletSparkArray[i].age = 0;
    }
}


#ifdef VERSION_EU
// M_TAU_F and M_SQRT2_F are owned by still-asm functions' late_rodata;
// reference them by label so no duplicate rodata entry is emitted.
extern f32 D_8004CE20; // M_TAU_F  (0x40c90fdb)
extern f32 D_8005774C; // M_SQRT2_F (0x3fb501e2)
#define _TAU_F   D_8004CE20
#define _SQRT2_F D_8005774C
#else
#define _TAU_F   M_TAU_F
#define _SQRT2_F M_SQRT2_F
#endif

/**
 * Address: 7F0A3C08
 */
void bullet_sparks_init(s_bullet_spark *spark, coord3d *arg1, s32 arg2, f32 arg3, s16 arg4)
{
    f32 angle;

    angle = randomGetNext();
    angle *= (1.0f / M_U32_MAX_VALUE_F);
    angle *= _TAU_F;

    spark->age = 0;
    spark->unk06 = arg4;

    if (arg2 == 4)
    {
        spark->lifetime = 1;
        spark->unk08 = 1.0f;
        spark->unk0C = flareimage2;
    }
    else if (arg2 == 1)
    {
        spark->lifetime = 11;
        spark->unk08 = 0.5f;
        spark->unk0C = explosion_smokeimages;
    }
    else if (arg2 == 3)
    {
        spark->lifetime = 9;
        spark->unk08 = 0.5f;
        spark->unk0C = scattered_explosions;
    }
    else if (arg2 == 6)
    {
        spark->lifetime = 100;
        spark->unk08 = 0.0f;
        spark->unk0C = flareimage2;
    }
    else
    {
        spark->lifetime = 11;
        spark->unk08 = 0.5f;
        spark->unk0C = explosion_smokeimages;
    }

    spark->unk28 = g_BulletSparkColors[arg2].r;
    spark->unk29 = g_BulletSparkColors[arg2].g;
    spark->unk2A = g_BulletSparkColors[arg2].b;
    spark->unk2B = g_BulletSparkColors[arg2].a;

    spark->unk10 = arg1->x;
    spark->unk14 = arg1->y;
    spark->unk18 = arg1->z;

    arg3 *= 1.0f + ((f32)randomGetNext() * (1.0f / M_U32_MAX_VALUE_F) * 0.25f);
    arg3 *= _SQRT2_F;
    spark->unk24 = arg3;

    spark->unk1c = cosf(angle) * arg3;
    spark->unk20 = sinf(angle) * arg3;
}

#undef _TAU_F
#undef _SQRT2_F


/**
 * Address: 7F0A3E1C
 */
s_bullet_spark *bullet_spark_create(coord3d *arg0, s32 arg1, f32 arg2, s16 arg3)
{
    s_bullet_spark *ptr;

    for (ptr = &g_BulletSparkArray[0]; ptr < &g_BulletSparkArray[BULLET_SPARKS_MAX]; ptr++)
    {
        if (ptr->lifetime == 0)
        {
            bullet_sparks_init(ptr, arg0, arg1, arg2, arg3);
            return ptr;
        }
    }

    return NULL;
}


/**
 * Address: 7F0A3EA0
 */
void bullet_sparks_update(void)
{
    s_bullet_spark *thing = &g_BulletSparkArray[0]; \
    s_bullet_spark *end = g_BulletSparkArray + BULLET_SPARKS_MAX;

    for (; thing < end; thing++)
    {
        if (thing->lifetime > 0)
        {
            thing->age += g_ClockTimer;

            if (thing->age >= 0 && thing->age >= thing->lifetime)
            {
                thing->lifetime = 0;
            } 
        }
    }
}


/**
 * Address: 7F0A3F04
 */
#ifdef NONMATCHING
void bullet_spark_render(void) {

}
#else
void bullet_spark_render(s_bullet_spark *arg0, Gfx *arg1, s32 arg2);
GLOBAL_ASM(
.text
glabel bullet_spark_render
/* 0D8A34 7F0A3F04 27BDFF48 */  addiu $sp, $sp, -0xb8
/* 0D8A38 7F0A3F08 AFBF002C */  sw    $ra, 0x2c($sp)
/* 0D8A3C 7F0A3F0C AFB20028 */  sw    $s2, 0x28($sp)
/* 0D8A40 7F0A3F10 AFB10024 */  sw    $s1, 0x24($sp)
/* 0D8A44 7F0A3F14 AFB00020 */  sw    $s0, 0x20($sp)
/* 0D8A48 7F0A3F18 AFA500BC */  sw    $a1, 0xbc($sp)
/* 0D8A4C 7F0A3F1C AFA600C0 */  sw    $a2, 0xc0($sp)
/* 0D8A50 7F0A3F20 848E0004 */  lh    $t6, 4($a0)
/* 0D8A54 7F0A3F24 00808025 */  move  $s0, $a0
/* 0D8A58 7F0A3F28 59C0017A */  blezl $t6, .L7F0A4514
/* 0D8A5C 7F0A3F2C 8FBF002C */   lw    $ra, 0x2c($sp)
/* 0D8A60 7F0A3F30 8C8F0000 */  lw    $t7, ($a0)
/* 0D8A64 7F0A3F34 24840010 */  addiu $a0, $a0, 0x10
/* 0D8A68 7F0A3F38 05E20176 */  bltzl $t7, .L7F0A4514
/* 0D8A6C 7F0A3F3C 8FBF002C */   lw    $ra, 0x2c($sp)
/* 0D8A70 7F0A3F40 0FC1E296 */  jal   camIsPosInScreen
/* 0D8A74 7F0A3F44 8E050024 */   lw    $a1, 0x24($s0)
/* 0D8A78 7F0A3F48 10400171 */  beqz  $v0, .L7F0A4510
/* 0D8A7C 7F0A3F4C 3C198004 */   lui   $t9, %hi(D_80040980)
/* 0D8A80 7F0A3F50 27390980 */  addiu $t9, %lo(D_80040980) # addiu $t9, $t9, 0x980
/* 0D8A84 7F0A3F54 8F210000 */  lw    $at, ($t9)
/* 0D8A88 7F0A3F58 8F290004 */  lw    $t1, 4($t9)
/* 0D8A8C 7F0A3F5C 27B800A8 */  addiu $t8, $sp, 0xa8
/* 0D8A90 7F0A3F60 AF010000 */  sw    $at, ($t8)
/* 0D8A94 7F0A3F64 AF090004 */  sw    $t1, 4($t8)
/* 0D8A98 7F0A3F68 8F29000C */  lw    $t1, 0xc($t9)
/* 0D8A9C 7F0A3F6C 8F210008 */  lw    $at, 8($t9)
/* 0D8AA0 7F0A3F70 AF09000C */  sw    $t1, 0xc($t8)
/* 0D8AA4 7F0A3F74 0FC1E111 */  jal   currentPlayerGetViewToWorldMtxf
/* 0D8AA8 7F0A3F78 AF010008 */   sw    $at, 8($t8)
/* 0D8AAC 7F0A3F7C 8FAA00BC */  lw    $t2, 0xbc($sp)
/* 0D8AB0 7F0A3F80 24040004 */  li    $a0, 4
/* 0D8AB4 7F0A3F84 8D4B0000 */  lw    $t3, ($t2)
/* 0D8AB8 7F0A3F88 AFA200A4 */  sw    $v0, 0xa4($sp)
/* 0D8ABC 7F0A3F8C 0FC2F5B1 */  jal   dynAllocateVertices
/* 0D8AC0 7F0A3F90 AFAB00A0 */   sw    $t3, 0xa0($sp)
/* 0D8AC4 7F0A3F94 86040006 */  lh    $a0, 6($s0)
/* 0D8AC8 7F0A3F98 00408825 */  move  $s1, $v0
/* 0D8ACC 7F0A3F9C 0FC2F285 */  jal   getRoomPositionByIndex
/* 0D8AD0 7F0A3FA0 AFA40058 */   sw    $a0, 0x58($sp)
/* 0D8AD4 7F0A3FA4 920C0028 */  lbu   $t4, 0x28($s0)
/* 0D8AD8 7F0A3FA8 8FA300A4 */  lw    $v1, 0xa4($sp)
/* 0D8ADC 7F0A3FAC 00409025 */  move  $s2, $v0
/* 0D8AE0 7F0A3FB0 A3AC00B4 */  sb    $t4, 0xb4($sp)
/* 0D8AE4 7F0A3FB4 920D0029 */  lbu   $t5, 0x29($s0)
/* 0D8AE8 7F0A3FB8 27A200A8 */  addiu $v0, $sp, 0xa8
/* 0D8AEC 7F0A3FBC A3AD00B5 */  sb    $t5, 0xb5($sp)
/* 0D8AF0 7F0A3FC0 920E002A */  lbu   $t6, 0x2a($s0)
/* 0D8AF4 7F0A3FC4 A3AE00B6 */  sb    $t6, 0xb6($sp)
/* 0D8AF8 7F0A3FC8 920F002B */  lbu   $t7, 0x2b($s0)
/* 0D8AFC 7F0A3FCC A3AF00B7 */  sb    $t7, 0xb7($sp)
/* 0D8B00 7F0A3FD0 8E080000 */  lw    $t0, ($s0)
/* 0D8B04 7F0A3FD4 C6080008 */  lwc1  $f8, 8($s0)
/* 0D8B08 7F0A3FD8 44882000 */  mtc1  $t0, $f4
/* 0D8B0C 7F0A3FDC 00000000 */  nop
/* 0D8B10 7F0A3FE0 468021A0 */  cvt.s.w $f6, $f4
/* 0D8B14 7F0A3FE4 46083282 */  mul.s $f10, $f6, $f8
/* 0D8B18 7F0A3FE8 4600540D */  trunc.w.s $f16, $f10
/* 0D8B1C 7F0A3FEC 44198000 */  mfc1  $t9, $f16
/* 0D8B20 7F0A3FF0 00000000 */  nop
/* 0D8B24 7F0A3FF4 AFB9005C */  sw    $t9, 0x5c($sp)
/* 0D8B28 7F0A3FF8 C6120010 */  lwc1  $f18, 0x10($s0)
/* 0D8B2C 7F0A3FFC E7B20090 */  swc1  $f18, 0x90($sp)
/* 0D8B30 7F0A4000 C6040014 */  lwc1  $f4, 0x14($s0)
/* 0D8B34 7F0A4004 E7A40094 */  swc1  $f4, 0x94($sp)
/* 0D8B38 7F0A4008 C6060018 */  lwc1  $f6, 0x18($s0)
/* 0D8B3C 7F0A400C E7A60098 */  swc1  $f6, 0x98($sp)
/* 0D8B40 7F0A4010 C60A001C */  lwc1  $f10, 0x1c($s0)
/* 0D8B44 7F0A4014 C4680000 */  lwc1  $f8, ($v1)
/* 0D8B48 7F0A4018 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8B4C 7F0A401C E7B00084 */  swc1  $f16, 0x84($sp)
/* 0D8B50 7F0A4020 C604001C */  lwc1  $f4, 0x1c($s0)
/* 0D8B54 7F0A4024 C4720004 */  lwc1  $f18, 4($v1)
/* 0D8B58 7F0A4028 46049182 */  mul.s $f6, $f18, $f4
/* 0D8B5C 7F0A402C E7A60088 */  swc1  $f6, 0x88($sp)
/* 0D8B60 7F0A4030 C60A001C */  lwc1  $f10, 0x1c($s0)
/* 0D8B64 7F0A4034 C4680008 */  lwc1  $f8, 8($v1)
/* 0D8B68 7F0A4038 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8B6C 7F0A403C E7B0008C */  swc1  $f16, 0x8c($sp)
/* 0D8B70 7F0A4040 C6040020 */  lwc1  $f4, 0x20($s0)
/* 0D8B74 7F0A4044 C4720000 */  lwc1  $f18, ($v1)
/* 0D8B78 7F0A4048 46049182 */  mul.s $f6, $f18, $f4
/* 0D8B7C 7F0A404C E7A60078 */  swc1  $f6, 0x78($sp)
/* 0D8B80 7F0A4050 C60A0020 */  lwc1  $f10, 0x20($s0)
/* 0D8B84 7F0A4054 C4680004 */  lwc1  $f8, 4($v1)
/* 0D8B88 7F0A4058 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8B8C 7F0A405C E7B0007C */  swc1  $f16, 0x7c($sp)
/* 0D8B90 7F0A4060 C6040020 */  lwc1  $f4, 0x20($s0)
/* 0D8B94 7F0A4064 C4720008 */  lwc1  $f18, 8($v1)
/* 0D8B98 7F0A4068 46049182 */  mul.s $f6, $f18, $f4
/* 0D8B9C 7F0A406C E7A60080 */  swc1  $f6, 0x80($sp)
/* 0D8BA0 7F0A4070 C60A001C */  lwc1  $f10, 0x1c($s0)
/* 0D8BA4 7F0A4074 C4680010 */  lwc1  $f8, 0x10($v1)
/* 0D8BA8 7F0A4078 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8BAC 7F0A407C E7B0006C */  swc1  $f16, 0x6c($sp)
/* 0D8BB0 7F0A4080 C604001C */  lwc1  $f4, 0x1c($s0)
/* 0D8BB4 7F0A4084 C4720014 */  lwc1  $f18, 0x14($v1)
/* 0D8BB8 7F0A4088 46049182 */  mul.s $f6, $f18, $f4
/* 0D8BBC 7F0A408C E7A60070 */  swc1  $f6, 0x70($sp)
/* 0D8BC0 7F0A4090 C60A001C */  lwc1  $f10, 0x1c($s0)
/* 0D8BC4 7F0A4094 C4680018 */  lwc1  $f8, 0x18($v1)
/* 0D8BC8 7F0A4098 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8BCC 7F0A409C E7B00074 */  swc1  $f16, 0x74($sp)
/* 0D8BD0 7F0A40A0 C6040020 */  lwc1  $f4, 0x20($s0)
/* 0D8BD4 7F0A40A4 C4720010 */  lwc1  $f18, 0x10($v1)
/* 0D8BD8 7F0A40A8 46049182 */  mul.s $f6, $f18, $f4
/* 0D8BDC 7F0A40AC E7A60060 */  swc1  $f6, 0x60($sp)
/* 0D8BE0 7F0A40B0 C60A0020 */  lwc1  $f10, 0x20($s0)
/* 0D8BE4 7F0A40B4 C4680014 */  lwc1  $f8, 0x14($v1)
/* 0D8BE8 7F0A40B8 460A4402 */  mul.s $f16, $f8, $f10
/* 0D8BEC 7F0A40BC E7B00064 */  swc1  $f16, 0x64($sp)
/* 0D8BF0 7F0A40C0 C6040020 */  lwc1  $f4, 0x20($s0)
/* 0D8BF4 7F0A40C4 C4720018 */  lwc1  $f18, 0x18($v1)
/* 0D8BF8 7F0A40C8 46049182 */  mul.s $f6, $f18, $f4
/* 0D8BFC 7F0A40CC E7A60068 */  swc1  $f6, 0x68($sp)
/* 0D8C00 7F0A40D0 8C410000 */  lw    $at, ($v0)
/* 0D8C04 7F0A40D4 AE210000 */  sw    $at, ($s1)
/* 0D8C08 7F0A40D8 8C4B0004 */  lw    $t3, 4($v0)
/* 0D8C0C 7F0A40DC AE2B0004 */  sw    $t3, 4($s1)
/* 0D8C10 7F0A40E0 8C410008 */  lw    $at, 8($v0)
/* 0D8C14 7F0A40E4 AE210008 */  sw    $at, 8($s1)
/* 0D8C18 7F0A40E8 8C4B000C */  lw    $t3, 0xc($v0)
/* 0D8C1C 7F0A40EC AE2B000C */  sw    $t3, 0xc($s1)
/* 0D8C20 7F0A40F0 8C410000 */  lw    $at, ($v0)
/* 0D8C24 7F0A40F4 AE210010 */  sw    $at, 0x10($s1)
/* 0D8C28 7F0A40F8 8C4E0004 */  lw    $t6, 4($v0)
/* 0D8C2C 7F0A40FC AE2E0014 */  sw    $t6, 0x14($s1)
/* 0D8C30 7F0A4100 8C410008 */  lw    $at, 8($v0)
/* 0D8C34 7F0A4104 AE210018 */  sw    $at, 0x18($s1)
/* 0D8C38 7F0A4108 8C4E000C */  lw    $t6, 0xc($v0)
/* 0D8C3C 7F0A410C AE2E001C */  sw    $t6, 0x1c($s1)
/* 0D8C40 7F0A4110 8C410000 */  lw    $at, ($v0)
/* 0D8C44 7F0A4114 AE210020 */  sw    $at, 0x20($s1)
/* 0D8C48 7F0A4118 8C580004 */  lw    $t8, 4($v0)
/* 0D8C4C 7F0A411C AE380024 */  sw    $t8, 0x24($s1)
/* 0D8C50 7F0A4120 8C410008 */  lw    $at, 8($v0)
/* 0D8C54 7F0A4124 AE210028 */  sw    $at, 0x28($s1)
/* 0D8C58 7F0A4128 8C58000C */  lw    $t8, 0xc($v0)
/* 0D8C5C 7F0A412C AE38002C */  sw    $t8, 0x2c($s1)
/* 0D8C60 7F0A4130 8C410000 */  lw    $at, ($v0)
/* 0D8C64 7F0A4134 AE210030 */  sw    $at, 0x30($s1)
/* 0D8C68 7F0A4138 8C490004 */  lw    $t1, 4($v0)
/* 0D8C6C 7F0A413C AE290034 */  sw    $t1, 0x34($s1)
/* 0D8C70 7F0A4140 8C410008 */  lw    $at, 8($v0)
/* 0D8C74 7F0A4144 AE210038 */  sw    $at, 0x38($s1)
/* 0D8C78 7F0A4148 8C49000C */  lw    $t1, 0xc($v0)
/* 0D8C7C 7F0A414C 0FC2D20F */  jal   get_room_data_float1
/* 0D8C80 7F0A4150 AE29003C */   sw    $t1, 0x3c($s1)
/* 0D8C84 7F0A4154 C7A80090 */  lwc1  $f8, 0x90($sp)
/* 0D8C88 7F0A4158 C7AA0084 */  lwc1  $f10, 0x84($sp)
/* 0D8C8C 7F0A415C C7B20060 */  lwc1  $f18, 0x60($sp)
/* 0D8C90 7F0A4160 460A4401 */  sub.s $f16, $f8, $f10
/* 0D8C94 7F0A4164 C6480000 */  lwc1  $f8, ($s2)
/* 0D8C98 7F0A4168 46128101 */  sub.s $f4, $f16, $f18
/* 0D8C9C 7F0A416C 46002182 */  mul.s $f6, $f4, $f0
/* 0D8CA0 7F0A4170 46083281 */  sub.s $f10, $f6, $f8
/* 0D8CA4 7F0A4174 4600540D */  trunc.w.s $f16, $f10
/* 0D8CA8 7F0A4178 440D8000 */  mfc1  $t5, $f16
/* 0D8CAC 7F0A417C 0FC2D20F */  jal   get_room_data_float1
/* 0D8CB0 7F0A4180 A62D0000 */   sh    $t5, ($s1)
/* 0D8CB4 7F0A4184 C7B20094 */  lwc1  $f18, 0x94($sp)
/* 0D8CB8 7F0A4188 C7A40088 */  lwc1  $f4, 0x88($sp)
/* 0D8CBC 7F0A418C C7A80064 */  lwc1  $f8, 0x64($sp)
/* 0D8CC0 7F0A4190 46049181 */  sub.s $f6, $f18, $f4
/* 0D8CC4 7F0A4194 C6520004 */  lwc1  $f18, 4($s2)
/* 0D8CC8 7F0A4198 46083281 */  sub.s $f10, $f6, $f8
/* 0D8CCC 7F0A419C 46005402 */  mul.s $f16, $f10, $f0
/* 0D8CD0 7F0A41A0 46128101 */  sub.s $f4, $f16, $f18
/* 0D8CD4 7F0A41A4 4600218D */  trunc.w.s $f6, $f4
/* 0D8CD8 7F0A41A8 440E3000 */  mfc1  $t6, $f6
/* 0D8CDC 7F0A41AC 0FC2D20F */  jal   get_room_data_float1
/* 0D8CE0 7F0A41B0 A62E0002 */   sh    $t6, 2($s1)
/* 0D8CE4 7F0A41B4 C7A80098 */  lwc1  $f8, 0x98($sp)
/* 0D8CE8 7F0A41B8 C7AA008C */  lwc1  $f10, 0x8c($sp)
/* 0D8CEC 7F0A41BC C7B20068 */  lwc1  $f18, 0x68($sp)
/* 0D8CF0 7F0A41C0 460A4401 */  sub.s $f16, $f8, $f10
/* 0D8CF4 7F0A41C4 C6480008 */  lwc1  $f8, 8($s2)
/* 0D8CF8 7F0A41C8 46128101 */  sub.s $f4, $f16, $f18
/* 0D8CFC 7F0A41CC 46002182 */  mul.s $f6, $f4, $f0
/* 0D8D00 7F0A41D0 46083281 */  sub.s $f10, $f6, $f8
/* 0D8D04 7F0A41D4 4600540D */  trunc.w.s $f16, $f10
/* 0D8D08 7F0A41D8 440F8000 */  mfc1  $t7, $f16
/* 0D8D0C 7F0A41DC 00000000 */  nop
/* 0D8D10 7F0A41E0 A62F0004 */  sh    $t7, 4($s1)
/* 0D8D14 7F0A41E4 8FA2005C */  lw    $v0, 0x5c($sp)
/* 0D8D18 7F0A41E8 8E0A000C */  lw    $t2, 0xc($s0)
/* 0D8D1C 7F0A41EC 0002C080 */  sll   $t8, $v0, 2
/* 0D8D20 7F0A41F0 0302C023 */  subu  $t8, $t8, $v0
/* 0D8D24 7F0A41F4 0018C080 */  sll   $t8, $t8, 2
/* 0D8D28 7F0A41F8 0158C821 */  addu  $t9, $t2, $t8
/* 0D8D2C 7F0A41FC 93290004 */  lbu   $t1, 4($t9)
/* 0D8D30 7F0A4200 A620000A */  sh    $zero, 0xa($s1)
/* 0D8D34 7F0A4204 00095940 */  sll   $t3, $t1, 5
/* 0D8D38 7F0A4208 A62B0008 */  sh    $t3, 8($s1)
/* 0D8D3C 7F0A420C 0FC2D20F */  jal   get_room_data_float1
/* 0D8D40 7F0A4210 AFB80034 */   sw    $t8, 0x34($sp)
/* 0D8D44 7F0A4214 C7B20090 */  lwc1  $f18, 0x90($sp)
/* 0D8D48 7F0A4218 C7A40078 */  lwc1  $f4, 0x78($sp)
/* 0D8D4C 7F0A421C C7A8006C */  lwc1  $f8, 0x6c($sp)
/* 0D8D50 7F0A4220 46049180 */  add.s $f6, $f18, $f4
/* 0D8D54 7F0A4224 C6520000 */  lwc1  $f18, ($s2)
/* 0D8D58 7F0A4228 46083281 */  sub.s $f10, $f6, $f8
/* 0D8D5C 7F0A422C 46005402 */  mul.s $f16, $f10, $f0
/* 0D8D60 7F0A4230 46128101 */  sub.s $f4, $f16, $f18
/* 0D8D64 7F0A4234 4600218D */  trunc.w.s $f6, $f4
/* 0D8D68 7F0A4238 440C3000 */  mfc1  $t4, $f6
/* 0D8D6C 7F0A423C 0FC2D20F */  jal   get_room_data_float1
/* 0D8D70 7F0A4240 A62C0010 */   sh    $t4, 0x10($s1)
/* 0D8D74 7F0A4244 C7A80094 */  lwc1  $f8, 0x94($sp)
/* 0D8D78 7F0A4248 C7AA007C */  lwc1  $f10, 0x7c($sp)
/* 0D8D7C 7F0A424C C7B20070 */  lwc1  $f18, 0x70($sp)
/* 0D8D80 7F0A4250 460A4400 */  add.s $f16, $f8, $f10
/* 0D8D84 7F0A4254 C6480004 */  lwc1  $f8, 4($s2)
/* 0D8D88 7F0A4258 46128101 */  sub.s $f4, $f16, $f18
/* 0D8D8C 7F0A425C 46002182 */  mul.s $f6, $f4, $f0
/* 0D8D90 7F0A4260 46083281 */  sub.s $f10, $f6, $f8
/* 0D8D94 7F0A4264 4600540D */  trunc.w.s $f16, $f10
/* 0D8D98 7F0A4268 44088000 */  mfc1  $t0, $f16
/* 0D8D9C 7F0A426C 0FC2D20F */  jal   get_room_data_float1
/* 0D8DA0 7F0A4270 A6280012 */   sh    $t0, 0x12($s1)
/* 0D8DA4 7F0A4274 C7B20098 */  lwc1  $f18, 0x98($sp)
/* 0D8DA8 7F0A4278 C7A40080 */  lwc1  $f4, 0x80($sp)
/* 0D8DAC 7F0A427C C7A80074 */  lwc1  $f8, 0x74($sp)
/* 0D8DB0 7F0A4280 46049180 */  add.s $f6, $f18, $f4
/* 0D8DB4 7F0A4284 C6520008 */  lwc1  $f18, 8($s2)
/* 0D8DB8 7F0A4288 A6200018 */  sh    $zero, 0x18($s1)
/* 0D8DBC 7F0A428C A620001A */  sh    $zero, 0x1a($s1)
/* 0D8DC0 7F0A4290 46083281 */  sub.s $f10, $f6, $f8
/* 0D8DC4 7F0A4294 46005402 */  mul.s $f16, $f10, $f0
/* 0D8DC8 7F0A4298 46128101 */  sub.s $f4, $f16, $f18
/* 0D8DCC 7F0A429C 4600218D */  trunc.w.s $f6, $f4
/* 0D8DD0 7F0A42A0 44183000 */  mfc1  $t8, $f6
/* 0D8DD4 7F0A42A4 0FC2D20F */  jal   get_room_data_float1
/* 0D8DD8 7F0A42A8 A6380014 */   sh    $t8, 0x14($s1)
/* 0D8DDC 7F0A42AC C7A80090 */  lwc1  $f8, 0x90($sp)
/* 0D8DE0 7F0A42B0 C7AA0084 */  lwc1  $f10, 0x84($sp)
/* 0D8DE4 7F0A42B4 C7B20060 */  lwc1  $f18, 0x60($sp)
/* 0D8DE8 7F0A42B8 460A4400 */  add.s $f16, $f8, $f10
/* 0D8DEC 7F0A42BC C6480000 */  lwc1  $f8, ($s2)
/* 0D8DF0 7F0A42C0 46128100 */  add.s $f4, $f16, $f18
/* 0D8DF4 7F0A42C4 46002182 */  mul.s $f6, $f4, $f0
/* 0D8DF8 7F0A42C8 46083281 */  sub.s $f10, $f6, $f8
/* 0D8DFC 7F0A42CC 4600540D */  trunc.w.s $f16, $f10
/* 0D8E00 7F0A42D0 44198000 */  mfc1  $t9, $f16
/* 0D8E04 7F0A42D4 0FC2D20F */  jal   get_room_data_float1
/* 0D8E08 7F0A42D8 A6390020 */   sh    $t9, 0x20($s1)
/* 0D8E0C 7F0A42DC C7B20094 */  lwc1  $f18, 0x94($sp)
/* 0D8E10 7F0A42E0 C7A40088 */  lwc1  $f4, 0x88($sp)
/* 0D8E14 7F0A42E4 C7A80064 */  lwc1  $f8, 0x64($sp)
/* 0D8E18 7F0A42E8 46049180 */  add.s $f6, $f18, $f4
/* 0D8E1C 7F0A42EC C6520004 */  lwc1  $f18, 4($s2)
/* 0D8E20 7F0A42F0 46083280 */  add.s $f10, $f6, $f8
/* 0D8E24 7F0A42F4 46005402 */  mul.s $f16, $f10, $f0
/* 0D8E28 7F0A42F8 46128101 */  sub.s $f4, $f16, $f18
/* 0D8E2C 7F0A42FC 4600218D */  trunc.w.s $f6, $f4
/* 0D8E30 7F0A4300 440B3000 */  mfc1  $t3, $f6
/* 0D8E34 7F0A4304 0FC2D20F */  jal   get_room_data_float1
/* 0D8E38 7F0A4308 A62B0022 */   sh    $t3, 0x22($s1)
/* 0D8E3C 7F0A430C C7A80098 */  lwc1  $f8, 0x98($sp)
/* 0D8E40 7F0A4310 C7AA008C */  lwc1  $f10, 0x8c($sp)
/* 0D8E44 7F0A4314 C7B20068 */  lwc1  $f18, 0x68($sp)
/* 0D8E48 7F0A4318 460A4400 */  add.s $f16, $f8, $f10
/* 0D8E4C 7F0A431C C6480008 */  lwc1  $f8, 8($s2)
/* 0D8E50 7F0A4320 A6200028 */  sh    $zero, 0x28($s1)
/* 0D8E54 7F0A4324 46128100 */  add.s $f4, $f16, $f18
/* 0D8E58 7F0A4328 46002182 */  mul.s $f6, $f4, $f0
/* 0D8E5C 7F0A432C 46083281 */  sub.s $f10, $f6, $f8
/* 0D8E60 7F0A4330 4600540D */  trunc.w.s $f16, $f10
/* 0D8E64 7F0A4334 440C8000 */  mfc1  $t4, $f16
/* 0D8E68 7F0A4338 00000000 */  nop
/* 0D8E6C 7F0A433C A62C0024 */  sh    $t4, 0x24($s1)
/* 0D8E70 7F0A4340 8FA80034 */  lw    $t0, 0x34($sp)
/* 0D8E74 7F0A4344 8E0E000C */  lw    $t6, 0xc($s0)
/* 0D8E78 7F0A4348 01C87821 */  addu  $t7, $t6, $t0
/* 0D8E7C 7F0A434C 91F80005 */  lbu   $t8, 5($t7)
/* 0D8E80 7F0A4350 00185140 */  sll   $t2, $t8, 5
/* 0D8E84 7F0A4354 0FC2D20F */  jal   get_room_data_float1
/* 0D8E88 7F0A4358 A62A002A */   sh    $t2, 0x2a($s1)
/* 0D8E8C 7F0A435C C7B20090 */  lwc1  $f18, 0x90($sp)
/* 0D8E90 7F0A4360 C7A40078 */  lwc1  $f4, 0x78($sp)
/* 0D8E94 7F0A4364 C7A8006C */  lwc1  $f8, 0x6c($sp)
/* 0D8E98 7F0A4368 46049181 */  sub.s $f6, $f18, $f4
/* 0D8E9C 7F0A436C C6520000 */  lwc1  $f18, ($s2)
/* 0D8EA0 7F0A4370 46083280 */  add.s $f10, $f6, $f8
/* 0D8EA4 7F0A4374 46005402 */  mul.s $f16, $f10, $f0
/* 0D8EA8 7F0A4378 46128101 */  sub.s $f4, $f16, $f18
/* 0D8EAC 7F0A437C 4600218D */  trunc.w.s $f6, $f4
/* 0D8EB0 7F0A4380 44093000 */  mfc1  $t1, $f6
/* 0D8EB4 7F0A4384 0FC2D20F */  jal   get_room_data_float1
/* 0D8EB8 7F0A4388 A6290030 */   sh    $t1, 0x30($s1)
/* 0D8EBC 7F0A438C C7A80094 */  lwc1  $f8, 0x94($sp)
/* 0D8EC0 7F0A4390 C7AA007C */  lwc1  $f10, 0x7c($sp)
/* 0D8EC4 7F0A4394 C7B20070 */  lwc1  $f18, 0x70($sp)
/* 0D8EC8 7F0A4398 460A4401 */  sub.s $f16, $f8, $f10
/* 0D8ECC 7F0A439C C6480004 */  lwc1  $f8, 4($s2)
/* 0D8ED0 7F0A43A0 46128100 */  add.s $f4, $f16, $f18
/* 0D8ED4 7F0A43A4 46002182 */  mul.s $f6, $f4, $f0
/* 0D8ED8 7F0A43A8 46083281 */  sub.s $f10, $f6, $f8
/* 0D8EDC 7F0A43AC 4600540D */  trunc.w.s $f16, $f10
/* 0D8EE0 7F0A43B0 440D8000 */  mfc1  $t5, $f16
/* 0D8EE4 7F0A43B4 0FC2D20F */  jal   get_room_data_float1
/* 0D8EE8 7F0A43B8 A62D0032 */   sh    $t5, 0x32($s1)
/* 0D8EEC 7F0A43BC C7B20098 */  lwc1  $f18, 0x98($sp)
/* 0D8EF0 7F0A43C0 C7A40080 */  lwc1  $f4, 0x80($sp)
/* 0D8EF4 7F0A43C4 C7A80074 */  lwc1  $f8, 0x74($sp)
/* 0D8EF8 7F0A43C8 8FA40034 */  lw    $a0, 0x34($sp)
/* 0D8EFC 7F0A43CC 46049181 */  sub.s $f6, $f18, $f4
/* 0D8F00 7F0A43D0 C6520008 */  lwc1  $f18, 8($s2)
/* 0D8F04 7F0A43D4 46083280 */  add.s $f10, $f6, $f8
/* 0D8F08 7F0A43D8 46005402 */  mul.s $f16, $f10, $f0
/* 0D8F0C 7F0A43DC 46128101 */  sub.s $f4, $f16, $f18
/* 0D8F10 7F0A43E0 4600218D */  trunc.w.s $f6, $f4
/* 0D8F14 7F0A43E4 440E3000 */  mfc1  $t6, $f6
/* 0D8F18 7F0A43E8 00000000 */  nop
/* 0D8F1C 7F0A43EC A62E0034 */  sh    $t6, 0x34($s1)
/* 0D8F20 7F0A43F0 8E08000C */  lw    $t0, 0xc($s0)
/* 0D8F24 7F0A43F4 01047821 */  addu  $t7, $t0, $a0
/* 0D8F28 7F0A43F8 91F80004 */  lbu   $t8, 4($t7)
/* 0D8F2C 7F0A43FC 240F2000 */  li    $t7, 8192
/* 0D8F30 7F0A4400 3C08B700 */  lui   $t0, 0xb700
/* 0D8F34 7F0A4404 00185140 */  sll   $t2, $t8, 5
/* 0D8F38 7F0A4408 A62A0038 */  sh    $t2, 0x38($s1)
/* 0D8F3C 7F0A440C 8E19000C */  lw    $t9, 0xc($s0)
/* 0D8F40 7F0A4410 03244821 */  addu  $t1, $t9, $a0
/* 0D8F44 7F0A4414 912B0005 */  lbu   $t3, 5($t1)
/* 0D8F48 7F0A4418 3C190103 */  lui   $t9, (0x01030040 >> 16) # lui $t9, 0x103
/* 0D8F4C 7F0A441C 37390040 */  ori   $t9, (0x01030040 & 0xFFFF) # ori $t9, $t9, 0x40
/* 0D8F50 7F0A4420 000B6940 */  sll   $t5, $t3, 5
/* 0D8F54 7F0A4424 A62D003A */  sh    $t5, 0x3a($s1)
/* 0D8F58 7F0A4428 8FAC00A0 */  lw    $t4, 0xa0($sp)
/* 0D8F5C 7F0A442C 258E0008 */  addiu $t6, $t4, 8
/* 0D8F60 7F0A4430 AFAE00A0 */  sw    $t6, 0xa0($sp)
/* 0D8F64 7F0A4434 AD8F0004 */  sw    $t7, 4($t4)
/* 0D8F68 7F0A4438 AD880000 */  sw    $t0, ($t4)
/* 0D8F6C 7F0A443C 8FB800A0 */  lw    $t8, 0xa0($sp)
/* 0D8F70 7F0A4440 270A0008 */  addiu $t2, $t8, 8
/* 0D8F74 7F0A4444 AFAA00A0 */  sw    $t2, 0xa0($sp)
/* 0D8F78 7F0A4448 AF190000 */  sw    $t9, ($t8)
/* 0D8F7C 7F0A444C 0FC1E0E5 */  jal   get_BONDdata_field_10E0
/* 0D8F80 7F0A4450 AFB8004C */   sw    $t8, 0x4c($sp)
/* 0D8F84 7F0A4454 0C003A2C */  jal   osVirtualToPhysical
/* 0D8F88 7F0A4458 00402025 */   move  $a0, $v0
/* 0D8F8C 7F0A445C 8FA9004C */  lw    $t1, 0x4c($sp)
/* 0D8F90 7F0A4460 AD220004 */  sw    $v0, 4($t1)
/* 0D8F94 7F0A4464 8FA50058 */  lw    $a1, 0x58($sp)
/* 0D8F98 7F0A4468 0FC2F271 */  jal   applyRoomMatrixToDisplayList
/* 0D8F9C 7F0A446C 8FA400A0 */   lw    $a0, 0xa0($sp)
/* 0D8FA0 7F0A4470 AFA200A0 */  sw    $v0, 0xa0($sp)
/* 0D8FA4 7F0A4474 8E0B000C */  lw    $t3, 0xc($s0)
/* 0D8FA8 7F0A4478 8FAD0034 */  lw    $t5, 0x34($sp)
/* 0D8FAC 7F0A447C 240C0002 */  li    $t4, 2
/* 0D8FB0 7F0A4480 AFAC0010 */  sw    $t4, 0x10($sp)
/* 0D8FB4 7F0A4484 27A400A0 */  addiu $a0, $sp, 0xa0
/* 0D8FB8 7F0A4488 24060004 */  li    $a2, 4
/* 0D8FBC 7F0A448C 8FA700C0 */  lw    $a3, 0xc0($sp)
/* 0D8FC0 7F0A4490 0FC1DB5A */  jal   texSelect
/* 0D8FC4 7F0A4494 016D2821 */   addu  $a1, $t3, $t5
/* 0D8FC8 7F0A4498 8FB000A0 */  lw    $s0, 0xa0($sp)
/* 0D8FCC 7F0A449C 3C0F0430 */  lui   $t7, (0x04300040 >> 16) # lui $t7, 0x430
/* 0D8FD0 7F0A44A0 35EF0040 */  ori   $t7, (0x04300040 & 0xFFFF) # ori $t7, $t7, 0x40
/* 0D8FD4 7F0A44A4 26080008 */  addiu $t0, $s0, 8
/* 0D8FD8 7F0A44A8 AFA800A0 */  sw    $t0, 0xa0($sp)
/* 0D8FDC 7F0A44AC 02202025 */  move  $a0, $s1
/* 0D8FE0 7F0A44B0 0C003A2C */  jal   osVirtualToPhysical
/* 0D8FE4 7F0A44B4 AE0F0000 */   sw    $t7, ($s0)
/* 0D8FE8 7F0A44B8 AE020004 */  sw    $v0, 4($s0)
/* 0D8FEC 7F0A44BC 8FB800A0 */  lw    $t8, 0xa0($sp)
/* 0D8FF0 7F0A44C0 3C19B100 */  lui   $t9, (0xB1000032 >> 16) # lui $t9, 0xb100
/* 0D8FF4 7F0A44C4 37390032 */  ori   $t9, (0xB1000032 & 0xFFFF) # ori $t9, $t9, 0x32
/* 0D8FF8 7F0A44C8 270A0008 */  addiu $t2, $t8, 8
/* 0D8FFC 7F0A44CC AFAA00A0 */  sw    $t2, 0xa0($sp)
/* 0D9000 7F0A44D0 24092010 */  li    $t1, 8208
/* 0D9004 7F0A44D4 AF090004 */  sw    $t1, 4($t8)
/* 0D9008 7F0A44D8 AF190000 */  sw    $t9, ($t8)
/* 0D900C 7F0A44DC 8FB100A0 */  lw    $s1, 0xa0($sp)
/* 0D9010 7F0A44E0 3C0C0103 */  lui   $t4, (0x01030040 >> 16) # lui $t4, 0x103
/* 0D9014 7F0A44E4 358C0040 */  ori   $t4, (0x01030040 & 0xFFFF) # ori $t4, $t4, 0x40
/* 0D9018 7F0A44E8 262D0008 */  addiu $t5, $s1, 8
/* 0D901C 7F0A44EC AFAD00A0 */  sw    $t5, 0xa0($sp)
/* 0D9020 7F0A44F0 0FC1E0DD */  jal   currentPlayerGetProjectionMatrix
/* 0D9024 7F0A44F4 AE2C0000 */   sw    $t4, ($s1)
/* 0D9028 7F0A44F8 0C003A2C */  jal   osVirtualToPhysical
/* 0D902C 7F0A44FC 00402025 */   move  $a0, $v0
/* 0D9030 7F0A4500 AE220004 */  sw    $v0, 4($s1)
/* 0D9034 7F0A4504 8FA800BC */  lw    $t0, 0xbc($sp)
/* 0D9038 7F0A4508 8FAE00A0 */  lw    $t6, 0xa0($sp)
/* 0D903C 7F0A450C AD0E0000 */  sw    $t6, ($t0)
.L7F0A4510:
/* 0D9040 7F0A4510 8FBF002C */  lw    $ra, 0x2c($sp)
.L7F0A4514:
/* 0D9044 7F0A4514 8FB00020 */  lw    $s0, 0x20($sp)
/* 0D9048 7F0A4518 8FB10024 */  lw    $s1, 0x24($sp)
/* 0D904C 7F0A451C 8FB20028 */  lw    $s2, 0x28($sp)
/* 0D9050 7F0A4520 03E00008 */  jr    $ra
/* 0D9054 7F0A4524 27BD00B8 */   addiu $sp, $sp, 0xb8
)
#endif


/**
 * Address: 7F0A4528
 */
void bullet_sparks_render(Gfx *gdl, s32 arg1)
{

    s_bullet_spark *thing = &g_BulletSparkArray[0]; \
    s_bullet_spark *end = g_BulletSparkArray + BULLET_SPARKS_MAX;

    for (; (thing < end); thing++)
    {
        bullet_spark_render(thing, gdl, arg1);
    }
}


/**
 * Address: 7F0A4594
 */
f32 bullet_spark_get_depth(s_bullet_spark* spark)
{
    coord3d tempVec;

    tempVec.x = spark->unk10;
    tempVec.y = spark->unk14;
    tempVec.z = spark->unk18;

    mtx4TransformVecInPlace(camGetWorldToScreenMtxf(), &tempVec);

    return -tempVec.z;
}


/**
 * Address: 7F0A45D8
 */
#ifndef VERSION_EU
void bullet_moving_sparks_reset(void)
{
    s_moving_bullet_spark *ptr;

    ptr = g_MovingBulletSparkArray;

    for (ptr = &g_MovingBulletSparkArray[0]; ptr < &g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX]; ptr++)
    {
        ptr->unk00.lifetime = 0;
    }
}
#else
void bullet_moving_sparks_reset(void)
{
    bullet_sparks_reset();
}
#endif


/**
 * Address: 7F0A4600
 */
#ifndef VERSION_EU
s_moving_bullet_spark *bullet_moving_spark_create(coord3d *arg0, coord3d *arg1, s32 arg2, f32 arg3, f32 arg4, s16 arg5)
{
    s_moving_bullet_spark *ptr;

    for (ptr = &g_MovingBulletSparkArray[0]; ptr < &g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX]; ptr++)
    {
        if (ptr->unk00.lifetime == 0)
        {
            bullet_sparks_init(&ptr->unk00, arg0, arg2, arg3, arg5);

            ptr->velocity.x = arg1->x;
            ptr->velocity.y = arg1->y;
            ptr->velocity.z = arg1->z;
            ptr->unk38 = arg4;

            return ptr;
        }
    }

    return NULL;
}
#else
void bullet_moving_spark_create(void)
{
    bullet_sparks_update();
}
#endif


/**
 * Address: 7F0A46A0
 */
#ifndef VERSION_EU
void bullet_moving_sparks_update(void)
{
    s_moving_bullet_spark *ptr;
    s_moving_bullet_spark *end;

    ptr = &g_MovingBulletSparkArray[0]; end = &g_MovingBulletSparkArray[BULLET_MOVING_SPARKS_MAX];

    while (ptr < end)
    {
        if (ptr->unk00.lifetime > 0)
        {
            ptr->unk00.age += g_ClockTimer;
            if (ptr->unk00.age >= 0)
            {
                if (ptr->unk00.lifetime > ptr->unk00.age)
                {
                    sub_GAME_7F057D88(&ptr->unk00.unk10, &ptr->velocity, g_GlobalTimerDelta);
                    if (ptr->unk00.unk14 < ptr->unk38)
                    {
                        ptr->unk00.lifetime = 0;
                    }
                }
                else
                {
                    ptr->unk00.lifetime = 0;
                }
            }
        }
        ptr++;
    }
}
#else
void bullet_moving_sparks_update(Gfx *arg0, s32 arg1)
{
    bullet_sparks_render(arg0, arg1);
}
#endif


/**
 * Address: 7F0A4768
 */
#ifndef VERSION_EU

void bullet_moving_sparks_render_all(Gfx *arg0, s32 arg1)
{
    s32 max_index;
    s_moving_bullet_spark *ptr;

    max_index = BULLET_MOVING_SPARKS_MAX;

    for (ptr = &g_MovingBulletSparkArray[0]; ptr < (&g_MovingBulletSparkArray[max_index]); ptr++)
    {
        bullet_spark_render(&ptr->unk00, arg0, arg1);
    }

}


/**
 * Address: 7F0A47D4
 */
void bullet_sparks_reset_all(void)
{
    bullet_sparks_reset();
    bullet_moving_sparks_reset();
}


void bullet_sparks_update_all(void)
{
    bullet_sparks_update();

    // responsible for updating bullet sparks and dust clouds that spawn when shooting at other players
    // these are 2D and always facing the camera
    bullet_moving_sparks_update();
}


/**
 * Address: 7F0A4824
 */
void bullet_sparks_render_all(Gfx *arg0, s32 arg1)
{
    bullet_sparks_render(arg0, arg1);
    bullet_moving_sparks_render_all(arg0, arg1);
}


#endif


