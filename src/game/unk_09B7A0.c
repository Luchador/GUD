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





s32 sub_GAME_7F09BE4C(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
    s16* var_t3;
    s16 temp_t2;
    s32 var_a1;
    s32 var_a2;
    s32 var_t4;
    s32 var_v0;
    s32 var_v1;
    s16 var_v1_2;
    struct unk_09B7A0_struct_parent* var_t0;

    switch (arg1) {
        case 0xCCCC:
            var_t0 = dword_CODE_bss_8007A0E8;
            var_t3 = &word_CODE_bss_8007A0F0;
            var_a2 = ((s16 *)&dword_CODE_bss_8007A0D4)[1];
            break;
        case 0xB0B:
            var_t0 = dword_CODE_bss_8007A0EC;
            var_t3 = &word_CODE_bss_8007A0F2;
            var_a2 = ((s16 *)&dword_CODE_bss_8007A0DC)[1];
            break;
        default:
            return 0;
    }
    var_v1_2 = 0;
    var_v0 = 0;
    var_a1 = 0;
    do {
        if ((var_t0[var_a1].unk0E == 0) && (var_t0[var_a1].unk0C >= arg0)) {
            var_v1_2 = 1;
        } else {
            var_a1 = var_t0[var_a1].unk10;
            var_v0 += 1;
            if ((var_a1 == -1) || (var_a2 < var_v0)) {
                var_v1_2 = (s16)-1;
            }
        }
    } while (var_v1_2 == 0);
    if (var_a2 < var_v0) {
        sub_GAME_7F09B7A8();
        sub_GAME_7F09B7E4();
        return 0;
    }
    // FAKE
    if (var_v0) {}
    if (var_v1_2 == 1) {
        var_t4 = 0;
        temp_t2 = var_t0[var_a1].unk0C;
        var_t0[var_a1].unk04 = arg2;
        var_t0[var_a1].unk08 = arg3;
        var_t0[var_a1].unk0E++;
        if (temp_t2 != arg0) {
            for (var_v1 = 0; var_v1 < var_a2; var_v1++) {
                if (var_t0[var_v1].unk0E == -1) {
                    var_t0[var_a1].unk0C = arg0;
                    var_t0[var_v1].unk00 = var_t0[var_a1].unk00 + arg0;
                    var_t0[var_v1].unk0C = temp_t2 - arg0;
                    var_t0[var_v1].unk0E = 0;
                    var_t0[var_v1].unk12 = var_a1;
                    var_t4 = 1;
                    var_t0[var_v1].unk10 = var_t0[var_a1].unk10;
                    if (var_t0[var_a1].unk10 >= 0) {
                        var_t0[var_t0[var_a1].unk10].unk12 = var_v1;
                    }
                    var_t0[var_a1].unk10 = var_v1;
                    break;
                }
            }
        }
        if (var_t4 != 0) {
            *var_t3 -= arg0;
        } else {
            *var_t3 -= temp_t2;
        }
        return (s32)var_t0[var_a1].unk00;
    }
    return 0;
}





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



