#include <ultra64.h>
#include <memp.h>
#include "bondtypes.h"
#include "unk_09B7A0.h"
#include "propobj.h"
#include "model.h"

// unsure if these structs are defined as something else, elsewhere
struct unk_09B7A0_struct_child {
    s32 unk00;
    s32 unk04;
    s32 unk08;
    s32 unk0C;
};

// unsure if these structs are defined as something else, elsewhere
struct unk_09B7A0_struct_parent {
    struct unk_09B7A0_struct_child* unk00;
    s32 unk04;
    s32 unk08;
    s16 unk0C;
    s16 unk0E;
    s16 unk10;
    s16 unk12;
};

// bss
//CODE.bss:8007A0D0
s32 dword_CODE_bss_8007A0D0; // item count for dword_CODE_bss_8007A0E0
//CODE.bss:8007A0D4
s32 dword_CODE_bss_8007A0D4; // item count for dword_CODE_bss_8007A0E8
//CODE.bss:8007A0D8
s32 dword_CODE_bss_8007A0D8; // item count for dword_CODE_bss_8007A0E4
//CODE.bss:8007A0DC
s32 dword_CODE_bss_8007A0DC; // item count for dword_CODE_bss_8007A0EC
//CODE.bss:8007A0E0
struct unk_09B7A0_struct_parent* dword_CODE_bss_8007A0E0; // array ( uses dword_CODE_bss_8007A0D0 as alloc count, item size 0x10 )
//CODE.bss:8007A0E4
struct unk_09B7A0_struct_parent* dword_CODE_bss_8007A0E4; // array ( uses dword_CODE_bss_8007A0D8 as alloc count, item size 0x10 )
//CODE.bss:8007A0E8
struct unk_09B7A0_struct_parent* dword_CODE_bss_8007A0E8; // array ( uses dword_CODE_bss_8007A0D4 as alloc count, item size 0x14 )
//CODE.bss:8007A0EC
struct unk_09B7A0_struct_parent* dword_CODE_bss_8007A0EC; // array ( uses dword_CODE_bss_8007A0DC as alloc count, item size 0x14 )
//CODE.bss:8007A0F0
s16 word_CODE_bss_8007A0F0;
//CODE.bss:8007A0F2
s16 word_CODE_bss_8007A0F2;

void null_init_main_1(void)
{
    return;
}


void sub_GAME_7F09B7A8(void)
{
    s32 i;
    for (i = 0; i < dword_CODE_bss_8007A0D4; i++)
    {
        if (dword_CODE_bss_8007A0E8[i].unk00);
    }
}

void sub_GAME_7F09B7E4(void)
{
    s32 i;
    for (i = 0; i < dword_CODE_bss_8007A0DC; i++)
    {
        if (dword_CODE_bss_8007A0EC[i].unk00);
    }
}

void sub_GAME_7F09B820(void)
{
    u32 tmp;
    s32 stage;
    s32 i;

    tmp = 0x5DC;

    if (getPlayerCount() >= 2)
    {
        dword_CODE_bss_8007A0D0 = 0xBB8;
        dword_CODE_bss_8007A0D4 = 0x50;
        dword_CODE_bss_8007A0D8 = 0x1F4;
        dword_CODE_bss_8007A0DC = 0x14;
    }
    else
    {
        stage = lvlGetCurrentStageToLoad();
        if ((stage != 0x1E) && (stage != 0x1D))
        {
            dword_CODE_bss_8007A0D0 = 0x1F4;
            dword_CODE_bss_8007A0D4 = 0x14;
            dword_CODE_bss_8007A0D8 = tmp;
            dword_CODE_bss_8007A0DC = 0x28;
        }
        else
        {
            dword_CODE_bss_8007A0D0 = 0x1F4;
            dword_CODE_bss_8007A0D4 = 0x14;
            dword_CODE_bss_8007A0D8 = 0x1F4;
            dword_CODE_bss_8007A0DC = 0x14;
        }
    }

    tmp = 0x14;
    dword_CODE_bss_8007A0E8 = mempAllocBytesInBank(dword_CODE_bss_8007A0D4 * tmp, MEMPOOL_STAGE);
    dword_CODE_bss_8007A0E0 = mempAllocBytesInBank(dword_CODE_bss_8007A0D0 * 0x10, MEMPOOL_STAGE);
    dword_CODE_bss_8007A0EC = mempAllocBytesInBank(dword_CODE_bss_8007A0DC * tmp, MEMPOOL_STAGE);
    dword_CODE_bss_8007A0E4 = mempAllocBytesInBank(dword_CODE_bss_8007A0D8 * 0x10, MEMPOOL_STAGE);

    word_CODE_bss_8007A0F0 = (s16) dword_CODE_bss_8007A0D0;
    dword_CODE_bss_8007A0E8->unk00 = (struct unk_09B7A0_struct_child *) dword_CODE_bss_8007A0E0;
    dword_CODE_bss_8007A0E8->unk0C = (s16) dword_CODE_bss_8007A0D0;
    dword_CODE_bss_8007A0E8->unk0E = 0;
    dword_CODE_bss_8007A0E8->unk10 = -1;
    dword_CODE_bss_8007A0E8->unk12 = -1;

    for (i = 1; i < dword_CODE_bss_8007A0D4; i++)
    {
        dword_CODE_bss_8007A0E8[i].unk0E = -1;
    }

    word_CODE_bss_8007A0F2 = (s16) dword_CODE_bss_8007A0D8;
    dword_CODE_bss_8007A0EC->unk00 = (struct unk_09B7A0_struct_child *) dword_CODE_bss_8007A0E4;
    dword_CODE_bss_8007A0EC->unk0C = (s16) dword_CODE_bss_8007A0D8;
    dword_CODE_bss_8007A0EC->unk0E = 0;
    dword_CODE_bss_8007A0EC->unk10 = -1;
    dword_CODE_bss_8007A0EC->unk12 = -1;

    for (i = 1; i < dword_CODE_bss_8007A0DC; i++)
    {
        dword_CODE_bss_8007A0EC[i].unk0E = -1;
    }
}


/*
* Address: 0x7F09BAC4
*/
void sub_GAME_7F09BAC4(s32 arg0, s32 arg1) {
    PropRecord* var_s1;
    ChrRecord* var_v0;
    Model* temp_a0;
    s32* temp_v0_2;
    ModelNode* var_a1;
    ModelFileHeader* var_v1;
    s32 val;

    var_s1 = get_ptr_obj_pos_list_current_entry();
    while (var_s1 != NULL) {
        if (var_s1->type == 1) {
            var_v0 = var_s1->chr;
            var_v1 = ((Model*)var_v0->chrflags)->obj;
            var_a1 = var_v1->RootNode;
            while (var_a1 != NULL) {
                val = var_a1->Opcode & 0xFF;
                if (val == 0x18) {
                    temp_v0_2 = modelGetNodeRwData(var_v0->chrflags, var_a1);
                    if (arg0 == *temp_v0_2) {
                        *temp_v0_2 = arg1;
                    }
                    break;
                } else {
                    if (var_a1->Child != NULL) {
                        var_a1 = var_a1->Child;
                    } else {
                        while (var_a1 != NULL) {
                            if (var_a1->Next != NULL) {
                                var_a1 = var_a1->Next;
                                break;
                            }
                            var_a1 = var_a1->Parent;
                        }
                    }
                }
            }
        }
        var_s1 = var_s1->prev;
    }
}





/*
* Address: 7F09BBBC
*/
void sub_GAME_7F09BBBC(void)
{
    s16 temp_s2;
    s16 var_fp;
    s16 var_s2;
    s32 stop;
    s32 var_a1;
    s32 var_s6;

    var_s6 = 0;

    if (word_CODE_bss_8007A0F2 < ((s32)dword_CODE_bss_8007A0D8 >> 2))
    {
        for (var_fp = 0; var_fp < dword_CODE_bss_8007A0DC - 1; var_fp++) {
            if (dword_CODE_bss_8007A0EC[var_fp].unk0E > 0)
            {
                for (var_s2 = var_fp + 1; var_s2 < dword_CODE_bss_8007A0DC; var_s2++) {
                    if ((dword_CODE_bss_8007A0EC[var_s2].unk0E > 0) &&
                        (dword_CODE_bss_8007A0EC[var_fp].unk04 == dword_CODE_bss_8007A0EC[var_s2].unk04) &&
                        (dword_CODE_bss_8007A0EC[var_fp].unk08 == dword_CODE_bss_8007A0EC[var_s2].unk08))
                    {
                        sub_GAME_7F09BAC4((s32)dword_CODE_bss_8007A0EC[var_s2].unk00, (s32)dword_CODE_bss_8007A0EC[var_fp].unk00);
                        var_s6 = 1;

                        dword_CODE_bss_8007A0EC[var_fp].unk0E += dword_CODE_bss_8007A0EC[var_s2].unk0E;
                        dword_CODE_bss_8007A0EC[var_s2].unk0E = 0;
                        
                        word_CODE_bss_8007A0F2 += dword_CODE_bss_8007A0EC[var_s2].unk0C;
                    }
                }
            }
        }
    }

    if (var_s6 != 0) {
        stop  = 0;
        var_fp = 0;

        while (stop == 0) {
            var_s2 = dword_CODE_bss_8007A0EC[var_fp].unk10;

            if (var_s2 >= 0) {
                if (dword_CODE_bss_8007A0EC[var_fp].unk0E == 0) {
                    if (dword_CODE_bss_8007A0EC[var_s2].unk0E == 0) {
                        dword_CODE_bss_8007A0EC[var_fp].unk0C += dword_CODE_bss_8007A0EC[var_s2].unk0C;
                        dword_CODE_bss_8007A0EC[var_s2].unk0E = -1;
                        var_s2 = dword_CODE_bss_8007A0EC[var_s2].unk10;
                        dword_CODE_bss_8007A0EC[var_fp].unk10 = var_s2;

                        if (var_s2 >= 0) {
                            dword_CODE_bss_8007A0EC[var_s2].unk12 = var_fp;
                        }
                        continue;
                    }
                }
                var_fp = var_s2;
            } else {
                stop = 1;
            }
        }
    }

    if (word_CODE_bss_8007A0F2 < ((s32)dword_CODE_bss_8007A0D8 >> 2)) {
        sub_GAME_7F056690();
    }
}





#ifdef NONMATCHING
void sub_GAME_7F09BE4C(void) {

}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F09BE4C
/* 0D097C 7F09BE4C 27BDFFE0 */  addiu $sp, $sp, -0x20
/* 0D0980 7F09BE50 AFB00018 */  sw    $s0, 0x18($sp)
/* 0D0984 7F09BE54 24010B0B */  li    $at, 2827
/* 0D0988 7F09BE58 00808025 */  move  $s0, $a0
/* 0D098C 7F09BE5C AFBF001C */  sw    $ra, 0x1c($sp)
/* 0D0990 7F09BE60 AFA60028 */  sw    $a2, 0x28($sp)
/* 0D0994 7F09BE64 10A1000A */  beq   $a1, $at, .L7F09BE90
/* 0D0998 7F09BE68 AFA7002C */   sw    $a3, 0x2c($sp)
/* 0D099C 7F09BE6C 3401CCCC */  li    $at, 52428
/* 0D09A0 7F09BE70 14A1000E */  bne   $a1, $at, .L7F09BEAC
/* 0D09A4 7F09BE74 3C088008 */   lui   $t0, %hi(dword_CODE_bss_8007A0E8) 
/* 0D09A8 7F09BE78 3C0B8008 */  lui   $t3, %hi(word_CODE_bss_8007A0F0) 
/* 0D09AC 7F09BE7C 3C068008 */  lui   $a2, %hi(dword_CODE_bss_8007A0D4+2)
/* 0D09B0 7F09BE80 8D08A0E8 */  lw    $t0, %lo(dword_CODE_bss_8007A0E8)($t0)
/* 0D09B4 7F09BE84 256BA0F0 */  addiu $t3, %lo(word_CODE_bss_8007A0F0) # addiu $t3, $t3, -0x5f10
/* 0D09B8 7F09BE88 1000000A */  b     .L7F09BEB4
/* 0D09BC 7F09BE8C 84C6A0D6 */   lh    $a2, %lo(dword_CODE_bss_8007A0D4+2)($a2)
.L7F09BE90:
/* 0D09C0 7F09BE90 3C088008 */  lui   $t0, %hi(dword_CODE_bss_8007A0EC) 
/* 0D09C4 7F09BE94 3C0B8008 */  lui   $t3, %hi(word_CODE_bss_8007A0F2) 
/* 0D09C8 7F09BE98 3C068008 */  lui   $a2, %hi(dword_CODE_bss_8007A0DC+0x2)
/* 0D09CC 7F09BE9C 8D08A0EC */  lw    $t0, %lo(dword_CODE_bss_8007A0EC)($t0)
/* 0D09D0 7F09BEA0 256BA0F2 */  addiu $t3, %lo(word_CODE_bss_8007A0F2) # addiu $t3, $t3, -0x5f0e
/* 0D09D4 7F09BEA4 10000003 */  b     .L7F09BEB4
/* 0D09D8 7F09BEA8 84C6A0DE */   lh    $a2, %lo(dword_CODE_bss_8007A0DC+0x2)($a2)
.L7F09BEAC:
/* 0D09DC 7F09BEAC 10000060 */  b     .L7F09C030
/* 0D09E0 7F09BEB0 00001025 */   move  $v0, $zero
.L7F09BEB4:
/* 0D09E4 7F09BEB4 00001825 */  move  $v1, $zero
/* 0D09E8 7F09BEB8 00001025 */  move  $v0, $zero
/* 0D09EC 7F09BEBC 00002825 */  move  $a1, $zero
/* 0D09F0 7F09BEC0 24090014 */  li    $t1, 20
/* 0D09F4 7F09BEC4 2407FFFF */  li    $a3, -1
.L7F09BEC8:
/* 0D09F8 7F09BEC8 00A90019 */  multu $a1, $t1
/* 0D09FC 7F09BECC 00007012 */  mflo  $t6
/* 0D0A00 7F09BED0 010E2021 */  addu  $a0, $t0, $t6
/* 0D0A04 7F09BED4 848F000E */  lh    $t7, 0xe($a0)
/* 0D0A08 7F09BED8 55E00008 */  bnezl $t7, .L7F09BEFC
/* 0D0A0C 7F09BEDC 84850010 */   lh    $a1, 0x10($a0)
/* 0D0A10 7F09BEE0 8498000C */  lh    $t8, 0xc($a0)
/* 0D0A14 7F09BEE4 0310082A */  slt   $at, $t8, $s0
/* 0D0A18 7F09BEE8 54200004 */  bnezl $at, .L7F09BEFC
/* 0D0A1C 7F09BEEC 84850010 */   lh    $a1, 0x10($a0)
/* 0D0A20 7F09BEF0 1000000B */  b     .L7F09BF20
/* 0D0A24 7F09BEF4 24030001 */   li    $v1, 1
/* 0D0A28 7F09BEF8 84850010 */  lh    $a1, 0x10($a0)
.L7F09BEFC:
/* 0D0A2C 7F09BEFC 24420001 */  addiu $v0, $v0, 1
/* 0D0A30 7F09BF00 00C2082A */  slt   $at, $a2, $v0
/* 0D0A34 7F09BF04 50A70004 */  beql  $a1, $a3, .L7F09BF18
/* 0D0A38 7F09BF08 00071C00 */   sll   $v1, $a3, 0x10
/* 0D0A3C 7F09BF0C 10200004 */  beqz  $at, .L7F09BF20
/* 0D0A40 7F09BF10 00000000 */   nop   
/* 0D0A44 7F09BF14 00071C00 */  sll   $v1, $a3, 0x10
.L7F09BF18:
/* 0D0A48 7F09BF18 0003CC03 */  sra   $t9, $v1, 0x10
/* 0D0A4C 7F09BF1C 03201825 */  move  $v1, $t9
.L7F09BF20:
/* 0D0A50 7F09BF20 1060FFE9 */  beqz  $v1, .L7F09BEC8
/* 0D0A54 7F09BF24 00000000 */   nop   
/* 0D0A58 7F09BF28 00C2082A */  slt   $at, $a2, $v0
/* 0D0A5C 7F09BF2C 50200008 */  beql  $at, $zero, .L7F09BF50
/* 0D0A60 7F09BF30 24010001 */   li    $at, 1
/* 0D0A64 7F09BF34 0FC26DEA */  jal   sub_GAME_7F09B7A8
/* 0D0A68 7F09BF38 00000000 */   nop   
/* 0D0A6C 7F09BF3C 0FC26DF9 */  jal   sub_GAME_7F09B7E4
/* 0D0A70 7F09BF40 00000000 */   nop   
/* 0D0A74 7F09BF44 1000003A */  b     .L7F09C030
/* 0D0A78 7F09BF48 00001025 */   move  $v0, $zero
/* 0D0A7C 7F09BF4C 24010001 */  li    $at, 1
.L7F09BF50:
/* 0D0A80 7F09BF50 14610037 */  bne   $v1, $at, .L7F09C030
/* 0D0A84 7F09BF54 00001025 */   move  $v0, $zero
/* 0D0A88 7F09BF58 00A90019 */  multu $a1, $t1
/* 0D0A8C 7F09BF5C 8FAE0028 */  lw    $t6, 0x28($sp)
/* 0D0A90 7F09BF60 00006025 */  move  $t4, $zero
/* 0D0A94 7F09BF64 00006812 */  mflo  $t5
/* 0D0A98 7F09BF68 010D2021 */  addu  $a0, $t0, $t5
/* 0D0A9C 7F09BF6C 8498000E */  lh    $t8, 0xe($a0)
/* 0D0AA0 7F09BF70 848A000C */  lh    $t2, 0xc($a0)
/* 0D0AA4 7F09BF74 AC8E0004 */  sw    $t6, 4($a0)
/* 0D0AA8 7F09BF78 8FAF002C */  lw    $t7, 0x2c($sp)
/* 0D0AAC 7F09BF7C 27190001 */  addiu $t9, $t8, 1
/* 0D0AB0 7F09BF80 A499000E */  sh    $t9, 0xe($a0)
/* 0D0AB4 7F09BF84 1150001F */  beq   $t2, $s0, .L7F09C004
/* 0D0AB8 7F09BF88 AC8F0008 */   sw    $t7, 8($a0)
/* 0D0ABC 7F09BF8C 18C0001D */  blez  $a2, .L7F09C004
/* 0D0AC0 7F09BF90 00001825 */   move  $v1, $zero
/* 0D0AC4 7F09BF94 01001025 */  move  $v0, $t0
.L7F09BF98:
/* 0D0AC8 7F09BF98 844D000E */  lh    $t5, 0xe($v0)
/* 0D0ACC 7F09BF9C 54ED0017 */  bnel  $a3, $t5, .L7F09BFFC
/* 0D0AD0 7F09BFA0 24630001 */   addiu $v1, $v1, 1
/* 0D0AD4 7F09BFA4 8C8E0000 */  lw    $t6, ($a0)
/* 0D0AD8 7F09BFA8 A490000C */  sh    $s0, 0xc($a0)
/* 0D0ADC 7F09BFAC 00107900 */  sll   $t7, $s0, 4
/* 0D0AE0 7F09BFB0 0150C823 */  subu  $t9, $t2, $s0
/* 0D0AE4 7F09BFB4 01CFC021 */  addu  $t8, $t6, $t7
/* 0D0AE8 7F09BFB8 AC580000 */  sw    $t8, ($v0)
/* 0D0AEC 7F09BFBC A459000C */  sh    $t9, 0xc($v0)
/* 0D0AF0 7F09BFC0 A440000E */  sh    $zero, 0xe($v0)
/* 0D0AF4 7F09BFC4 A4450012 */  sh    $a1, 0x12($v0)
/* 0D0AF8 7F09BFC8 848D0010 */  lh    $t5, 0x10($a0)
/* 0D0AFC 7F09BFCC 240C0001 */  li    $t4, 1
/* 0D0B00 7F09BFD0 A44D0010 */  sh    $t5, 0x10($v0)
/* 0D0B04 7F09BFD4 84820010 */  lh    $v0, 0x10($a0)
/* 0D0B08 7F09BFD8 04400005 */  bltz  $v0, .L7F09BFF0
/* 0D0B0C 7F09BFDC 00000000 */   nop   
/* 0D0B10 7F09BFE0 00490019 */  multu $v0, $t1
/* 0D0B14 7F09BFE4 00007012 */  mflo  $t6
/* 0D0B18 7F09BFE8 010E7821 */  addu  $t7, $t0, $t6
/* 0D0B1C 7F09BFEC A5E30012 */  sh    $v1, 0x12($t7)
.L7F09BFF0:
/* 0D0B20 7F09BFF0 10000004 */  b     .L7F09C004
/* 0D0B24 7F09BFF4 A4830010 */   sh    $v1, 0x10($a0)
/* 0D0B28 7F09BFF8 24630001 */  addiu $v1, $v1, 1
.L7F09BFFC:
/* 0D0B2C 7F09BFFC 1466FFE6 */  bne   $v1, $a2, .L7F09BF98
/* 0D0B30 7F09C000 24420014 */   addiu $v0, $v0, 0x14
.L7F09C004:
/* 0D0B34 7F09C004 51800006 */  beql  $t4, $zero, .L7F09C020
/* 0D0B38 7F09C008 856D0000 */   lh    $t5, ($t3)
/* 0D0B3C 7F09C00C 85780000 */  lh    $t8, ($t3)
/* 0D0B40 7F09C010 0310C823 */  subu  $t9, $t8, $s0
/* 0D0B44 7F09C014 10000004 */  b     .L7F09C028
/* 0D0B48 7F09C018 A5790000 */   sh    $t9, ($t3)
/* 0D0B4C 7F09C01C 856D0000 */  lh    $t5, ($t3)
.L7F09C020:
/* 0D0B50 7F09C020 01AA7023 */  subu  $t6, $t5, $t2
/* 0D0B54 7F09C024 A56E0000 */  sh    $t6, ($t3)
.L7F09C028:
/* 0D0B58 7F09C028 10000001 */  b     .L7F09C030
/* 0D0B5C 7F09C02C 8C820000 */   lw    $v0, ($a0)
.L7F09C030:
/* 0D0B60 7F09C030 8FBF001C */  lw    $ra, 0x1c($sp)
/* 0D0B64 7F09C034 8FB00018 */  lw    $s0, 0x18($sp)
/* 0D0B68 7F09C038 27BD0020 */  addiu $sp, $sp, 0x20
/* 0D0B6C 7F09C03C 03E00008 */  jr    $ra
/* 0D0B70 7F09C040 00000000 */   nop   
)
#endif





#ifdef NONMATCHING
void sub_GAME_7F09C044(Vertex* arg0) {

}
#else
void sub_GAME_7F09C044(Vertex* arg0);

GLOBAL_ASM(
.text
glabel sub_GAME_7F09C044
/* 0D0B74 7F09C044 3C028008 */  lui   $v0, %hi(dword_CODE_bss_8007A0E0)
/* 0D0B78 7F09C048 8C42A0E0 */  lw    $v0, %lo(dword_CODE_bss_8007A0E0)($v0)
/* 0D0B7C 7F09C04C 27BDFFE0 */  addiu $sp, $sp, -0x20
/* 0D0B80 7F09C050 AFB00018 */  sw    $s0, 0x18($sp)
/* 0D0B84 7F09C054 0082082B */  sltu  $at, $a0, $v0
/* 0D0B88 7F09C058 00808025 */  move  $s0, $a0
/* 0D0B8C 7F09C05C 1420000D */  bnez  $at, .L7F09C094
/* 0D0B90 7F09C060 AFBF001C */   sw    $ra, 0x1c($sp)
/* 0D0B94 7F09C064 3C0E8008 */  lui   $t6, %hi(dword_CODE_bss_8007A0D0) 
/* 0D0B98 7F09C068 8DCEA0D0 */  lw    $t6, %lo(dword_CODE_bss_8007A0D0)($t6)
/* 0D0B9C 7F09C06C 3C0A8008 */  lui   $t2, %hi(word_CODE_bss_8007A0F0) 
/* 0D0BA0 7F09C070 3C078008 */  lui   $a3, %hi(dword_CODE_bss_8007A0E8)
/* 0D0BA4 7F09C074 25CFFFFF */  addiu $t7, $t6, -1
/* 0D0BA8 7F09C078 000FC100 */  sll   $t8, $t7, 4
/* 0D0BAC 7F09C07C 0302C821 */  addu  $t9, $t8, $v0
/* 0D0BB0 7F09C080 0324082B */  sltu  $at, $t9, $a0
/* 0D0BB4 7F09C084 14200003 */  bnez  $at, .L7F09C094
/* 0D0BB8 7F09C088 254AA0F0 */   addiu $t2, %lo(word_CODE_bss_8007A0F0) # addiu $t2, $t2, -0x5f10
/* 0D0BBC 7F09C08C 10000018 */  b     .L7F09C0F0
/* 0D0BC0 7F09C090 8CE7A0E8 */   lw    $a3, %lo(dword_CODE_bss_8007A0E8)($a3)
.L7F09C094:
/* 0D0BC4 7F09C094 3C028008 */  lui   $v0, %hi(dword_CODE_bss_8007A0E4)
/* 0D0BC8 7F09C098 8C42A0E4 */  lw    $v0, %lo(dword_CODE_bss_8007A0E4)($v0)
/* 0D0BCC 7F09C09C 3C0B8008 */  lui   $t3, %hi(dword_CODE_bss_8007A0D8) 
/* 0D0BD0 7F09C0A0 0202082B */  sltu  $at, $s0, $v0
/* 0D0BD4 7F09C0A4 1420000C */  bnez  $at, .L7F09C0D8
/* 0D0BD8 7F09C0A8 00000000 */   nop   
/* 0D0BDC 7F09C0AC 8D6BA0D8 */  lw    $t3, %lo(dword_CODE_bss_8007A0D8)($t3)
/* 0D0BE0 7F09C0B0 3C0A8008 */  lui   $t2, %hi(word_CODE_bss_8007A0F2) 
/* 0D0BE4 7F09C0B4 3C078008 */  lui   $a3, %hi(dword_CODE_bss_8007A0EC)
/* 0D0BE8 7F09C0B8 256CFFFF */  addiu $t4, $t3, -1
/* 0D0BEC 7F09C0BC 000C6900 */  sll   $t5, $t4, 4
/* 0D0BF0 7F09C0C0 01A27021 */  addu  $t6, $t5, $v0
/* 0D0BF4 7F09C0C4 01D0082B */  sltu  $at, $t6, $s0
/* 0D0BF8 7F09C0C8 14200003 */  bnez  $at, .L7F09C0D8
/* 0D0BFC 7F09C0CC 254AA0F2 */   addiu $t2, %lo(word_CODE_bss_8007A0F2) # addiu $t2, $t2, -0x5f0e
/* 0D0C00 7F09C0D0 10000007 */  b     .L7F09C0F0
/* 0D0C04 7F09C0D4 8CE7A0EC */   lw    $a3, %lo(dword_CODE_bss_8007A0EC)($a3)
.L7F09C0D8:
/* 0D0C08 7F09C0D8 0FC26DEA */  jal   sub_GAME_7F09B7A8
/* 0D0C0C 7F09C0DC 00000000 */   nop   
/* 0D0C10 7F09C0E0 0FC26DF9 */  jal   sub_GAME_7F09B7E4
/* 0D0C14 7F09C0E4 00000000 */   nop   
/* 0D0C18 7F09C0E8 10000053 */  b     .L7F09C238
/* 0D0C1C 7F09C0EC 8FBF001C */   lw    $ra, 0x1c($sp)
.L7F09C0F0:
/* 0D0C20 7F09C0F0 00002825 */  move  $a1, $zero
/* 0D0C24 7F09C0F4 00004025 */  move  $t0, $zero
/* 0D0C28 7F09C0F8 2409FFFF */  li    $t1, -1
/* 0D0C2C 7F09C0FC 24060014 */  li    $a2, 20
.L7F09C100:
/* 0D0C30 7F09C100 00A60019 */  multu $a1, $a2
/* 0D0C34 7F09C104 00007812 */  mflo  $t7
/* 0D0C38 7F09C108 00EF2021 */  addu  $a0, $a3, $t7
/* 0D0C3C 7F09C10C 8C980000 */  lw    $t8, ($a0)
/* 0D0C40 7F09C110 56180043 */  bnel  $s0, $t8, .L7F09C220
/* 0D0C44 7F09C114 84850010 */   lh    $a1, 0x10($a0)
/* 0D0C48 7F09C118 8482000E */  lh    $v0, 0xe($a0)
/* 0D0C4C 7F09C11C 24080001 */  li    $t0, 1
/* 0D0C50 7F09C120 2459FFFF */  addiu $t9, $v0, -1
/* 0D0C54 7F09C124 A499000E */  sh    $t9, 0xe($a0)
/* 0D0C58 7F09C128 848B000E */  lh    $t3, 0xe($a0)
/* 0D0C5C 7F09C12C 1560003F */  bnez  $t3, .L7F09C22C
/* 0D0C60 7F09C130 00000000 */   nop   
/* 0D0C64 7F09C134 854C0000 */  lh    $t4, ($t2)
/* 0D0C68 7F09C138 848D000C */  lh    $t5, 0xc($a0)
/* 0D0C6C 7F09C13C 018D7021 */  addu  $t6, $t4, $t5
/* 0D0C70 7F09C140 A54E0000 */  sh    $t6, ($t2)
/* 0D0C74 7F09C144 84830010 */  lh    $v1, 0x10($a0)
/* 0D0C78 7F09C148 04620016 */  bltzl $v1, .L7F09C1A4
/* 0D0C7C 7F09C14C 84830012 */   lh    $v1, 0x12($a0)
/* 0D0C80 7F09C150 00660019 */  multu $v1, $a2
/* 0D0C84 7F09C154 00007812 */  mflo  $t7
/* 0D0C88 7F09C158 00EF1021 */  addu  $v0, $a3, $t7
/* 0D0C8C 7F09C15C 8458000E */  lh    $t8, 0xe($v0)
/* 0D0C90 7F09C160 57000010 */  bnezl $t8, .L7F09C1A4
/* 0D0C94 7F09C164 84830012 */   lh    $v1, 0x12($a0)
/* 0D0C98 7F09C168 8499000C */  lh    $t9, 0xc($a0)
/* 0D0C9C 7F09C16C 844B000C */  lh    $t3, 0xc($v0)
/* 0D0CA0 7F09C170 032B6021 */  addu  $t4, $t9, $t3
/* 0D0CA4 7F09C174 A48C000C */  sh    $t4, 0xc($a0)
/* 0D0CA8 7F09C178 844D0010 */  lh    $t5, 0x10($v0)
/* 0D0CAC 7F09C17C A48D0010 */  sh    $t5, 0x10($a0)
/* 0D0CB0 7F09C180 A449000E */  sh    $t1, 0xe($v0)
/* 0D0CB4 7F09C184 84830010 */  lh    $v1, 0x10($a0)
/* 0D0CB8 7F09C188 04620006 */  bltzl $v1, .L7F09C1A4
/* 0D0CBC 7F09C18C 84830012 */   lh    $v1, 0x12($a0)
/* 0D0CC0 7F09C190 00660019 */  multu $v1, $a2
/* 0D0CC4 7F09C194 00007012 */  mflo  $t6
/* 0D0CC8 7F09C198 00EE7821 */  addu  $t7, $a3, $t6
/* 0D0CCC 7F09C19C A5E50012 */  sh    $a1, 0x12($t7)
/* 0D0CD0 7F09C1A0 84830012 */  lh    $v1, 0x12($a0)
.L7F09C1A4:
/* 0D0CD4 7F09C1A4 04600021 */  bltz  $v1, .L7F09C22C
/* 0D0CD8 7F09C1A8 00000000 */   nop   
/* 0D0CDC 7F09C1AC 00660019 */  multu $v1, $a2
/* 0D0CE0 7F09C1B0 0000C012 */  mflo  $t8
/* 0D0CE4 7F09C1B4 00F81021 */  addu  $v0, $a3, $t8
/* 0D0CE8 7F09C1B8 8459000E */  lh    $t9, 0xe($v0)
/* 0D0CEC 7F09C1BC 0003C400 */  sll   $t8, $v1, 0x10
/* 0D0CF0 7F09C1C0 1720001A */  bnez  $t9, .L7F09C22C
/* 0D0CF4 7F09C1C4 0018CC03 */   sra   $t9, $t8, 0x10
/* 0D0CF8 7F09C1C8 03260019 */  multu $t9, $a2
/* 0D0CFC 7F09C1CC 844B000C */  lh    $t3, 0xc($v0)
/* 0D0D00 7F09C1D0 848C000C */  lh    $t4, 0xc($a0)
/* 0D0D04 7F09C1D4 00032C00 */  sll   $a1, $v1, 0x10
/* 0D0D08 7F09C1D8 00057C03 */  sra   $t7, $a1, 0x10
/* 0D0D0C 7F09C1DC 016C6821 */  addu  $t5, $t3, $t4
/* 0D0D10 7F09C1E0 A44D000C */  sh    $t5, 0xc($v0)
/* 0D0D14 7F09C1E4 848E0010 */  lh    $t6, 0x10($a0)
/* 0D0D18 7F09C1E8 01E02825 */  move  $a1, $t7
/* 0D0D1C 7F09C1EC 00005812 */  mflo  $t3
/* 0D0D20 7F09C1F0 A44E0010 */  sh    $t6, 0x10($v0)
/* 0D0D24 7F09C1F4 A489000E */  sh    $t1, 0xe($a0)
/* 0D0D28 7F09C1F8 00EB6021 */  addu  $t4, $a3, $t3
/* 0D0D2C 7F09C1FC 85830010 */  lh    $v1, 0x10($t4)
/* 0D0D30 7F09C200 0460000A */  bltz  $v1, .L7F09C22C
/* 0D0D34 7F09C204 00000000 */   nop   
/* 0D0D38 7F09C208 00660019 */  multu $v1, $a2
/* 0D0D3C 7F09C20C 00006812 */  mflo  $t5
/* 0D0D40 7F09C210 00ED7021 */  addu  $t6, $a3, $t5
/* 0D0D44 7F09C214 10000005 */  b     .L7F09C22C
/* 0D0D48 7F09C218 A5CF0012 */   sh    $t7, 0x12($t6)
/* 0D0D4C 7F09C21C 84850010 */  lh    $a1, 0x10($a0)
.L7F09C220:
/* 0D0D50 7F09C220 14A90002 */  bne   $a1, $t1, .L7F09C22C
/* 0D0D54 7F09C224 00000000 */   nop   
/* 0D0D58 7F09C228 24080001 */  li    $t0, 1
.L7F09C22C:
/* 0D0D5C 7F09C22C 1100FFB4 */  beqz  $t0, .L7F09C100
/* 0D0D60 7F09C230 00000000 */   nop   
/* 0D0D64 7F09C234 8FBF001C */  lw    $ra, 0x1c($sp)
.L7F09C238:
/* 0D0D68 7F09C238 8FB00018 */  lw    $s0, 0x18($sp)
/* 0D0D6C 7F09C23C 27BD0020 */  addiu $sp, $sp, 0x20
/* 0D0D70 7F09C240 03E00008 */  jr    $ra
/* 0D0D74 7F09C244 00000000 */   nop   
)
#endif



