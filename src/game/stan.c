#include <ultra64.h>
//#include <bondtypes.h>
#include <deb.h>
#include "stan.h"
#include "bg.h"
#include "chrai.h"
#include "chr.h"
#include "stanintersection.h"
#include "assert.h"

typedef struct StanRoomBounds {
    s16 minX;
    s16 minY;
    s16 minZ;
    s16 maxX;
    s16 maxY;
    s16 maxZ;
} StanRoomBounds;

void getTileMidPoint(StandTile *tile, coord3d *out);

// bss
struct StanPrefixRecord {
    //CODE.bss:8007B120
    s32 stanfile;
    //CODE.bss:8007B124
    StandTile *ptr_firstroom;    // read as offset 4, hence the struct
};

struct StanPrefixRecord *stan_prefix;
s32 dword_CODE_bss_8007B124;

//CODE.bss:8007B128
s32 firststaninroom;
//CODE.bss:8007B12C
s32 dword_CODE_bss_8007B12C;
//CODE.bss:8007B130
s32 dword_CODE_bss_8007B130; //stanladder s1
//CODE.bss:8007B134
char dword_CODE_bss_8007B134;
char dword_CODE_bss_8007B135;
char dword_CODE_bss_8007B136;
char dword_CODE_bss_8007B137;
char dword_CODE_bss_8007B138[0x21C];
//CODE.bss:8007B354
s32 dword_CODE_bss_8007B354;
//CODE.bss:8007B358 //stan list array
s32 dword_CODE_bss_8007B358[0x1a1];
//CODE.bss:8007B9DC
s32 dword_CODE_bss_8007B9DC; //region?
//CODE.bss:8007B9E0
s32 dword_CODE_bss_8007B9E0;



// All relating to a saved collision, but not one struct
//CODE.bss:8007B9E4
StandTile *stanSavedColl_tile;
//CODE.bss:8007B9E8
s32 stanSavedColl_pointI;
//CODE.bss:8007B9EC
s32 stanSavedColl_unknown;
//CODE.bss:8007B9F0
struct coord2d stanSavedColl_pntA;
//CODE.bss:8007B9F8
struct coord2d stanSavedColl_pntB;
//CODE.bss:8007BA00
f32 stanSavedColl_someMin;

//CODE.bss:8007BA04
PropRecord * stanSavedColl_posData;

//CODE.bss:8007BA08
s32 dword_CODE_bss_8007BA08;
//CODE.bss:8007BA0C
StandTile * dword_CODE_bss_8007BA0C;
//CODE.bss:8007BA10
StandTile *bfsTileStack[352];


// data

//D:80040F30
// Indexed by StandTile.mid.headerMid.special.
u8 g_StanTileSpecialFlags[] = {
    0x8D, 0x86, 0x04, 0xC5,
    0x9D, 0xA4, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

s32 stan_c_debug_notice_list_entry = 0;

//D:80040F44
f32 level_scale = 1.0;
//D:80040F48
f32 inv_level_scale = 1.0;
//D:80040F4C
u8 list_of_tilesizes[] = {
    0x20,0x20,0x20,0x20,
    0x28,0x30,0x38,0x40,
    0x48,0x50,0x58,0x00
};
//D:80040F58
struct StandTile * standTileStart = NULL;
//D:80040F5C
s32 ptr_firstroom_0 = 0;
//D:80040F60
struct StandTile* stanTileEnd = NULL;
//D:80040F64
s32 D_80040F64[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
//D:80040FAC
s32 D_80040FAC = 0;
//D:80040FB0
s32 m_stanRegion = 0;
//D:80040FB4
s32 stanlinelog_flag = 0;

#if defined(LEFTOVERDEBUG)

s32 D_80040FB8[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0
};
#endif
//D:800413BC
s32 D_800413BC =  0;
//D:800413C0
f32 D_800413C0 =  0.0;
//D:800413C4
f32 D_800413C4 =  0.0;
//D:800413C8
s32 D_800413C8 =  1;
//D:800413CC
s32 D_800413CC =  1;
//D:800413D0
s32 D_800413D0[] =  {0, 0, 0, 0, 0, 0, 0, 0};


// rodata
//D:800585A0
const char aCDCC[] = "%c%d%c%c";
//D:800585AC
const char aStan_c_debug[] = "stan_c_debug";
//D:800585BC
const char aStanlinelog[] = "-stanlinelog";

// forward declarations

s32 stanIsSpecialBit1Set(StandTile *arg0, struct StandTileLocusCallbackRecord* arg1);
s32 stanCheckLinkedSpecialTile(StandTile *tile, s32 pointIdx, s32 arg2, s32 arg3, s32 arg4, s32 *outFlags);
s32 sub_GAME_7F0B21B0(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax);
f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 start3index, f32 p_x, f32 p_z);
s32 sub_GAME_7F0B1DDC(struct StandTile**, f32, f32, f32, standTileLocusCallback_A_t, standTileLocusCallback_B_t, standTileLocusCallback_C_t, struct StandTileLocusCallbackRecord*);
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec);
s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *arg0);
s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *arg0);
bool stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, f32 *yThreshold);

// end forward declarations

s32 stanBitwiseCastF32(f32 arg0)
{
    // disgusting
    return *(s32*)&arg0;
}


// maybe getstanroomID and returns a string
char *sub_GAME_7F0AEF3C(StandTile *tile)
{
    char *buffer;
    s32 nextidx;
    s32 type;
    s32 letter;
    s32 digit_raw;
    s32 masked_number;
    s32 idpart1;
    u8 idpart2;
    s32 idx;
    
    idx = D_80040FAC;
    buffer = (char *)D_80040F64 + (idx * 9);
    
    idpart1 = *((u16 *) tile);
    digit_raw = ((u8 *) tile)[2];
    idpart2 = digit_raw;
    
    letter = idpart2 >> 3;
    idx = (idx + 1) & 7;
    masked_number = idpart1 & 0x7fff;
    type = (idpart1 >> 15) & 1;
    letter &= 0x1f;
    nextidx = idx;
    digit_raw = idpart2 & 7;
    
    if (digit_raw)
    {
        idpart1 = *((u16 *) tile);
    }
    
    D_80040FAC = nextidx;
    
    if (!digit_raw)
    {
        if (digit_raw && digit_raw);
        
        idpart1 = 0;
    }
    else 
    {
        idpart1 = digit_raw + '0';
    }
    
    sprintf(buffer, aCDCC, type + 'p', masked_number, letter + 'a', idpart1);
    
    return buffer;
}


/**
 * Unreferenced.
 */
void sub_GAME_7F0AEFE0(StandTile *tile)
{
    sub_GAME_7F0AEF3C(tile); //maybe getstanroomID
}


//stanChecksf
u32 stanRemovedAnimationRoutine(s32 arg0) {
#ifdef DEBUG
    if (arg0 < ptr_firstroom_0)
    {
        osSyncPrintf("checksf: ERROR line %d %08x<%08x", __LINE__, arg0, ptr_firstroom_0);
    }
    if (stanTileEnd < arg0)
    {
        osSyncPrintf("checksf: ERROR line %d %08x>%08x", __LINE__, arg0, stanTileEnd);
    }
#endif
    return 0;
}

void stanInit(void) {
    debTryAdd(&stan_c_debug_notice_list_entry, &aStan_c_debug);//"stan_c_debug");
}






#ifdef NONMATCHING
void sub_GAME_7F0AF038(void) {
    void *temp_v1;
    s32 temp_s0;
    s32 temp_a2;
    void *temp_s0_2;
    s32 temp_s0_4;
    s16 temp_s0_3;
    s32 temp_s2;
    s32 temp_v1_2;
    s32 temp_s0_5;
    void *temp_a0;
    void *phi_v1;
    s32 curroom;
    void *firststan;
    void *phi_s3;
    void *phi_s5;
    s16 phi_s0;
    s32 phi_s2;
    s32 phi_v1_2;
    s32 phi_s0_2;
    void *phi_s1;
    s32 phi_a2;

    // Node 0
    dword_CODE_bss_8007B9DC = 0;
    dword_CODE_bss_8007B12C = 0;
    dword_CODE_bss_8007B12C = 0;
    dword_CODE_bss_8007B130 = 0;
    phi_v1 = &dword_CODE_bss_8007B134;
loop_1:
    // Node 1
    temp_v1 = (phi_v1 + 0x10);
    temp_v1->unk-10 = 0;
    temp_v1->unk-C = 0;
    temp_v1->unk-8 = 0;
    temp_v1->unk-4 = 0;
    phi_v1 = temp_v1;
    if (temp_v1 != &dword_CODE_bss_8007B354)
    {
        goto loop_1;
    }
    // Node 2
    if (*stan_prefix.ptr_firstroom != 0)
    {
        // Node 3
        curroom = 0xff;
        firststan = stan_prefix.ptr_firstroom;
        curroom = 0xff;
loop_4:
        // Node 4
        temp_s0 = firststan->unk3;
        curroom = curroom;
        if (curroom != temp_s0)
        {
            // Node 5
            curroom = (temp_s0 & 0xff);
            //assert(curroom<MAXSTANROOMS);
            if (curroom >= dword_CODE_bss_8007B9DC)
            {
                // Node 6
                dword_CODE_bss_8007B9DC = (s32) (curroom + 1);
            }
            //        assert(firststaninroom[curroom]==NULL);
            // Node 7
            *(&firststaninroom + (curroom * 4)) = (void *) firststan;
            temp_s0_2 = (&dword_CODE_bss_8007B358 + (curroom * 0xc));
            temp_s0_2->unk4 = (u16)0x7fff;
            temp_s0_2->unkA = (u16)-0x8000;
            temp_s0_2->unk2 = (s16) temp_s0_2->unk4;
            *temp_s0_2 = (s16) temp_s0_2->unk4;
            temp_s0_2->unk8 = (s16) temp_s0_2->unkA;
            temp_s0_2->unk6 = (s16) temp_s0_2->unkA;
            curroom = curroom;
            curroom = curroom;
        }
        // Node 8
        temp_s0_4 = (((s32) firststan->unk6 >> 0xc) & 0xf);
        phi_s0_2 = temp_s0_4;
        if (temp_s0_4 > 0)
        {
            // Node 9
            phi_v1_2 = 0;
            phi_s1 = firststan;
loop_10:
            // Node 10
            phi_s3 = phi_s1;
            phi_s5 = (&dword_CODE_bss_8007B358 + (((curroom * 4) - curroom) * 4));
            phi_s2 = 0;
loop_11:
            // Node 11
            temp_s0_3 = phi_s3->unk8;
            temp_s2 = (phi_s2 + 2);
            phi_s0 = temp_s0_3;
            if (temp_s0_3 < *phi_s5)
            {
                // Node 12
                *phi_s5 = temp_s0_3;
                phi_s0 = phi_s3->unk8;
            }
            // Node 13
            if (phi_s5->unk6 < phi_s0)
            {
                // Node 14
                phi_s5->unk6 = (s16) phi_s0;
            }
            // Node 15
            phi_s3 = (phi_s3 + 2);
            phi_s5 = (phi_s5 + 2);
            phi_s2 = temp_s2;
            if (temp_s2 != 6)
            {
                goto loop_11;
            }
            // Node 16
            temp_v1_2 = (phi_v1_2 + 1);
            temp_s0_5 = (((s32) firststan->unk6 >> 0xc) & 0xf);
            phi_v1_2 = temp_v1_2;
            phi_s0_2 = temp_s0_5;
            phi_s1 = (phi_s1 + 8);
            if (temp_v1_2 < temp_s0_5)
            {
                goto loop_10;
            }
        }
        // Node 17
        temp_a0 = (*(&list_of_tilesizes + phi_s0_2) + firststan);
        firststan = temp_a0;
        curroom = curroom;
        if (*temp_a0 != 0)
        {
            goto loop_4;
        }
    }
    // Node 18
    // Error: assert not self.is_leaf
    return;
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F0AF038
/* 0E3B68 7F0AF038 27BDFFE0 */  addiu $sp, $sp, -0x20
/* 0E3B6C 7F0AF03C 3C078008 */  lui   $a3, %hi(dword_CODE_bss_8007B9DC)
/* 0E3B70 7F0AF040 24E7B9DC */  addiu $a3, %lo(dword_CODE_bss_8007B9DC) # addiu $a3, $a3, -0x4624
/* 0E3B74 7F0AF044 ACE00000 */  sw    $zero, ($a3)
/* 0E3B78 7F0AF048 3C018008 */  lui   $at, %hi(dword_CODE_bss_8007B12C)
/* 0E3B7C 7F0AF04C AC20B128 */  sw    $zero, %lo(firststaninroom)($at)
/* 0E3B80 7F0AF050 AC20B12C */  sw    $zero, %lo(dword_CODE_bss_8007B12C)($at)
/* 0E3B84 7F0AF054 3C018008 */  lui   $at, %hi(dword_CODE_bss_8007B130)
/* 0E3B88 7F0AF058 3C038008 */  lui   $v1, %hi(dword_CODE_bss_8007B134)
/* 0E3B8C 7F0AF05C 3C028008 */  lui   $v0, %hi(dword_CODE_bss_8007B354)
/* 0E3B90 7F0AF060 AFB5001C */  sw    $s5, 0x1c($sp)
/* 0E3B94 7F0AF064 AFB40018 */  sw    $s4, 0x18($sp)
/* 0E3B98 7F0AF068 AFB30014 */  sw    $s3, 0x14($sp)
/* 0E3B9C 7F0AF06C AFB20010 */  sw    $s2, 0x10($sp)
/* 0E3BA0 7F0AF070 AFB1000C */  sw    $s1, 0xc($sp)
/* 0E3BA4 7F0AF074 AFB00008 */  sw    $s0, 8($sp)
/* 0E3BA8 7F0AF078 240600FF */  li    $a2, 255
/* 0E3BAC 7F0AF07C 2442B354 */  addiu $v0, %lo(dword_CODE_bss_8007B354) # addiu $v0, $v0, -0x4cac
/* 0E3BB0 7F0AF080 2463B134 */  addiu $v1, %lo(dword_CODE_bss_8007B134) # addiu $v1, $v1, -0x4ecc
/* 0E3BB4 7F0AF084 AC20B130 */  sw    $zero, %lo(dword_CODE_bss_8007B130)($at)
.L7F0AF088:
/* 0E3BB8 7F0AF088 24630010 */  addiu $v1, $v1, 0x10
/* 0E3BBC 7F0AF08C AC60FFF0 */  sw    $zero, -0x10($v1)
/* 0E3BC0 7F0AF090 AC60FFF4 */  sw    $zero, -0xc($v1)
/* 0E3BC4 7F0AF094 AC60FFF8 */  sw    $zero, -8($v1)
/* 0E3BC8 7F0AF098 1462FFFB */  bne   $v1, $v0, .L7F0AF088
/* 0E3BCC 7F0AF09C AC60FFFC */   sw    $zero, -4($v1)
/* 0E3BD0 7F0AF0A0 3C0E8008 */  lui   $t6, %hi(stan_prefix)
/* 0E3BD4 7F0AF0A4 8DCEB120 */  lw    $t6, %lo(stan_prefix)($t6)
/* 0E3BD8 7F0AF0A8 3C0D8004 */  lui   $t5, %hi(list_of_tilesizes)
/* 0E3BDC 7F0AF0AC 25AD0F4C */  addiu $t5, %lo(list_of_tilesizes) # addiu $t5, $t5, 0xf4c
/* 0E3BE0 7F0AF0B0 8DC40004 */  lw    $a0, 4($t6)
/* 0E3BE4 7F0AF0B4 00C02825 */  move  $a1, $a2
/* 0E3BE8 7F0AF0B8 240C8000 */  li    $t4, -32768
/* 0E3BEC 7F0AF0BC 8C8F0000 */  lw    $t7, ($a0)
/* 0E3BF0 7F0AF0C0 240B7FFF */  li    $t3, 32767
/* 0E3BF4 7F0AF0C4 240A000C */  li    $t2, 12
/* 0E3BF8 7F0AF0C8 11E00048 */  beqz  $t7, .L7F0AF1EC
/* 0E3BFC 7F0AF0CC 3C098008 */   lui   $t1, %hi(dword_CODE_bss_8007B358)
/* 0E3C00 7F0AF0D0 3C088008 */  lui   $t0, %hi(firststaninroom)
/* 0E3C04 7F0AF0D4 2508B128 */  addiu $t0, %lo(firststaninroom) # addiu $t0, $t0, -0x4ed8
/* 0E3C08 7F0AF0D8 2529B358 */  addiu $t1, %lo(dword_CODE_bss_8007B358) # addiu $t1, $t1, -0x4ca8
/* 0E3C0C 7F0AF0DC 24020006 */  li    $v0, 6
/* 0E3C10 7F0AF0E0 90900003 */  lbu   $s0, 3($a0)
.L7F0AF0E4:
/* 0E3C14 7F0AF0E4 00001825 */  move  $v1, $zero
/* 0E3C18 7F0AF0E8 50B00017 */  beql  $a1, $s0, .L7F0AF148
/* 0E3C1C 7F0AF0EC 84900006 */   lh    $s0, 6($a0)
/* 0E3C20 7F0AF0F0 8CF80000 */  lw    $t8, ($a3)
/* 0E3C24 7F0AF0F4 320600FF */  andi  $a2, $s0, 0xff
/* 0E3C28 7F0AF0F8 00C02825 */  move  $a1, $a2
/* 0E3C2C 7F0AF0FC 00D8082A */  slt   $at, $a2, $t8
/* 0E3C30 7F0AF100 14200002 */  bnez  $at, .L7F0AF10C
/* 0E3C34 7F0AF104 24B90001 */   addiu $t9, $a1, 1
/* 0E3C38 7F0AF108 ACF90000 */  sw    $t9, ($a3)
.L7F0AF10C:
/* 0E3C3C 7F0AF10C 00CA0019 */  multu $a2, $t2
/* 0E3C40 7F0AF110 00067080 */  sll   $t6, $a2, 2
/* 0E3C44 7F0AF114 010E7821 */  addu  $t7, $t0, $t6
/* 0E3C48 7F0AF118 ADE40000 */  sw    $a0, ($t7)
/* 0E3C4C 7F0AF11C 0000C012 */  mflo  $t8
/* 0E3C50 7F0AF120 01388021 */  addu  $s0, $t1, $t8
/* 0E3C54 7F0AF124 A60B0004 */  sh    $t3, 4($s0)
/* 0E3C58 7F0AF128 86110004 */  lh    $s1, 4($s0)
/* 0E3C5C 7F0AF12C A60C000A */  sh    $t4, 0xa($s0)
/* 0E3C60 7F0AF130 A6110002 */  sh    $s1, 2($s0)
/* 0E3C64 7F0AF134 A6110000 */  sh    $s1, ($s0)
/* 0E3C68 7F0AF138 8611000A */  lh    $s1, 0xa($s0)
/* 0E3C6C 7F0AF13C A6110008 */  sh    $s1, 8($s0)
/* 0E3C70 7F0AF140 A6110006 */  sh    $s1, 6($s0)
/* 0E3C74 7F0AF144 84900006 */  lh    $s0, 6($a0)
.L7F0AF148:
/* 0E3C78 7F0AF148 00067880 */  sll   $t7, $a2, 2
/* 0E3C7C 7F0AF14C 01E67823 */  subu  $t7, $t7, $a2
/* 0E3C80 7F0AF150 0010CB03 */  sra   $t9, $s0, 0xc
/* 0E3C84 7F0AF154 3330000F */  andi  $s0, $t9, 0xf
/* 0E3C88 7F0AF158 1A00001E */  blez  $s0, .L7F0AF1D4
/* 0E3C8C 7F0AF15C 00808825 */   move  $s1, $a0
/* 0E3C90 7F0AF160 000F7880 */  sll   $t7, $t7, 2
/* 0E3C94 7F0AF164 012FA021 */  addu  $s4, $t1, $t7
/* 0E3C98 7F0AF168 00009025 */  move  $s2, $zero
.L7F0AF16C:
/* 0E3C9C 7F0AF16C 02209825 */  move  $s3, $s1
/* 0E3CA0 7F0AF170 0280A825 */  move  $s5, $s4
.L7F0AF174:
/* 0E3CA4 7F0AF174 86700008 */  lh    $s0, 8($s3)
/* 0E3CA8 7F0AF178 86B80000 */  lh    $t8, ($s5)
/* 0E3CAC 7F0AF17C 26520002 */  addiu $s2, $s2, 2
/* 0E3CB0 7F0AF180 0218082A */  slt   $at, $s0, $t8
/* 0E3CB4 7F0AF184 50200004 */  beql  $at, $zero, .L7F0AF198
/* 0E3CB8 7F0AF188 86B90006 */   lh    $t9, 6($s5)
/* 0E3CBC 7F0AF18C A6B00000 */  sh    $s0, ($s5)
/* 0E3CC0 7F0AF190 86700008 */  lh    $s0, 8($s3)
/* 0E3CC4 7F0AF194 86B90006 */  lh    $t9, 6($s5)
.L7F0AF198:
/* 0E3CC8 7F0AF198 26730002 */  addiu $s3, $s3, 2
/* 0E3CCC 7F0AF19C 0330082A */  slt   $at, $t9, $s0
/* 0E3CD0 7F0AF1A0 10200002 */  beqz  $at, .L7F0AF1AC
/* 0E3CD4 7F0AF1A4 00000000 */   nop
/* 0E3CD8 7F0AF1A8 A6B00006 */  sh    $s0, 6($s5)
.L7F0AF1AC:
/* 0E3CDC 7F0AF1AC 1642FFF1 */  bne   $s2, $v0, .L7F0AF174
/* 0E3CE0 7F0AF1B0 26B50002 */   addiu $s5, $s5, 2
/* 0E3CE4 7F0AF1B4 84900006 */  lh    $s0, 6($a0)
/* 0E3CE8 7F0AF1B8 24630001 */  addiu $v1, $v1, 1
/* 0E3CEC 7F0AF1BC 26310008 */  addiu $s1, $s1, 8
/* 0E3CF0 7F0AF1C0 00107303 */  sra   $t6, $s0, 0xc
/* 0E3CF4 7F0AF1C4 31D0000F */  andi  $s0, $t6, 0xf
/* 0E3CF8 7F0AF1C8 0070082A */  slt   $at, $v1, $s0
/* 0E3CFC 7F0AF1CC 5420FFE7 */  bnezl $at, .L7F0AF16C
/* 0E3D00 7F0AF1D0 00009025 */   move  $s2, $zero
.L7F0AF1D4:
/* 0E3D04 7F0AF1D4 01B0C021 */  addu  $t8, $t5, $s0
/* 0E3D08 7F0AF1D8 93190000 */  lbu   $t9, ($t8)
/* 0E3D0C 7F0AF1DC 03242021 */  addu  $a0, $t9, $a0
/* 0E3D10 7F0AF1E0 8C8E0000 */  lw    $t6, ($a0)
/* 0E3D14 7F0AF1E4 55C0FFBF */  bnezl $t6, .L7F0AF0E4
/* 0E3D18 7F0AF1E8 90900003 */   lbu   $s0, 3($a0)
.L7F0AF1EC:
/* 0E3D1C 7F0AF1EC 8FB00008 */  lw    $s0, 8($sp)
/* 0E3D20 7F0AF1F0 8FB1000C */  lw    $s1, 0xc($sp)
/* 0E3D24 7F0AF1F4 8FB20010 */  lw    $s2, 0x10($sp)
/* 0E3D28 7F0AF1F8 8FB30014 */  lw    $s3, 0x14($sp)
/* 0E3D2C 7F0AF1FC 8FB40018 */  lw    $s4, 0x18($sp)
/* 0E3D30 7F0AF200 8FB5001C */  lw    $s5, 0x1c($sp)
/* 0E3D34 7F0AF204 03E00008 */  jr    $ra
/* 0E3D38 7F0AF208 27BD0020 */   addiu $sp, $sp, 0x20
)
#endif


/**
 * Address: 7F0AF20C
 * 
 * Finds the highest stan file beneath pos. If a tile is found, yRtn is set to the tile
 * height beneath pos.
 * @returns NULL if no suitable tile is found.
 */
StandTile *stanFindTileBelowPos(coord3d *pos, u8 *rooms, f32 *yRtn)
{
    StandTile *firstTile;
    f32 scaled[3];
    StandTile *tile;
    s16 scaledShort[3];
    coord3d *midPointPtr;
    u8 *tileSizes;
    StandTile *tileStack;
    StandTile *bestTile;
    f32 maxY;
    f32 bestY;
    f32 edgeDist;
    f32 tileY;
    s32 nearEdge;
    coord3d midPoint;
    s32 tailhalf;
    s32 room;
    StandTile **roomFirstTiles;
    s32 i;

    maxY = 32767.0f;
    bestY = -3.4028235e38f;
    bestTile = NULL;

    scaled[0] = pos->x * level_scale;
    scaled[1] = pos->y * level_scale;
    scaled[2] = pos->z * level_scale;

    if (maxY < scaled[1]) 
    {
        scaled[1] = maxY;
    }

    if (scaled[1] < -32767.0f) 
    {
        scaled[1] = -32767.0f;
    }

    scaledShort[0] = scaled[0];
    scaledShort[1] = scaled[1];
    scaledShort[2] = scaled[2];

    tileSizes = list_of_tilesizes;
    midPointPtr = &midPoint;
    room = 0; \
    if (dword_CODE_bss_8007B9DC > 0) \
    { \
        roomFirstTiles = (StandTile **) &firststaninroom; \
        do 
        {
            firstTile = *roomFirstTiles;
            
            if (firstTile != NULL)
            {
                if (scaledShort[0] < (&((StanRoomBounds *) dword_CODE_bss_8007B358)[room])->minX)
                {
                    goto next_room;
                }

                if ((&((StanRoomBounds *) dword_CODE_bss_8007B358)[room])->maxX < scaledShort[0])
                {
                    goto next_room;
                }

                if (scaledShort[2] < (&((StanRoomBounds *) dword_CODE_bss_8007B358)[room])->minZ)
                {
                    goto next_room;
                }

                if ((&((StanRoomBounds *) dword_CODE_bss_8007B358)[room])->maxZ < scaledShort[2])
                {
                    goto next_room;
                }

                if (scaledShort[1] < (&((StanRoomBounds *) dword_CODE_bss_8007B358)[room])->minY)
                {
                    goto next_room;
                }

                if (rooms != NULL)
                {
                    for (i = 0; (rooms[i] != 0xff) && (i != 4); i++)
                    {
                        if (room == rooms[i])
                        {
                            goto found_room;
                        }
                    }
                    goto next_room;
found_room:
                    ;
                }

                tile = firstTile;
                firstTile = *roomFirstTiles;

                while (((*((u32 *) tile)) != 0) && (tile->room == room))
                {
                    for (i = 0; i < 3; i++)
                    {
                        edgeDist = getShortest2dDispToInfTripleEdge(tile, i, scaled[0], scaled[2]);

                        if (edgeDist < -2.0f)
                        {
                            goto nexttile;
                        }
                        
                        if (edgeDist < 2.0f)
                        {
                            nearEdge = 1;
                        }
                    }
                    
                    if (stanTileHasZeroArea(tile))
                    {
                        goto nexttile;
                    }
                    
                    if (nearEdge)
                    {
                        getTileMidPoint(tile, midPointPtr);
                        tileStack = tile;
                        
                        if (!walkTilesBetweenPoints_NoCallback(&tileStack, midPointPtr->x, midPointPtr->z, pos->x, pos->z)) 
                        {
                            goto nexttile;
                        }
                        
                        if (tileStack != tile) 
                        {
                            goto nexttile;
                        }
                    }
                    
                    tileY = stanGetPositionYValue(tile, pos->x, pos->z);
                    
                    if (pos->y < tileY)
                    {
                        goto nexttile;
                    }
                    
                    if (bestY < tileY)
                    {
                        bestTile = tile;
                        bestY = tileY;
                    }
nexttile:
                    tailhalf = tile->tail.half;
                    tile = (StandTile *) (((u8 *) tile) + list_of_tilesizes[(tailhalf >> 12) & 0xf]); \
                } \
next_room:
                ;
            }
            
            room++;
            roomFirstTiles++;
        } while (room < dword_CODE_bss_8007B9DC);
    }
    
    if ((bestTile != NULL) && (yRtn != NULL))
    {
        *yRtn = bestY;
    }
        
    return bestTile;
}


void stanLoadFile(struct StanPrefixRecord *file)
{
    struct StanPrefixRecord *prefix = &stan_prefix;
    s32 tokenIndexMask;

    m_stanRegion = 1;
    tokenIndexMask = !file->ptr_firstroom;
    prefix->stanfile = file;
    tokenIndexMask = 1;

    /*
     * Matching artifacts.
     */
    if (prefix);
    if (prefix);
    if (prefix);

    standTileStart = (StandTile *)(((u8 *)file->ptr_firstroom) - 0x80);

    if (tokenFind(tokenIndexMask, aStanlinelog))
    {
        stanlinelog_flag = 1;
    }

    sub_GAME_7F0AF038();
    setLevelScale(1.0f);
}


//stanRegion()
void sub_GAME_7F0AF630(s32 arg0)
{
#ifdef DEBUG
    StandTile **rooms;

    rooms = &stan_prefix->ptr_firstroom;

    if (arg0 < 0)
    {
        if (rooms[m_stanRegion - 1] != NULL)
        {
            m_stanRegion--;
        }
    }
    else if (arg0 == 0)
    {
        m_stanRegion = 1;
    }
    else if (rooms[m_stanRegion] != NULL)
    {
        m_stanRegion++;
    }

    osSyncPrintf("stanRegion():  region=%d", m_stanRegion);
#endif
    return;
}


/**
 * Address: 7F0AF638
 * 
 * Unreferenced
 * 
 * Somewhere in this function a loop is checked for overflow
 * if (i < param4)
 * {
 *     printf("stanFillin: Stack overflow %d>%d",local_20,uStack);
 * }
 */
s32 stanFillin(StandTile *starttile, u8 targetbit, StandTile **stack) // Canonical function name
{
    StandTile *tile;
    StandTile *linkedtile;
    StandTilePoint *point;
    u16 *tmp;
    s32 pointcount;
    s32 link;
    s32 result;
    s32 count;
    s32 i;
    s32 stackcount;

    count = 0;
    stack[0] = starttile;
    stackcount = 1;

    for (stack += stackcount; stackcount != 0;)
    {
        tile = stack[-1];
        stackcount--;
        stack--;
        i = 0;

        if (targetbit != (((*((u16 *) tile)) >> 15) & 1))
        {
            tmp = (u16 *) tile;
            *tmp ^= 0x8000;
            result = stanTileHasZeroArea(tile);
            point = (StandTilePoint *) tile;

            if (stackcount);
            
            if (result == 0)
            {
                count++;
            }

            pointcount = ((&tile->tail)->half >> 12) & 0xf;

            if (pointcount > 0)
            {
                do
                {
                    link = point[1].link;
                    i++;

                    if (stackcount);
                    
                    if ((link >> 4) != 0)
                    {
                        linkedtile = (StandTile *) (((u8 *) standTileStart) + (((0, link)) << 3));

                        if (targetbit != (((*((u16 *) linkedtile)) >> 15) & 1))
                        {
                            *stack = linkedtile;
                            pointcount = (tile->tail.half >> 12) & 0xf;
                            stackcount++;
                            stack++;
                        }
                    }

                    point++;

                    if (starttile);
                }
                while (i < pointcount);
            }
        }
    }

    return count;
}


/**
 * Address: 7F0AF760
 * 
 * Returns true if x/z coords from the three point indices out of tile->tail.half are colinear i.e. the triangle has zero horizontal area.
 */
bool stanTileHasZeroArea(StandTile *tile)
{
    s32 AB[3];
    s32 AC[3];
    u32 crossStore[2];
    s32 temp1, temp2, temp3;
    

    temp1 = (tile->tail.half >> 8) & 0xf;
    temp2 = (tile->tail.half >> 4) & 0xf;
    temp3 = (tile->tail.half) & 0xf;

    AB[0] = tile->points[temp2].x - tile->points[temp1].x;
    AB[2] = tile->points[temp2].z - tile->points[temp1].z;
    
    AC[0] = tile->points[temp3].x - tile->points[temp1].x;
    AC[2] = tile->points[temp3].z - tile->points[temp1].z;

    crossStore[0] = (AB[2] * AC[0]) - (AB[0] * AC[2]);

    return crossStore[0] == 0;
}


/**
 * Address: 7F0AF808
 * 
 * Unreferenced.
 */
StandTile *stanFindFloorTileBelowY(f32 x, f32 maxY, f32 z, f32 radius)
{
    StandTile *tileStack[2];
    StandTile *tile;
    s32 temp;

    tile = stan_prefix->ptr_firstroom;

    while (*(u32 *)tile != 0)
    {
        tileStack[0] = tile;

        if (stanTileHasZeroArea(tile) == 0)
        {
            if (isPointInsideTriStandTileUnscaled_Maybe(tile, x, z))
            {
                if (sub_GAME_7F0B20D0(tileStack, x, z, radius))
                {
                    if (tileStack[0] == tile)
                    {
                        if (stanGetPositionYValue(tile, x, z) < maxY)
                        {
                            return tile;
                        }
                    }
                }
            }
        }

        temp = tile->tail.hdrTail.pointCount;
        tile = (StandTile *)((u8 *)tile + list_of_tilesizes[temp & 0xf]);
    }

    return NULL;
}
void getTileMidPoint(StandTile *tile, coord3d *out)
{
    u16 tail;
    u8 indexA;
    u32 indexB;
    u32 indexC;
    StandTilePoint *pointA;
    StandTilePoint *pointB;
    unsigned int new_var2;
    StandTilePoint *pointC;
    s16 *new_var3;

    tail = (indexC = tile->tail.half);
    new_var2 = (tail & 0xFFFF) >> 4;
    indexA = (tail >> 8) & 0xf;
    indexB = new_var2 & 0xf;
    indexC = indexC & 0xf;
    new_var3 = &(&tile->points[indexC])->x;
    pointA = &tile->points[indexA];
    pointB = &tile->points[indexB];
    out->x = (((((f32) pointA->x) + ((f32) pointB->x)) + ((f32) (*new_var3))) / 3.0f) * inv_level_scale;
    out->y = (((((f32) (&tile->points[indexA])->y) + ((f32) pointB->y)) + ((f32) (&tile->points[indexC])->y)) / 3.0f) * inv_level_scale;
    out->z = (((((f32) (&tile->points[indexA])->z) + ((f32) pointB->z)) + ((f32) ((float) (&tile->points[indexC])->z))) / 3.0f) * inv_level_scale;
}


void getPointJustInsideOfTileTriple(StandTile *tile, s32 tripleIndex /*canonically c */, coord3d *out)
{
    coord3d midPoint;
    s32 pntIndex;

    #ifdef DEBUG
    assert(c<3);
    #endif

    pntIndex = (tile->tail.half >> (8 - (tripleIndex * 4))) & 0xf;
    
    if (1);
    if (&midPoint);
    
    out->x = ((f32) tile->points[pntIndex].x) * inv_level_scale;
    out->y = ((f32) tile->points[pntIndex].y) * inv_level_scale;
    out->z = ((f32) tile->points[pntIndex].z) * inv_level_scale;
    
    getTileMidPoint(tile, &midPoint);
    
    // 10% of the way from the actual tile point towards the tile's centre.
    out->x = (midPoint.x * 0.1f) + (0.9f * out->x);
    out->y = (midPoint.y * 0.1f) + (0.9f * out->y);
    out->z = (midPoint.z * 0.1f) + (0.9f * out->z);
}


/*
* Address: 0x7F0AFB1C
 */
f32 sub_GAME_7F0AFB1C(coord3d *p,coord3d *q)
{
    // Should be a coord3d or vec3d, but they used an array which
    // causes lots of data reads and writes to the stack.
    f32 components[3];

    components[0] = q->x - p->x;
    components[1] = q->y - p->y;
    components[2] = q->z - p->z;

    return components[0]*components[0] + components[1]*components[1] + components[2]*components[2];
}


StandTile *sub_GAME_7F0AFB78(f32 *x, f32 *y, f32 *z, f32 arg3)
{
    StandTile *tile;
    s32 tileTail;
    StandTile *stack[1];
    StandTile *bestTile;
    s32 i;
    s32 midpointIndex;
    coord3d original;
    coord3d candidate;
    f32 bestDist;
    f32 dist;

    bestTile = NULL;
    original.x = *x;
    original.y = *y;
    original.z = *z;
    midpointIndex = 3;
    bestDist = M_U32_MAX_VALUE_F;

    if (&original);

    tile = stan_prefix->ptr_firstroom;

    if (*((u32 *) tile))
    {
        do
        {
            if (((((u16 *) tile)[0] >> 15) & 1) != 1)
            {
                if (stanTileHasZeroArea(tile) == FALSE)
                {
                    for (i = 0; i != 4; i++)
                    {
                        if (i == midpointIndex)
                        {
                            getTileMidPoint(tile, &candidate);
                        }
                        else
                        {
                            getPointJustInsideOfTileTriple(tile, i, &candidate);
                        }

                        stack[0] = tile;

                        if (sub_GAME_7F0B20D0(stack, candidate.x, candidate.z, arg3) < 0)
                        {
                            if (x);
                            if (y);
                            if (z);

                            dist = sub_GAME_7F0AFB1C(&candidate, &original);

                            if (dist < bestDist)
                            {
                                bestTile = tile;
                                bestDist = dist;
                                *x = candidate.x;
                                *y = candidate.y;
                                *z = candidate.z;
                            }
                        }
                    }
                }
            }

            tileTail = tile->tail.half;
            tile = (StandTile *) (((u8 *) tile) + list_of_tilesizes[(tileTail >> 12) & 0xf]);
            
        } while (*((u32 *) tile));
    }

    return bestTile;
}


// Returns the shortest distance from (p_x,p_z) to the infinite extention of tile's index-th edge, projected into XZ.
// Where the edge is vertical (or degenerate) they just return the distance between the points.
// cannonically tile is sf and index is ei
f32 getShortest2dDispToInfTileEdge(StandTile *tile,s32 index,f32 p_x,f32 p_z)
{
    s32 nextIndex;
    f32 edge_x;
    f32 edge_z;
    f32 edge_len; //canonically d

    f32 v_x;
    f32 v_z;
    f32 crossProduct;

    // 3 unused. We use 2 for the points to make our code cleaner,
    //   though it seems much more likely that the variables were used in the else clause.
    struct StandTilePoint* currPnt;
    struct StandTilePoint* nextPnt;
    f32 UNUSED;

    #ifdef DEBUG
    assert(ei<getsides(sf));
    #endif

    // Omiting the '& 0xF' is equivalent, but keeping it is necessary to match.
    // Perhaps the structure isn't correct but this seems much cleaner than doing an explicit >> 0xC.
    nextIndex = (index + 1) % STAN_TAIL_E(tile);

    nextPnt = &tile->points[nextIndex];
    currPnt = &tile->points[index];
    edge_x = (f32)(nextPnt->x - currPnt->x);
    edge_z = (f32)(nextPnt->z - currPnt->z);

    edge_len = sqrtf(edge_x * edge_x + edge_z * edge_z);

    if (edge_len == 0) {
        // Degenerate case, edge is vertical
        // They just return the distance between the points, which is sensible and the correct value in 3 dimensions.
        v_x = p_x - (f32)tile->points[nextIndex].x;
        v_z = p_z - (f32)tile->points[nextIndex].z;
        return sqrtf(v_x * v_x + v_z * v_z);
    }
    else
    {
        #ifdef DEBUG
        assert(d>0.0f);
        #endif

        // | (AP x AB) / ||AB|| | = ||PA|| sin(a),
        // so we're returning the SIGNED displacement
        crossProduct = (
            edge_z * (p_x - (f32)tile->points[index].x)
            +
            -edge_x * (p_z - (f32)tile->points[index].z)
        );
        return crossProduct / edge_len;
    }

}


f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 start3index, f32 p_x, f32 p_z)
{
    f32 dx;
    f32 edgeX;
    f32 edgeZ;
    f32 edgeLen;
    f32 dz;
    f32 crossProduct;
    s32 end3index;
    s32 currPntI;
    s32 nextPntI;
    s32 tail;

    #ifdef DEBUG
    assert(ei<getsides(sf));
    #endif

    nextPntI = 2;

    if (start3index != nextPntI) {
        end3index = start3index + 1;
    } else {
        end3index = 0;
    }

    start3index = (tile->tail.half >> (8 - (start3index << nextPntI))) & 0xf;
    end3index = (tile->tail.half >> (8 - (end3index << nextPntI))) & 0xf;

    edgeX = tile->points[end3index].x - tile->points[start3index].x;
    edgeZ = tile->points[end3index].z - tile->points[start3index].z;
    edgeLen = sqrtf((edgeX * edgeX) + (edgeZ * edgeZ));

    if (edgeLen == 0.0f) {
        dx = p_x - tile->points[end3index].x;
        dz = p_z - tile->points[end3index].z;
        return sqrtf((dx * dx) + (dz * dz));
    }

    #ifdef DEBUG
    assert(d>0.0f);
    #endif

    crossProduct = (edgeZ * (p_x - tile->points[start3index].x)) + (-edgeX * (p_z - tile->points[start3index].z));
    return crossProduct / edgeLen;
}


f32 getShortest2dDispToInfTileEdgeUnscaled(StandTile *tile, int index,f32 x,f32 z)
{
  f32 disp;

  disp = getShortest2dDispToInfTileEdge(tile, index, x * level_scale, z * level_scale);
  return disp * inv_level_scale;
}


f32 getShortest2dDispToInfTripleEdgeUnscaled(StandTile *tile,s32 start3index,f32 p_x,f32 p_z)
{
  f32 disp;

  disp = getShortest2dDispToInfTripleEdge(tile, start3index, p_x * level_scale, p_z * level_scale);
  return disp * inv_level_scale;
}


f32 distToTilePnt2D(StandTile *tile,int pntI,f32 p_x,f32 p_z)
{
  f32 len;

  p_x -= (f32)tile->points[pntI].x;
  p_z -= (f32)tile->points[pntI].z;
  return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Unreferenced.
 */
f32 sub_GAME_7F0B00C4(StandTile *tile, s32 pntI, f32 p_x, f32 p_z)
{
    p_x *= level_scale;
    p_z *= level_scale;

    p_x -= tile->points[pntI].x;
    p_z -= tile->points[pntI].z;

    return sqrtf((p_x * p_x) + (p_z * p_z)) * inv_level_scale;
}


/**
 * Address: 7F0B0140
 * 
 * Unreferenced.
 */
f32 stanPointDot2D(StandTile *tile, s32 index, f32 x, f32 z)
{
    StandTilePoint *point;

    point = &tile->points[index];

    x *= level_scale;
    z *= level_scale;

    return (((f32)point->z * z) + (x * (f32)point->x)) * inv_level_scale;
}


/**
 * Address: 7F0B0198
 * 
 * Returns true if the perpendicular projection of the X/Z point onto the
 * tile edge's infinite line falls between the edge endpoints.
 *
 * Example:
 *
 *     A -------- B
 *          |
 *          |
 *          P
 *
 * P is not on the edge, but its projection lands between A and B.
 */
bool stanPointProjectsOntoTileEdge(StandTile *tile, s32 edgeIndex, f32 p_x, f32 p_z)
{
    StandTilePoint *point;
    f32 edgeXCopy;
    f32 startX;
    f32 startZ;
    f32 edgeX;
    f32 edgeZ;
    StandTilePoint *nextPoint;

    point = &tile->points[edgeIndex];

    startX = point->x;
    startZ = point->z;

    edgeIndex = (edgeIndex + 1) % ((tile->tail.half >> 12) & 0xf);

    point = (nextPoint = &tile->points[edgeIndex]);

    edgeX = point->x;
    edgeX = edgeX - startX;

    edgeZ = point->z;
    edgeZ = edgeZ - startZ;

    p_x -= startX;
    p_z -= startZ;

    edgeXCopy = edgeX;

    startZ = (edgeXCopy * edgeXCopy) + (edgeZ * edgeZ);
    startX = (p_x * edgeXCopy) + (p_z * edgeZ);

    edgeZ = startX;

    return ((startZ < edgeZ) && (edgeZ < 0.0f))
        || ((0.0f < edgeZ) && (edgeZ < startZ));
}


// Determines if inside (presumably - it effectively does an && of the checks on signs of cross products)
//   based on the 3 edges. So probably only for triangular tiles.
s32 isPointInsideTriStandTile_Maybe(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 disp;
    s32 i;

    for (i = 0; i != 3; i++)
    {
        disp = getShortest2dDispToInfTripleEdge(tile,i,p_x,p_z);
        if (disp < 0) {
            return 0;
        }
    }

    return 1;
}



s32 isPointInsideTriStandTileUnscaled_Maybe(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 disp;
    s32 i;

    for (i = 0; i != 3; i++)
    {
        disp = getShortest2dDispToInfTripleEdgeUnscaled(tile,i,p_x,p_z);
        if (disp < 0) {
            return 0;
        }
    }

    return 1;
}


/*
* Address: 0x7F0B0400
*/
f32 sub_GAME_7F0B0400(StandTile *tile, s32 start3index, f32 p_x, f32 p_z)
{
    f32 temp_f0;
    f32 temp_f2;
    f32 temp_f14;
    s32 var_a0;
    s32 padding;

    f32 tempf;
    s32 extra_padding[2];

    #ifdef DEBUG
    assert(ei<getsides(sf));
    #endif

    var_a0 = (start3index != 2) ? start3index + 1 : 0;

    start3index = (tile->tail.half >> (8 - (start3index << 2))) & 0xF;
    var_a0 = (tile->tail.half >> (8 - (var_a0 << 2))) & 0xF;

    temp_f2 = (f32)(tile->points[var_a0].x - tile->points[start3index].x);
    temp_f14 = (f32)(tile->points[var_a0].z - tile->points[start3index].z);

    temp_f0 = sqrtf((temp_f2 * temp_f2) + (temp_f14 * temp_f14));

    if (temp_f0 == 0.0f) {
        return 0.0f;
    }

    #ifdef DEBUG
    assert(d>0.0f);
    #endif

    tempf = (temp_f14 * (p_x - tile->points[start3index].x)) + ((p_z - tile->points[start3index].z) * -temp_f2);
    return tempf / temp_f0;
}




bool stanTestPointWithinTileBoundsMaybe(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 unk;
    s32 i;

    p_x *= level_scale;
    p_z *= level_scale;

    for (i = 0; i != 3; i++)
    {
        unk = sub_GAME_7F0B0400(tile, i, p_x, p_z);

        if (unk < -2)
        {
            return FALSE;
        }
    }

    return TRUE;
}



// A->B ACWS returns 1, CWS (including opposite) returns -1.
// Identical direction and |A| >= |B| returns 0
int getRotationalDirectionBetween(f32 a_x,f32 a_z,f32 b_x,f32 b_z)
{
    // The main 2 cases : return the sign of AxB where it's non-zero
    if (a_z * b_x < a_x * b_z) {
        return 1;
    }
    if (a_x * b_z < a_z * b_x) {
        return -1;
    }

    // [AxB == 0 now]

    // If the vectors are opposite, default to clockwise
    if ((a_x * b_x < 0) || (a_z * b_z < 0)) {
      return -1;
    }

    // If A is shorter, return anti-clockwise
    if (a_x * a_x + a_z * a_z < b_x * b_x + b_z * b_z) {
        return 1;   // ACWS
    }

    // Identical direction, |A| >= |B|
    return 0;
}





s32 sub_GAME_7F0B0688(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7)
{
    s32 unused1;
    s32 unused2;
    f32 sp24;
    f32 sp20;
    f32 sp1C;
    f32 sp18;

    sp24 = arg0 - arg4;
    sp20 = arg1 - arg5;
    sp1C = arg2 - arg0;
    sp18 = arg3 - arg1;

    return
        (
            (getRotationalDirectionBetween(sp1C, sp18, -sp24, -sp20)
            * getRotationalDirectionBetween(sp1C, sp18, arg6 - arg0, arg7 - arg1)) < 1)
        &&
        (
            (getRotationalDirectionBetween(arg6 - arg4, arg7 - arg5, sp24, sp20)
            * getRotationalDirectionBetween(arg6 - arg4, arg7 - arg5, arg2 - arg4, arg3 - arg5)) < 1)
        ;
}


#if defined(LEFTOVERDEBUG)
s32 sub_GAME_7F0B07BC(f32 arg0, f32 arg1, f32 arg2, f32 arg3,
                      f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    f32 a_x;
    f32 a_z;
    f32 b_x;
    f32 b_z;
    f32 c_x;
    f32 c_z;

    s32 val1;
    s32 unused4;
    s32 rc;
    s32 unused5;
    s32 unused6;
    s32 val2;
    s32 unused7;
    s32 unused8;
    s32 tmp;

    rc = 1;

    b_x = arg0 - arg4;
    b_z = arg1 - arg5;
    a_x = arg2 - arg0;
    a_z = arg3 - arg1;

    tmp = getRotationalDirectionBetween(a_x, a_z, -b_x, -b_z);
    val1 = tmp * getRotationalDirectionBetween(a_x, a_z, arg6 - arg0, arg7 - arg1);

    c_x = arg6 - arg4;
    c_z = arg7 - arg5;

    tmp = getRotationalDirectionBetween(c_x, c_z, b_x, b_z);
    val2 = tmp * getRotationalDirectionBetween(c_x, c_z, arg2 - arg4, arg3 - arg5);

    if (val1 >= arg8) {
        rc = 0;
    }

    if (val2 >= arg8) {
        rc = 0;
    }

    return rc;
}
#else
s32 sub_GAME_7F0B07BC(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5, f32 arg6, f32 arg7, s32 arg8) {
    f32 a_z;
    f32 a_x;
    f32 b_z;
    f32 b_x;
    f32 c_x;

    s32 val2;
    s32 val1;
	f32 c_z;
    s32 rc;

    rc = 1;

    b_x = -(arg0 - arg4);
    b_z = -(arg1 - arg5);
	a_x = (arg2 - arg0);
    a_z = (arg3 - arg1);
    c_x = arg6 - arg0;
    c_z = arg7 - arg1;
    val1 = getRotationalDirectionBetween(a_x, a_z, b_x, b_z) * getRotationalDirectionBetween(a_x, a_z, c_x, c_z);

	a_x = (arg6 - arg4);
    a_z = (arg7 - arg5);
    b_x = (arg0 - arg4);
    b_z = (arg1 - arg5);
    c_x = arg2 - arg4;
    c_z = arg3 - arg5;

    val2 = getRotationalDirectionBetween(a_x, a_z, b_x, b_z) * getRotationalDirectionBetween(a_x, a_z, c_x, c_z);

    if (val1 >= arg8) {
        rc = 0;
    }

    if (val2 >= arg8) {
        rc = 0;
    }
    return rc;
}
#endif


// 'walkTilesBetweenPoints_withCallback'
// sig declared for caller matches
bool sub_GAME_7F0B0914(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z,
    standTileWalkCallback_t func, struct StandTileWalkCallbackRecord *funcData);


#ifdef NONMATCHING
bool sub_GAME_7F0B0914(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z,
    standTileWalkCallback_t func, struct StandTileWalkCallbackRecord *funcData)
{
     s32 sp8C;
    StandTile *tileStackTop;
    StandTile *temp_v0_2;
    StandTile *var_s0;
    StandTile *var_s3;
    StandTile *var_s6;
    StandTile *var_s7;
    f32 scaled_startX;
    f32 scaled_startZ;
    f32 scaled_destX;
    f32 scaled_destZ;
    f32 zDist;
    f32 xDist;
    s16 point2_z;
    s16 startpoint2_z;
    s16 point2_x;
    s16 startpoint2_x;
    s32 temp_s2;
    s32 temp_t5;
    s32 pointcount;
    s32 nextTile;
    s32 pointcount2;
    s32 var_fp;
    s32 var_s1;
    s32 var_s4;
    s32 trynexttile;
    u16 temp_v1;


    scaled_startX = start_x * level_scale;
    scaled_startZ = start_z * level_scale;
    scaled_destX = dest_x * level_scale;
    scaled_destZ = dest_z * level_scale;
    tileStackTop = *tileStack;
    trynexttile = 0;
    sp8C = 0;
    zDist = -(scaled_destZ - scaled_startZ);
    xDist = scaled_destX - scaled_startX;
    var_s1 = 0;
    var_s4 = 0;
    var_fp = sp98;
    var_s3 = tileStackTop;
    var_s6 = tileStackTop;
    var_s7 = tileStackTop;

    while (true)
    {
        if (func != NULL)
        {
            func(var_s3, var_s6, funcData);
    #ifdef DEBUG
            ossyncPrintf("{\"%s\",0x%08x,0x%08x,0x%08x,0x%08x},\t/* %8.3f %8.3f  %8.3f %8.3f */\n", GetStanRoomID(*tilestack), start_x, start_z, dest_x, dest_z, start_x, start_z);
    #endif

        }
        var_s0 = var_s3;
        pointcount = (var_s3->tail.half >> 0xC) & 0xF; //pointcount
        pointcount2 = pointcount;
        if (pointcount > 0)
        {
            do
            {
                temp_s2 = var_s1 + 1;
                temp_v0_2 = &var_s3[temp_s2 % pointcount2];
                point2_z = temp_v0_2->unkC;
                startpoint2_z = var_s0->unkC;
                point2_x = temp_v0_2->unk8;
                startpoint2_x = var_s0->unk8;
                if (((((point2_z - startpoint2_z) * xDist) + (zDist * (point2_x - startpoint2_x))) <= 0.0f) && (sub_GAME_7F0B07BC(scaled_startX, scaled_startZ, pointcount2, point2_z, scaled_destX, scaled_destZ, startpoint2_x, startpoint2_z, point2_x, point2_z, (var_s0->unkE >> 4) != 0) != 0))
                {
                    temp_v1 = var_s0->unkE;
                    var_s4 += 1;
                    nextTile = (temp_v1 * 8) + standTileStart;
                    if ((var_s6 != nextTile) && (var_s7 != nextTile))
                    {
                        var_fp = var_s1;
                        if ((temp_v1 >> 4) != 0)
                        {
                            trynexttile = nextTile;
                        }
                        else
                        {
                            trynexttile = NULL;
                        }
                    }
                }
                var_s1 = temp_s2;
                var_s0 += 8;
                temp_t5 = (var_s3->tail.half >> 0xC) & 0xF;
                pointcount2 = temp_t5;
            } while (temp_s2 < temp_t5);
        }
        var_s7 = var_s6;
        var_s6 = var_s3;
        var_s3 = trynexttile;
    #ifdef DEBUG
        assert(intersections != 0);
        assert(intersections != 3);
        osSyncPrintf("sf: stanLineDo %d   %5.1f %5.1f %5.1f %5.1f  %s %s %s\n", 3, start_x, start_z, dest_x, dest_z, GetStanRoomID(*tilestack), GetStanRoomID(puVar3));
    #endif
        if ((var_s3 ^ trynexttile) == 0)
        {
            var_s4 = 0;
        }
        if (var_s4 == 0)
        {
            return TRUE;
        }
        sp8C += 1;
        if ((((sp8C < 0x1F5) ^ 1) != 0) || (trynexttile == 0) || (var_s1 = 0, (var_s4 == 0)))
        {
            stanSavedColl_tile = var_s6;
            stanSavedColl_pointI = var_fp;
            #ifdef DEBUG
            osSyncPrintf("stanLine: Looping; ret=0\n");
            #endif
            return FALSE;
        }
        *tileStack = trynexttile;
        var_s4 = 0;
    }
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F0B0914
/* 0E5444 7F0B0914 27BDFF48 */  addiu $sp, $sp, -0xb8
/* 0E5448 7F0B0918 F7B40030 */  sdc1  $f20, 0x30($sp)
/* 0E544C 7F0B091C 3C018004 */  lui   $at, %hi(level_scale)
/* 0E5450 7F0B0920 4485A000 */  mtc1  $a1, $f20
/* 0E5454 7F0B0924 C4200F44 */  lwc1  $f0, %lo(level_scale)($at)
/* 0E5458 7F0B0928 F7B60038 */  sdc1  $f22, 0x38($sp)
/* 0E545C 7F0B092C 4486B000 */  mtc1  $a2, $f22
/* 0E5460 7F0B0930 4600A502 */  mul.s $f20, $f20, $f0
/* 0E5464 7F0B0934 F7B80040 */  sdc1  $f24, 0x40($sp)
/* 0E5468 7F0B0938 4487C000 */  mtc1  $a3, $f24
/* 0E546C 7F0B093C 4600B582 */  mul.s $f22, $f22, $f0
/* 0E5470 7F0B0940 F7BA0048 */  sdc1  $f26, 0x48($sp)
/* 0E5474 7F0B0944 C7BA00C8 */  lwc1  $f26, 0xc8($sp)
/* 0E5478 7F0B0948 4600C602 */  mul.s $f24, $f24, $f0
/* 0E547C 7F0B094C AFBF0084 */  sw    $ra, 0x84($sp)
/* 0E5480 7F0B0950 AFBE0080 */  sw    $fp, 0x80($sp)
/* 0E5484 7F0B0954 4600D682 */  mul.s $f26, $f26, $f0
/* 0E5488 7F0B0958 AFB7007C */  sw    $s7, 0x7c($sp)
/* 0E548C 7F0B095C AFB60078 */  sw    $s6, 0x78($sp)
/* 0E5490 7F0B0960 AFB50074 */  sw    $s5, 0x74($sp)
/* 0E5494 7F0B0964 AFB40070 */  sw    $s4, 0x70($sp)
/* 0E5498 7F0B0968 AFB3006C */  sw    $s3, 0x6c($sp)
/* 0E549C 7F0B096C AFB20068 */  sw    $s2, 0x68($sp)
/* 0E54A0 7F0B0970 AFB10064 */  sw    $s1, 0x64($sp)
/* 0E54A4 7F0B0974 AFB00060 */  sw    $s0, 0x60($sp)
/* 0E54A8 7F0B0978 F7BE0058 */  sdc1  $f30, 0x58($sp)
/* 0E54AC 7F0B097C F7BC0050 */  sdc1  $f28, 0x50($sp)
/* 0E54B0 7F0B0980 AFA400B8 */  sw    $a0, 0xb8($sp)
/* 0E54B4 7F0B0984 8C820000 */  lw    $v0, ($a0)
/* 0E54B8 7F0B0988 4616D701 */  sub.s $f28, $f26, $f22
/* 0E54BC 7F0B098C 00807025 */  move  $t6, $a0
/* 0E54C0 7F0B0990 0000A825 */  move  $s5, $zero
/* 0E54C4 7F0B0994 AFA0008C */  sw    $zero, 0x8c($sp)
/* 0E54C8 7F0B0998 4600E707 */  neg.s $f28, $f28
/* 0E54CC 7F0B099C 4614C781 */  sub.s $f30, $f24, $f20
/* 0E54D0 7F0B09A0 00008825 */  move  $s1, $zero
/* 0E54D4 7F0B09A4 0000A025 */  move  $s4, $zero
/* 0E54D8 7F0B09A8 8FBE0098 */  lw    $fp, 0x98($sp)
/* 0E54DC 7F0B09AC 00409825 */  move  $s3, $v0
/* 0E54E0 7F0B09B0 0040B025 */  move  $s6, $v0
/* 0E54E4 7F0B09B4 0040B825 */  move  $s7, $v0
.L7F0B09B8:
/* 0E54E8 7F0B09B8 8FAF00CC */  lw    $t7, 0xcc($sp)
/* 0E54EC 7F0B09BC 02602025 */  move  $a0, $s3
/* 0E54F0 7F0B09C0 51E00005 */  beql  $t7, $zero, .L7F0B09D8
/* 0E54F4 7F0B09C4 86640006 */   lh    $a0, 6($s3)
/* 0E54F8 7F0B09C8 02C02825 */  move  $a1, $s6
/* 0E54FC 7F0B09CC 01E0F809 */  jalr  $t7
/* 0E5500 7F0B09D0 8FA600D0 */  lw    $a2, 0xd0($sp)
/* 0E5504 7F0B09D4 86640006 */  lh    $a0, 6($s3)
.L7F0B09D8:
/* 0E5508 7F0B09D8 02608025 */  move  $s0, $s3
/* 0E550C 7F0B09DC 0004C303 */  sra   $t8, $a0, 0xc
/* 0E5510 7F0B09E0 3319000F */  andi  $t9, $t8, 0xf
/* 0E5514 7F0B09E4 1B200051 */  blez  $t9, .L7F0B0B2C
/* 0E5518 7F0B09E8 03202025 */   move  $a0, $t9
.L7F0B09EC:
/* 0E551C 7F0B09EC 26320001 */  addiu $s2, $s1, 1
/* 0E5520 7F0B09F0 0244001A */  div   $zero, $s2, $a0
/* 0E5524 7F0B09F4 00001810 */  mfhi  $v1
/* 0E5528 7F0B09F8 000358C0 */  sll   $t3, $v1, 3
/* 0E552C 7F0B09FC 026B1021 */  addu  $v0, $s3, $t3
/* 0E5530 7F0B0A00 8445000C */  lh    $a1, 0xc($v0)
/* 0E5534 7F0B0A04 8608000C */  lh    $t0, 0xc($s0)
/* 0E5538 7F0B0A08 84490008 */  lh    $t1, 8($v0)
/* 0E553C 7F0B0A0C 860A0008 */  lh    $t2, 8($s0)
/* 0E5540 7F0B0A10 00A86023 */  subu  $t4, $a1, $t0
/* 0E5544 7F0B0A14 448C3000 */  mtc1  $t4, $f6
/* 0E5548 7F0B0A18 012A6823 */  subu  $t5, $t1, $t2
/* 0E554C 7F0B0A1C 448D8000 */  mtc1  $t5, $f16
/* 0E5550 7F0B0A20 46803220 */  cvt.s.w $f8, $f6
/* 0E5554 7F0B0A24 44802000 */  mtc1  $zero, $f4
/* 0E5558 7F0B0A28 14800002 */  bnez  $a0, .L7F0B0A34
/* 0E555C 7F0B0A2C 00000000 */   nop
/* 0E5560 7F0B0A30 0007000D */  break 7
.L7F0B0A34:
/* 0E5564 7F0B0A34 2401FFFF */  li    $at, -1
/* 0E5568 7F0B0A38 14810004 */  bne   $a0, $at, .L7F0B0A4C
/* 0E556C 7F0B0A3C 3C018000 */   lui   $at, 0x8000
/* 0E5570 7F0B0A40 16410002 */  bne   $s2, $at, .L7F0B0A4C
/* 0E5574 7F0B0A44 00000000 */   nop
/* 0E5578 7F0B0A48 0006000D */  break 6
.L7F0B0A4C:
/* 0E557C 7F0B0A4C 468084A0 */  cvt.s.w $f18, $f16
/* 0E5580 7F0B0A50 461E4282 */  mul.s $f10, $f8, $f30
/* 0E5584 7F0B0A54 00000000 */  nop
/* 0E5588 7F0B0A58 4612E182 */  mul.s $f6, $f28, $f18
/* 0E558C 7F0B0A5C 46065200 */  add.s $f8, $f10, $f6
/* 0E5590 7F0B0A60 4604403E */  c.le.s $f8, $f4
/* 0E5594 7F0B0A64 00000000 */  nop
/* 0E5598 7F0B0A68 45000028 */  bc1f  .L7F0B0B0C
/* 0E559C 7F0B0A6C 00000000 */   nop
/* 0E55A0 7F0B0A70 448A8000 */  mtc1  $t2, $f16
/* 0E55A4 7F0B0A74 44885000 */  mtc1  $t0, $f10
/* 0E55A8 7F0B0A78 44892000 */  mtc1  $t1, $f4
/* 0E55AC 7F0B0A7C 468084A0 */  cvt.s.w $f18, $f16
/* 0E55B0 7F0B0A80 44858000 */  mtc1  $a1, $f16
/* 0E55B4 7F0B0A84 9602000E */  lhu   $v0, 0xe($s0)
/* 0E55B8 7F0B0A88 4406C000 */  mfc1  $a2, $f24
/* 0E55BC 7F0B0A8C 4407D000 */  mfc1  $a3, $f26
/* 0E55C0 7F0B0A90 468051A0 */  cvt.s.w $f6, $f10
/* 0E55C4 7F0B0A94 E7B20010 */  swc1  $f18, 0x10($sp)
/* 0E55C8 7F0B0A98 00027103 */  sra   $t6, $v0, 4
/* 0E55CC 7F0B0A9C 000E782B */  sltu  $t7, $zero, $t6
/* 0E55D0 7F0B0AA0 AFAF0020 */  sw    $t7, 0x20($sp)
/* 0E55D4 7F0B0AA4 46802220 */  cvt.s.w $f8, $f4
/* 0E55D8 7F0B0AA8 E7A60014 */  swc1  $f6, 0x14($sp)
/* 0E55DC 7F0B0AAC 4600A306 */  mov.s $f12, $f20
/* 0E55E0 7F0B0AB0 4600B386 */  mov.s $f14, $f22
/* 0E55E4 7F0B0AB4 468084A0 */  cvt.s.w $f18, $f16
/* 0E55E8 7F0B0AB8 E7A80018 */  swc1  $f8, 0x18($sp)
/* 0E55EC 7F0B0ABC 0FC2C1EF */  jal   sub_GAME_7F0B07BC
/* 0E55F0 7F0B0AC0 E7B2001C */   swc1  $f18, 0x1c($sp)
/* 0E55F4 7F0B0AC4 50400012 */  beql  $v0, $zero, .L7F0B0B10
/* 0E55F8 7F0B0AC8 86640006 */   lh    $a0, 6($s3)
/* 0E55FC 7F0B0ACC 9603000E */  lhu   $v1, 0xe($s0)
/* 0E5600 7F0B0AD0 3C198004 */  lui   $t9, %hi(standTileStart)
/* 0E5604 7F0B0AD4 8F390F58 */  lw    $t9, %lo(standTileStart)($t9)
/* 0E5608 7F0B0AD8 0003C0C0 */  sll   $t8, $v1, 3
/* 0E560C 7F0B0ADC 26940001 */  addiu $s4, $s4, 1
/* 0E5610 7F0B0AE0 03191021 */  addu  $v0, $t8, $t9
/* 0E5614 7F0B0AE4 12C20009 */  beq   $s6, $v0, .L7F0B0B0C
/* 0E5618 7F0B0AE8 00000000 */   nop
/* 0E561C 7F0B0AEC 12E20007 */  beq   $s7, $v0, .L7F0B0B0C
/* 0E5620 7F0B0AF0 00000000 */   nop
/* 0E5624 7F0B0AF4 00035903 */  sra   $t3, $v1, 4
/* 0E5628 7F0B0AF8 11600003 */  beqz  $t3, .L7F0B0B08
/* 0E562C 7F0B0AFC 0220F025 */   move  $fp, $s1
/* 0E5630 7F0B0B00 10000002 */  b     .L7F0B0B0C
/* 0E5634 7F0B0B04 0040A825 */   move  $s5, $v0
.L7F0B0B08:
/* 0E5638 7F0B0B08 0000A825 */  move  $s5, $zero
.L7F0B0B0C:
/* 0E563C 7F0B0B0C 86640006 */  lh    $a0, 6($s3)
.L7F0B0B10:
/* 0E5640 7F0B0B10 02408825 */  move  $s1, $s2
/* 0E5644 7F0B0B14 26100008 */  addiu $s0, $s0, 8
/* 0E5648 7F0B0B18 00046303 */  sra   $t4, $a0, 0xc
/* 0E564C 7F0B0B1C 318D000F */  andi  $t5, $t4, 0xf
/* 0E5650 7F0B0B20 024D082A */  slt   $at, $s2, $t5
/* 0E5654 7F0B0B24 1420FFB1 */  bnez  $at, .L7F0B09EC
/* 0E5658 7F0B0B28 01A02025 */   move  $a0, $t5
.L7F0B0B2C:
/* 0E565C 7F0B0B2C 02C0B825 */  move  $s7, $s6
/* 0E5660 7F0B0B30 0260B025 */  move  $s6, $s3
/* 0E5664 7F0B0B34 02751026 */  xor   $v0, $s3, $s5
/* 0E5668 7F0B0B38 14400002 */  bnez  $v0, .L7F0B0B44
/* 0E566C 7F0B0B3C 02A09825 */   move  $s3, $s5
/* 0E5670 7F0B0B40 0000A025 */  move  $s4, $zero
.L7F0B0B44:
/* 0E5674 7F0B0B44 16800003 */  bnez  $s4, .L7F0B0B54
/* 0E5678 7F0B0B48 8FAE008C */   lw    $t6, 0x8c($sp)
/* 0E567C 7F0B0B4C 10000013 */  b     .L7F0B0B9C
/* 0E5680 7F0B0B50 24020001 */   li    $v0, 1
.L7F0B0B54:
/* 0E5684 7F0B0B54 29C201F5 */  slti  $v0, $t6, 0x1f5
/* 0E5688 7F0B0B58 38420001 */  xori  $v0, $v0, 1
/* 0E568C 7F0B0B5C 25CF0001 */  addiu $t7, $t6, 1
/* 0E5690 7F0B0B60 14400005 */  bnez  $v0, .L7F0B0B78
/* 0E5694 7F0B0B64 AFAF008C */   sw    $t7, 0x8c($sp)
/* 0E5698 7F0B0B68 12A00003 */  beqz  $s5, .L7F0B0B78
/* 0E569C 7F0B0B6C 8FB800B8 */   lw    $t8, 0xb8($sp)
/* 0E56A0 7F0B0B70 16800007 */  bnez  $s4, .L7F0B0B90
/* 0E56A4 7F0B0B74 00008825 */   move  $s1, $zero
.L7F0B0B78:
/* 0E56A8 7F0B0B78 3C018008 */  lui   $at, %hi(stanSavedColl_tile)
/* 0E56AC 7F0B0B7C AC36B9E4 */  sw    $s6, %lo(stanSavedColl_tile)($at)
/* 0E56B0 7F0B0B80 3C018008 */  lui   $at, %hi(stanSavedColl_pointI)
/* 0E56B4 7F0B0B84 AC3EB9E8 */  sw    $fp, %lo(stanSavedColl_pointI)($at)
/* 0E56B8 7F0B0B88 10000004 */  b     .L7F0B0B9C
/* 0E56BC 7F0B0B8C 00001025 */   move  $v0, $zero
.L7F0B0B90:
/* 0E56C0 7F0B0B90 AF150000 */  sw    $s5, ($t8)
/* 0E56C4 7F0B0B94 1000FF88 */  b     .L7F0B09B8
/* 0E56C8 7F0B0B98 0000A025 */   move  $s4, $zero
.L7F0B0B9C:
/* 0E56CC 7F0B0B9C 8FBF0084 */  lw    $ra, 0x84($sp)
/* 0E56D0 7F0B0BA0 D7B40030 */  ldc1  $f20, 0x30($sp)
/* 0E56D4 7F0B0BA4 D7B60038 */  ldc1  $f22, 0x38($sp)
/* 0E56D8 7F0B0BA8 D7B80040 */  ldc1  $f24, 0x40($sp)
/* 0E56DC 7F0B0BAC D7BA0048 */  ldc1  $f26, 0x48($sp)
/* 0E56E0 7F0B0BB0 D7BC0050 */  ldc1  $f28, 0x50($sp)
/* 0E56E4 7F0B0BB4 D7BE0058 */  ldc1  $f30, 0x58($sp)
/* 0E56E8 7F0B0BB8 8FB00060 */  lw    $s0, 0x60($sp)
/* 0E56EC 7F0B0BBC 8FB10064 */  lw    $s1, 0x64($sp)
/* 0E56F0 7F0B0BC0 8FB20068 */  lw    $s2, 0x68($sp)
/* 0E56F4 7F0B0BC4 8FB3006C */  lw    $s3, 0x6c($sp)
/* 0E56F8 7F0B0BC8 8FB40070 */  lw    $s4, 0x70($sp)
/* 0E56FC 7F0B0BCC 8FB50074 */  lw    $s5, 0x74($sp)
/* 0E5700 7F0B0BD0 8FB60078 */  lw    $s6, 0x78($sp)
/* 0E5704 7F0B0BD4 8FB7007C */  lw    $s7, 0x7c($sp)
/* 0E5708 7F0B0BD8 8FBE0080 */  lw    $fp, 0x80($sp)
/* 0E570C 7F0B0BDC 03E00008 */  jr    $ra
/* 0E5710 7F0B0BE0 27BD00B8 */   addiu $sp, $sp, 0xb8
)
#endif



/**
 * Name: walkTilesBetweenPoints_NoCallback
 * Address 0x7F0B0BE4.
*/
s32 walkTilesBetweenPoints_NoCallback(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z)
{
    return sub_GAME_7F0B0914(tileStack, start_x, start_z, dest_x, dest_z, 0, 0);
}




/**
 * Name: walkTilesBetweenPoints_NotingRooms
 * Address 0x7F0B0BE4.
*/
s32 sub_GAME_7F0B0C24(StandTile **tileStack, f32 start_x, f32 start_z, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 *rtnCountSize, s32 maxBufSize)
{
    struct StandTileWalkCallbackRecord callbackData;
    s32 rtn;


    callbackData.roomBuf = roomBuffer;
    callbackData.count = 0;
    callbackData.bufMax = maxBufSize;
    callbackData.lastRoom = -1;

    rtn = sub_GAME_7F0B0914(tileStack, start_x, start_z, dest_x, dest_z, noteTileRoomIfDifferentToPrev, &callbackData);

    *rtnCountSize = callbackData.count;
    return rtn;
}


/*
* Address: 0x0x7f0b0c98
*/
void noteTileRoomIfDifferentToPrev(StandTile *tile, StandTile *unused, struct StandTileWalkCallbackRecord *data)
{
    s32 newRoom;

    if (tile->room != data->lastRoom && data->count < data->bufMax)
    {
        newRoom = (s32)tile->room;
        *data->roomBuf = newRoom;
        data->lastRoom = newRoom;
        data->roomBuf += 1;
        data->count += 1;
    }

    return;
}



/*
* Address: 0x7f0b0cec
*/
void noteTileRoomIfDifferentToPrev_2(StandTile *tile, StandTile *unused, struct StandTileWalkCallbackRecord *data) {
    noteTileRoomIfDifferentToPrev(tile, unused, data);
}


/**
 * Builds a list of room IDs between a start position and a destination position.
 * Only used for objects on set paths e.g. patrolling guards.
 */
s32 sub_GAME_7F0B0D0C(StandTile *tile, f32 start_x, f32 start_z, StandTile **destTile, f32 dest_x, f32 dest_z, s32 *roomBuffer, s32 maxBufSize)
{
    StandTile *savedTile;
    s32 count;

    savedTile = tile;
    count = 0;

    if (*destTile != NULL) {
        u8 roomA; // Source tile's room
        u8 roomB; // Destination tile's room

        roomA = tile->room;
        roomB = (*destTile)->room;

        // Fast path: start and destination tiles are in the same room, return a count of 1.
        if (roomB == roomA) {  
            roomBuffer[0] = roomA;
            return 1;
        }

        // Next fastest case: both rooms directly connected by a portal, write two room IDs and return a count of 2.
        if (bgRoomsSharePortal(roomA & 0xff, roomB & 0xff)) { 
            roomBuffer[0] = tile->room;
            roomBuffer[1] = (*destTile)->room;
            return 2;
        }
    }

    /**
     * Full path check needed. Find the rooms between the points, store them in roomBuffer, and save the number of rooms in count.
     * If the path check fails, return 0.
     */
    if (!sub_GAME_7F0B0C24(&savedTile, start_x, start_z, dest_x, dest_z, roomBuffer, &count, maxBufSize)) {
        return 0;
    }

    if (maxBufSize < count) {
        count = maxBufSize;
    }

    if (*destTile == NULL) {
        *destTile = savedTile;
    }

    if (savedTile != *destTile) {
        #ifdef DEBUG
        osSyncPrintf("stan %s(%d) != %s(%d) from=%s\n", GetStanRoomID(savedTile),
        /*funcForTileNumber(savedTile)*/, GetStanRoomID(tile), /*funcForTileNumber(tile)*/, GetStanRoomID(roomBuffer));
        #endif
        return 0;
    }

    return count;
}


/**
 * Can change global variables:
 *
 * - D_800413BC
 * - stanSavedColl_pntA
 * - stanSavedColl_pntB
 * - stanSavedColl_tile
 * - stanSavedColl_pointI
 * - stanSavedColl_posData
 *
 * US address 7F0B0E24.
 *
 * 'testLineUnobstructed'
*/
s32 stanTestLineUnobstructed(StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, f32 unkHeight, f32 unkA, f32 unkB, f32 unkC)
{
    struct PropRecord *prop;
    s32 retval; // sp158
    StandTile *sp154; // sp154
    struct coord2d sp14C;
    struct coord2d sp144;
    f32 sp140;
    s32 point_index;
    struct coord2d sp134;
    struct coord2d sp12C;
    s32 loop_flag;
    s32 sp124; // sp124
    s32 padding;
    s32 spD0[0x14]; //spD0
    s32 spCC; // spCC
    s32 next;
    f32 spC4;
    f32 spC0;
    f32 temp_f0_2;
    s16 *spB8;
    struct rect4f *polygon; // spB4
    s32 numvertices0; // spB0
    //f32 unused2;
    s32 i;
    f32 temp_f0;
    f32 spA4;
    f32 spA0; // spA0
    f32 temp_f2;
    s32 already_set;

    sp140 = 1.0f;
    sp124 = 0;
    spCC = (unkA <= unkHeight);
    already_set = 0;

    sp154 = *pTile;
    sp14C.f[0] = p_x;
    sp14C.f[1] = p_z;
    sp144.f[0] = dest_x;
    sp144.f[1] = dest_z;

    retval = sub_GAME_7F0B0C24(&sp154, p_x, p_z, dest_x, dest_z, &spD0[0], &sp124, 0x14);


    if (sp124 > 20)
    {
        #ifdef DEBUG
            osSyncPrintf("stanLineObjType: %d rooms is more than %d\n", retval, 20);
#endif

        sp124 = 20;
    }

    if (retval == 0)
    {
        s32 padding[2];

        point_index = (stanSavedColl_pointI + 1) % (s32)((stanSavedColl_tile->tail.half >> 0xC) & 0xF);
        D_800413BC = 1;

        stanSavedColl_pntA.f[0] = (f32) stanSavedColl_tile->points[stanSavedColl_pointI].x * inv_level_scale;
        stanSavedColl_pntA.f[1] = (f32) stanSavedColl_tile->points[stanSavedColl_pointI].z * inv_level_scale;

        stanSavedColl_pntB.f[0] = (f32) stanSavedColl_tile->points[point_index].x * inv_level_scale;
        stanSavedColl_pntB.f[1] = (f32) stanSavedColl_tile->points[point_index].z * inv_level_scale;

        sp140 = calculateSegmentIntersectionFraction(&sp14C, &sp144, &stanSavedColl_pntA, &stanSavedColl_pntB);
    }
    else
    {
        //
    }

    stanSavedColl_posData = NULL;

    if (cdtypes != 0)
    {
        spD0[sp124] = -1;
        roomGetProps((s32 *)&spD0);

        for (spB8 = ptr_list_object_lookup_indices; *spB8 >= 0; spB8++)
        {
            prop = &pos_data_entry[*spB8];

            if (propIsOfCdType(prop, cdtypes) != 0)
            {
                chraiGetCollisionBounds(prop, &polygon, &numvertices0, &spA4, &spA0);

                if (numvertices0 > 0)
                {
                    for (i = 0; i < numvertices0; i++)
                    {
                        next = (i + 1) % numvertices0;

                        if (sub_GAME_7F0B0688(p_x, p_z, dest_x, dest_z, polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1]) != 0)
                        {
                            sp134.f[0] = polygon->points[i].f[0];
                            sp134.f[1] = polygon->points[i].f[1];
                            sp12C.f[0] = polygon->points[next].f[0];
                            sp12C.f[1] = polygon->points[next].f[1];

                            temp_f0 = calculateSegmentIntersectionFraction(&sp14C, &sp144, &sp134, &sp12C);

                            if (temp_f0 < sp140)
                            {
                                loop_flag = 1;

                                if (spCC != 0)
                                {
                                    if (already_set == 0)
                                    {
                                        already_set = 1;

                                        if (unkC <= unkB)
                                        {
                                            spC4 = unkB - unkHeight;
                                            spC0 = unkC - unkA;
                                        }
                                        else
                                        {
                                            if (sp140 < 1.0f)
                                            {
                                                dest_x -= p_x;
                                                dest_x *= sp140;
                                                dest_x = p_x + dest_x;

                                                dest_z -= p_z;
                                                dest_z *= sp140;
                                                dest_z = p_z + dest_z;
                                            }

                                            temp_f0_2 = stanGetPositionYValue(*pTile, p_x, p_z);
                                            unkHeight += temp_f0_2;
                                            unkA += temp_f0_2;
                                            temp_f2 = (stanGetPositionYValue(sp154, dest_x, dest_z) - temp_f0_2) / sp140;
                                            spC0 = temp_f2;
                                            spC4 = temp_f2;
                                        }
                                    }

                                    if ((spA4 <= ((spC0 * temp_f0) + unkA)) || (((spC4 * temp_f0) + unkHeight) <= spA0))
                                    {
                                        loop_flag = 0;
                                    }
                                }

                                if (loop_flag != 0)
                                {
                                    retval = 0;
                                    sp140 = temp_f0;
                                    D_800413BC = 1;
                                    stanSavedColl_pntA = sp134;
                                    stanSavedColl_pntB = sp12C;
                                    stanSavedColl_tile = NULL;
                                    stanSavedColl_pointI = 0;
                                    stanSavedColl_posData = prop;
                                    sp154 = NULL;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (sp154 == NULL)
    {
        sp154 = *pTile;

        dest_x -= p_x;
        dest_x *= sp140;
        dest_x = p_x + dest_x;

        dest_z -= p_z;
        dest_z *= sp140;
        dest_z = p_z + dest_z;
        /*stanlineret = */ walkTilesBetweenPoints_NoCallback(&sp154, p_x, p_z, dest_x, dest_z);
        #ifdef DEBUG
        assert(stanlineret==1)
        #endif
    }

    *pTile = sp154;
    stanSavedColl_someMin = sp140;

    return retval;
}


PropRecord *sub_GAME_7F0B1410(StandTile *t, f32 start_x, f32 start_z, f32 end_x, f32 end_z, s32 cdtypes)
{
    f32 frac;
    PropRecord *prop;
    coord2d lineStart;
    coord2d lineEnd;
    struct coord2d *tmp;
    s32 pad;
    coord2d edgeStart;
    coord2d edgeEnd;
    s32 i;
    StandTile *tile;
    s32 roomCount;
    s32 roomBuffer[21];
    s16 *propIndexPtr;
    struct rect4f *polygon;
    s32 numEdges;
    PropRecord *bestProp;
    s32 next;
    f32 bestFrac;

    bestProp = NULL;
    bestFrac = 1.0f;

    tile = t;
    roomCount = 0;

    sub_GAME_7F0B0C24(&tile, start_x, start_z, end_x, end_z, roomBuffer, &roomCount, 20);

    if (roomCount >= 21)
    {
        // The comment below was in the unmatched function. I have left it where I think it was most likely meant to go.
        //osSyncPrintf("stanLineDoor: %d rooms is more than %d\n");
        roomCount = 20;
    }

    lineStart.f[0] = start_x;
    lineStart.f[1] = start_z;
    lineEnd.f[0] = end_x;
    lineEnd.f[1] = end_z;

    if (cdtypes != 0)
    {
        roomBuffer[roomCount] = -1;
        roomGetProps(roomBuffer);

        propIndexPtr = ptr_list_object_lookup_indices;

        // Fake but needed for matching.
        if (polygon);

        if (*propIndexPtr >= 0)
        {
            do
            {
                prop = &pos_data_entry[*propIndexPtr];

                if (propIsOfCdType(prop, cdtypes))
                {
                    chraiGetCollisionBoundsWithoutY(prop, &polygon, &numEdges);

                    if (numEdges > 0)
                    {
                        i = 0;

                        while (i < numEdges)
                        {
                            next = (i + 1) % numEdges;

                            if (sub_GAME_7F0B0688(start_x, start_z, end_x, end_z, polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1]))
                            {
                                edgeStart.f[0] = polygon->points[i].f[0];
                                tmp = &polygon->points[i];
                                edgeStart.f[1] = (*tmp).f[1];

                                edgeEnd.f[0] = polygon->points[next].f[0];
                                edgeEnd.f[1] = polygon->points[next].f[1];

                                frac = calculateSegmentIntersectionFraction(&lineStart, &lineEnd, &edgeStart, &edgeEnd);

                                if (frac < bestFrac)
                                {
                                    bestFrac = frac;
                                    bestProp = prop;
                                }
                            }

                            i++;
                        }
                    }
                }

                propIndexPtr++;
            }
            while (*propIndexPtr >= 0);
        }
    }

    return bestProp;
}


/**
 * Address: 7F0B16C4
 * 
 * Computes the signed perpendicular distance from point P to the infinite
 * line that goes through point A and point B.
 * The sign indicates which side of the line the point is on.
 */
f32 stanGetSignedPointLineDistance(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    u32 stack[8];
    f32 result; //d

    result = sqrtf((b_x - a_x) * (b_x - a_x) + (b_z - a_z) * (b_z - a_z));

    if (result == 0.0f)
    {
        return sqrtf((p_x - b_x) * (p_x - b_x) + (p_z - b_z) * (p_z - b_z));
    }
    #ifdef DEBUG
    assert(d>0.0F);
    #endif
    return ((b_z - a_z) * (p_x - a_x) + -(b_x - a_x) * (p_z - a_z)) / result;
}


f32 distBetweenPoints2d(f32 o_x,f32 o_z,f32 p_x,f32 p_z)
{
    p_x -= o_x;
    p_z -= o_z;
    return sqrtf(p_x * p_x + p_z * p_z);
}


/**
 * Address: 7F0B17E4
 * 
 * Tests whether a point P's perpendicular projection onto the infinite line
 * going through points A and B falls inside the finite edge segment.
 */
bool stanPointProjectsOntoEdge(f32 a_x, f32 a_z, f32 b_x, f32 b_z, f32 p_x, f32 p_z)
{
    f32 f0;
    f32 f2;
    f32 f16;
    f32 f18;

    p_x -= a_x;
    p_z -= a_z;

    f0 = b_x - a_x;
    f2 = b_z - a_z;

    f16 = p_x * f0 + p_z * f2;
    f18 = f0 * f0 + f2 * f2;

    return (f18 < f16 && f16 < 0) || (f16 > 0 && f16 < f18);
}


/**
 * Can change global variables
 * - D_800413BC
 * - stanSavedColl_pntA
 * - stanSavedColl_pntB
 * - stanSavedColl_tile
 * - stanSavedColl_pointI
 * - stanSavedColl_posData
 *
 * US address 7F0B18B8.
 * Perfect Dark cdTestVolume (from context)
*/
s32 stanTestVolume(StandTile **arg0, f32 arg1, f32 arg2, f32 arg3, s32 cdtypes, f32 arg5, f32 arg6)
{
    s32 i; // stack ??
    f32 var_f20; // stack ??
    f32 var_f24; // stack ??
    s32 temp_v0; // stack ??
    s32 next; // stack ??

    s32 sp108;
    f32 temp_f0;  // stack ??
    s16 *sp100;
    s32 spFC;
    struct PropRecord *prop; // no stack
    s32 spA8[0x14];
    struct rect4f *polygon;
    s32 numvertices0;  // spa0
    f32 temp_f0_3; // stack ??
    f32 temp_f0_2; // stack ??
    f32 sp94;
    f32 sp90;

    s32 padding1;
    s32 padding2;

    sp108 = (arg6 <= arg5);

    spFC = 0;

    temp_v0 = sub_GAME_7F0B21B0(arg0, arg1, arg2, arg3, &spA8[0], &spFC, 20);
    if (temp_v0 >= 0)
    {
        return temp_v0;
    }


    if (spFC > 20)
    {
        #ifdef DEBUG
            osSyncPrintf("stanCircleLegalXFObjTypeY: %d rooms is more than %d\n",spFC,20);
        #endif
        spFC = 20;
    }

    stanSavedColl_posData = NULL;

    if (cdtypes)
    {
        if (sp108)
        {
            temp_f0 = stanGetPositionYValue(*arg0, arg1, arg2);
            arg5 += temp_f0;
            arg6 += temp_f0;
        }

        spA8[spFC] = -1;
        roomGetProps(&spA8[0]);

        for (sp100 = ptr_list_object_lookup_indices; *sp100 >= 0; sp100++)
        {
            prop = &pos_data_entry[*sp100];

            if (propIsOfCdType(prop, cdtypes) != 0)
            {
                chraiGetCollisionBounds(prop, &polygon, &numvertices0, &sp94, &sp90);
                if ((numvertices0 > 0) && ((sp108 == 0) || ((sp90 <= arg5) && (arg6 <= sp94))))
                {
                    var_f24 = -1.0f;

                    i=0;
                    while(1)
                    {
                        next = (i + 1) % numvertices0;

                        var_f20 = stanGetSignedPointLineDistance(polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2);

                        if (var_f20 < 0.0f)
                        {
                            var_f20 = -var_f20;
                        }

                        if (var_f24 < var_f20)
                        {
                            temp_f0_2 = distBetweenPoints2d(polygon->points[i].f[0], polygon->points[i].f[1], arg1, arg2);
                            temp_f0_3 = distBetweenPoints2d(polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2);

                            if ((var_f20 < arg3)
                                && (
                                    (temp_f0_2 < arg3)
                                    || (temp_f0_3 < arg3)
                                    || (stanPointProjectsOntoEdge(polygon->points[i].f[0], polygon->points[i].f[1], polygon->points[next].f[0], polygon->points[next].f[1], arg1, arg2) != 0)))
                            {
                                D_800413BC = 1;
                                var_f24 = var_f20;

                                stanSavedColl_pntA.f[0] = polygon->points[i].f[0];
                                stanSavedColl_pntA.f[1] = polygon->points[i].f[1];
                                stanSavedColl_pntB.f[0] = polygon->points[next].f[0];
                                stanSavedColl_pntB.f[1] = polygon->points[next].f[1];
                                stanSavedColl_tile = NULL;
                                stanSavedColl_pointI = 0;
                                stanSavedColl_posData = prop;
                            }
                        }

                        if (next == 0)
                        {
                            break;
                        }

                        i = next;
                    }

                    if (var_f24 > -1.0f)
                    {
                        return 2;
                    }
                }
            }
        }
    }

    return -2;
}



//stanResetHits
void stanResetHits(void) {
    stanSavedColl_tile = 0;
    stanSavedColl_pointI = 0;
    D_800413BC = 0;
}

StandTile *sub_GAME_7F0B1CE0(void)
{
    #ifdef DEBUG
        osSyncPrintf("Don\'t call stanCircleLegalHit()!\n");
    #endif
    return stanSavedColl_tile;
}

s32 sub_GAME_7F0B1CEC(void)
{
    #ifdef DEBUG
        osSyncPrintf("Don\'t call stanCircleLegalHitEdge()!\n");
    #endif

    return stanSavedColl_pointI;
}


void getTileEdgePoints(StandTile *tile, s32 pointI, coord3d *currPntRtn, coord3d *nextPointRtn)
{
    f32 scale;

    scale = inv_level_scale;

    currPntRtn->x = tile->points[pointI].x * scale;
    currPntRtn->y = tile->points[pointI].y * scale;
    currPntRtn->z = tile->points[pointI].z * scale;

    /**
     * This line could potentially become:
     * pointI = (pointI + 1) % STAN_POINT_COUNT(tile);
     * 
     * If STAN_POINT_COUNT were redefined as:
     * #define STAN_POINT_COUNT(tile) (((tile)->tail.half >> 12) & 0xf)
     * Something to consider?
     */
    pointI = (pointI + 1) % ((tile->tail.half >> 12) & 0xf);

    nextPointRtn->x = tile->points[pointI].x * scale;
    nextPointRtn->y = tile->points[pointI].y * scale;
    nextPointRtn->z = tile->points[pointI].z * scale;
}


#ifdef NONMATCHING
s32 sub_GAME_7F0B1DDC(void)
{
    #ifdef DEBUG
    if (cat>= 0x190)
    {
      osSyncPrintf("cat=%d !!!!!!!!!!!!!!!!!!!! NEED TO INCREASE ARRAY SIZE\n",cat);
    }
    #endif
    if (cat >= 0x320)
    {
      return 5; //error value?
    }
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F0B1DDC
/* 0E690C 7F0B1DDC 27BDFEB0 */  addiu $sp, $sp, -0x150
/* 0E6910 7F0B1DE0 F7BA0038 */  sdc1  $f26, 0x38($sp)
/* 0E6914 7F0B1DE4 3C018004 */  lui   $at, %hi(level_scale)
/* 0E6918 7F0B1DE8 4485D000 */  mtc1  $a1, $f26
/* 0E691C 7F0B1DEC C4200F44 */  lwc1  $f0, %lo(level_scale)($at)
/* 0E6920 7F0B1DF0 F7BC0040 */  sdc1  $f28, 0x40($sp)
/* 0E6924 7F0B1DF4 4486E000 */  mtc1  $a2, $f28
/* 0E6928 7F0B1DF8 4600D682 */  mul.s $f26, $f26, $f0
/* 0E692C 7F0B1DFC F7BE0048 */  sdc1  $f30, 0x48($sp)
/* 0E6930 7F0B1E00 4487F000 */  mtc1  $a3, $f30
/* 0E6934 7F0B1E04 4600E702 */  mul.s $f28, $f28, $f0
/* 0E6938 7F0B1E08 AFBF0074 */  sw    $ra, 0x74($sp)
/* 0E693C 7F0B1E0C AFBE0070 */  sw    $fp, 0x70($sp)
/* 0E6940 7F0B1E10 AFB7006C */  sw    $s7, 0x6c($sp)
/* 0E6944 7F0B1E14 AFB60068 */  sw    $s6, 0x68($sp)
/* 0E6948 7F0B1E18 AFB50064 */  sw    $s5, 0x64($sp)
/* 0E694C 7F0B1E1C AFB40060 */  sw    $s4, 0x60($sp)
/* 0E6950 7F0B1E20 AFB3005C */  sw    $s3, 0x5c($sp)
/* 0E6954 7F0B1E24 AFB20058 */  sw    $s2, 0x58($sp)
/* 0E6958 7F0B1E28 AFB10054 */  sw    $s1, 0x54($sp)
/* 0E695C 7F0B1E2C AFB00050 */  sw    $s0, 0x50($sp)
/* 0E6960 7F0B1E30 F7B80030 */  sdc1  $f24, 0x30($sp)
/* 0E6964 7F0B1E34 F7B60028 */  sdc1  $f22, 0x28($sp)
/* 0E6968 7F0B1E38 F7B40020 */  sdc1  $f20, 0x20($sp)
/* 0E696C 7F0B1E3C AFA000A4 */  sw    $zero, 0xa4($sp)
/* 0E6970 7F0B1E40 8C8E0000 */  lw    $t6, ($a0)
/* 0E6974 7F0B1E44 27AF00B0 */  addiu $t7, $sp, 0xb0
/* 0E6978 7F0B1E48 4600F782 */  mul.s $f30, $f30, $f0
/* 0E697C 7F0B1E4C 24140001 */  li    $s4, 1
/* 0E6980 7F0B1E50 AFAF0084 */  sw    $t7, 0x84($sp)
/* 0E6984 7F0B1E54 27B200B0 */  addiu $s2, $sp, 0xb0
/* 0E6988 7F0B1E58 8FB60164 */  lw    $s6, 0x164($sp)
/* 0E698C 7F0B1E5C 8FB7016C */  lw    $s7, 0x16c($sp)
/* 0E6990 7F0B1E60 8FBE0168 */  lw    $fp, 0x168($sp)
/* 0E6994 7F0B1E64 AFAE00B0 */  sw    $t6, 0xb0($sp)
/* 0E6998 7F0B1E68 8FB80084 */  lw    $t8, 0x84($sp)
.L7F0B1E6C:
/* 0E699C 7F0B1E6C 8FB900A4 */  lw    $t9, 0xa4($sp)
/* 0E69A0 7F0B1E70 8FAA0160 */  lw    $t2, 0x160($sp)
/* 0E69A4 7F0B1E74 8F130000 */  lw    $s3, ($t8)
/* 0E69A8 7F0B1E78 27090004 */  addiu $t1, $t8, 4
/* 0E69AC 7F0B1E7C 27280001 */  addiu $t0, $t9, 1
/* 0E69B0 7F0B1E80 AFA800A4 */  sw    $t0, 0xa4($sp)
/* 0E69B4 7F0B1E84 AFA90084 */  sw    $t1, 0x84($sp)
/* 0E69B8 7F0B1E88 11400004 */  beqz  $t2, .L7F0B1E9C
/* 0E69BC 7F0B1E8C 00008025 */   move  $s0, $zero
/* 0E69C0 7F0B1E90 02602025 */  move  $a0, $s3
/* 0E69C4 7F0B1E94 0140F809 */  jalr  $t2
/* 0E69C8 7F0B1E98 02E02825 */  move  $a1, $s7
.L7F0B1E9C:
/* 0E69CC 7F0B1E9C 86620006 */  lh    $v0, 6($s3)
/* 0E69D0 7F0B1EA0 00025B03 */  sra   $t3, $v0, 0xc
/* 0E69D4 7F0B1EA4 316C000F */  andi  $t4, $t3, 0xf
/* 0E69D8 7F0B1EA8 19800069 */  blez  $t4, .L7F0B2050
/* 0E69DC 7F0B1EAC 01801025 */   move  $v0, $t4
.L7F0B1EB0:
/* 0E69E0 7F0B1EB0 26150001 */  addiu $s5, $s0, 1
/* 0E69E4 7F0B1EB4 02A2001A */  div   $zero, $s5, $v0
/* 0E69E8 7F0B1EB8 00008810 */  mfhi  $s1
/* 0E69EC 7F0B1EBC 4406D000 */  mfc1  $a2, $f26
/* 0E69F0 7F0B1EC0 14400002 */  bnez  $v0, .L7F0B1ECC
/* 0E69F4 7F0B1EC4 00000000 */   nop
/* 0E69F8 7F0B1EC8 0007000D */  break 7
.L7F0B1ECC:
/* 0E69FC 7F0B1ECC 2401FFFF */  li    $at, -1
/* 0E6A00 7F0B1ED0 14410004 */  bne   $v0, $at, .L7F0B1EE4
/* 0E6A04 7F0B1ED4 3C018000 */   lui   $at, 0x8000
/* 0E6A08 7F0B1ED8 16A10002 */  bne   $s5, $at, .L7F0B1EE4
/* 0E6A0C 7F0B1EDC 00000000 */   nop
/* 0E6A10 7F0B1EE0 0006000D */  break 6
.L7F0B1EE4:
/* 0E6A14 7F0B1EE4 4407E000 */  mfc1  $a3, $f28
/* 0E6A18 7F0B1EE8 02602025 */  move  $a0, $s3
/* 0E6A1C 7F0B1EEC 0FC2BF47 */  jal   getShortest2dDispToInfTileEdge
/* 0E6A20 7F0B1EF0 02002825 */   move  $a1, $s0
/* 0E6A24 7F0B1EF4 4406D000 */  mfc1  $a2, $f26
/* 0E6A28 7F0B1EF8 4407E000 */  mfc1  $a3, $f28
/* 0E6A2C 7F0B1EFC 46000586 */  mov.s $f22, $f0
/* 0E6A30 7F0B1F00 02602025 */  move  $a0, $s3
/* 0E6A34 7F0B1F04 0FC2C019 */  jal   distToTilePnt2D
/* 0E6A38 7F0B1F08 02002825 */   move  $a1, $s0
/* 0E6A3C 7F0B1F0C 4406D000 */  mfc1  $a2, $f26
/* 0E6A40 7F0B1F10 4407E000 */  mfc1  $a3, $f28
/* 0E6A44 7F0B1F14 46000506 */  mov.s $f20, $f0
/* 0E6A48 7F0B1F18 02602025 */  move  $a0, $s3
/* 0E6A4C 7F0B1F1C 0FC2C019 */  jal   distToTilePnt2D
/* 0E6A50 7F0B1F20 02202825 */   move  $a1, $s1
/* 0E6A54 7F0B1F24 461EB03C */  c.lt.s $f22, $f30
/* 0E6A58 7F0B1F28 46000606 */  mov.s $f24, $f0
/* 0E6A5C 7F0B1F2C 45020042 */  bc1fl .L7F0B2038
/* 0E6A60 7F0B1F30 86620006 */   lh    $v0, 6($s3)
/* 0E6A64 7F0B1F34 461EA03C */  c.lt.s $f20, $f30
/* 0E6A68 7F0B1F38 00000000 */  nop
/* 0E6A6C 7F0B1F3C 4501000B */  bc1t  .L7F0B1F6C
/* 0E6A70 7F0B1F40 00000000 */   nop
/* 0E6A74 7F0B1F44 461E003C */  c.lt.s $f0, $f30
/* 0E6A78 7F0B1F48 02602025 */  move  $a0, $s3
/* 0E6A7C 7F0B1F4C 45010007 */  bc1t  .L7F0B1F6C
/* 0E6A80 7F0B1F50 00000000 */   nop
/* 0E6A84 7F0B1F54 4406D000 */  mfc1  $a2, $f26
/* 0E6A88 7F0B1F58 4407E000 */  mfc1  $a3, $f28
/* 0E6A8C 7F0B1F5C 0FC2C066 */  jal   stanPointProjectsOntoTileEdge
/* 0E6A90 7F0B1F60 02002825 */   move  $a1, $s0
/* 0E6A94 7F0B1F64 50400034 */  beql  $v0, $zero, .L7F0B2038
/* 0E6A98 7F0B1F68 86620006 */   lh    $v0, 6($s3)
.L7F0B1F6C:
/* 0E6A9C 7F0B1F6C 12C00008 */  beqz  $s6, .L7F0B1F90
/* 0E6AA0 7F0B1F70 02602025 */   move  $a0, $s3
/* 0E6AA4 7F0B1F74 4406B000 */  mfc1  $a2, $f22
/* 0E6AA8 7F0B1F78 4407A000 */  mfc1  $a3, $f20
/* 0E6AAC 7F0B1F7C 02002825 */  move  $a1, $s0
/* 0E6AB0 7F0B1F80 E7B80010 */  swc1  $f24, 0x10($sp)
/* 0E6AB4 7F0B1F84 02C0F809 */  jalr  $s6
/* 0E6AB8 7F0B1F88 AFB70014 */  sw    $s7, 0x14($sp)
/* 0E6ABC 7F0B1F8C 1440001B */  bnez  $v0, .L7F0B1FFC
.L7F0B1F90:
/* 0E6AC0 7F0B1F90 001068C0 */   sll   $t5, $s0, 3
/* 0E6AC4 7F0B1F94 026D7021 */  addu  $t6, $s3, $t5
/* 0E6AC8 7F0B1F98 95C4000E */  lhu   $a0, 0xe($t6)
/* 0E6ACC 7F0B1F9C 3C088004 */  lui   $t0, %hi(standTileStart)
/* 0E6AD0 7F0B1FA0 2682FFFF */  addiu $v0, $s4, -1
/* 0E6AD4 7F0B1FA4 00047903 */  sra   $t7, $a0, 4
/* 0E6AD8 7F0B1FA8 11E00014 */  beqz  $t7, .L7F0B1FFC
/* 0E6ADC 7F0B1FAC 0004C8C0 */   sll   $t9, $a0, 3
/* 0E6AE0 7F0B1FB0 8D080F58 */  lw    $t0, %lo(standTileStart)($t0)
/* 0E6AE4 7F0B1FB4 00402825 */  move  $a1, $v0
/* 0E6AE8 7F0B1FB8 0440000B */  bltz  $v0, .L7F0B1FE8
/* 0E6AEC 7F0B1FBC 03281821 */   addu  $v1, $t9, $t0
/* 0E6AF0 7F0B1FC0 0002C080 */  sll   $t8, $v0, 2
/* 0E6AF4 7F0B1FC4 27A900B0 */  addiu $t1, $sp, 0xb0
/* 0E6AF8 7F0B1FC8 03091021 */  addu  $v0, $t8, $t1
/* 0E6AFC 7F0B1FCC 8C4A0000 */  lw    $t2, ($v0)
.L7F0B1FD0:
/* 0E6B00 7F0B1FD0 2442FFFC */  addiu $v0, $v0, -4
/* 0E6B04 7F0B1FD4 0052082B */  sltu  $at, $v0, $s2
/* 0E6B08 7F0B1FD8 506A0017 */  beql  $v1, $t2, .L7F0B2038
/* 0E6B0C 7F0B1FDC 86620006 */   lh    $v0, 6($s3)
/* 0E6B10 7F0B1FE0 5020FFFB */  beql  $at, $zero, .L7F0B1FD0
/* 0E6B14 7F0B1FE4 8C4A0000 */   lw    $t2, ($v0)
.L7F0B1FE8:
/* 0E6B18 7F0B1FE8 00145880 */  sll   $t3, $s4, 2
/* 0E6B1C 7F0B1FEC 03AB6021 */  addu  $t4, $sp, $t3
/* 0E6B20 7F0B1FF0 AD8300B0 */  sw    $v1, 0xb0($t4)
/* 0E6B24 7F0B1FF4 1000000F */  b     .L7F0B2034
/* 0E6B28 7F0B1FF8 26940001 */   addiu $s4, $s4, 1
.L7F0B1FFC:
/* 0E6B2C 7F0B1FFC 3C018008 */  lui   $at, %hi(stanSavedColl_tile)
/* 0E6B30 7F0B2000 AC33B9E4 */  sw    $s3, %lo(stanSavedColl_tile)($at)
/* 0E6B34 7F0B2004 3C018008 */  lui   $at, %hi(stanSavedColl_pointI)
/* 0E6B38 7F0B2008 13C00008 */  beqz  $fp, .L7F0B202C
/* 0E6B3C 7F0B200C AC30B9E8 */   sw    $s0, %lo(stanSavedColl_pointI)($at)
/* 0E6B40 7F0B2010 27A400B0 */  addiu $a0, $sp, 0xb0
/* 0E6B44 7F0B2014 02802825 */  move  $a1, $s4
/* 0E6B48 7F0B2018 03C0F809 */  jalr  $fp
/* 0E6B4C 7F0B201C 02E03025 */  move  $a2, $s7
/* 0E6B50 7F0B2020 24010001 */  li    $at, 1
/* 0E6B54 7F0B2024 50410004 */  beql  $v0, $at, .L7F0B2038
/* 0E6B58 7F0B2028 86620006 */   lh    $v0, 6($s3)
.L7F0B202C:
/* 0E6B5C 7F0B202C 10000016 */  b     .L7F0B2088
/* 0E6B60 7F0B2030 24020002 */   li    $v0, 2
.L7F0B2034:
/* 0E6B64 7F0B2034 86620006 */  lh    $v0, 6($s3)
.L7F0B2038:
/* 0E6B68 7F0B2038 02A08025 */  move  $s0, $s5
/* 0E6B6C 7F0B203C 00026B03 */  sra   $t5, $v0, 0xc
/* 0E6B70 7F0B2040 31AE000F */  andi  $t6, $t5, 0xf
/* 0E6B74 7F0B2044 02AE082A */  slt   $at, $s5, $t6
/* 0E6B78 7F0B2048 1420FF99 */  bnez  $at, .L7F0B1EB0
/* 0E6B7C 7F0B204C 01C01025 */   move  $v0, $t6
.L7F0B2050:
/* 0E6B80 7F0B2050 2E810029 */  sltiu $at, $s4, 0x29
/* 0E6B84 7F0B2054 14200003 */  bnez  $at, .L7F0B2064
/* 0E6B88 7F0B2058 8FAF00A4 */   lw    $t7, 0xa4($sp)
/* 0E6B8C 7F0B205C 1000000A */  b     .L7F0B2088
/* 0E6B90 7F0B2060 24020005 */   li    $v0, 5
.L7F0B2064:
/* 0E6B94 7F0B2064 01F4082A */  slt   $at, $t7, $s4
/* 0E6B98 7F0B2068 5420FF80 */  bnezl $at, .L7F0B1E6C
/* 0E6B9C 7F0B206C 8FB80084 */   lw    $t8, 0x84($sp)
/* 0E6BA0 7F0B2070 13C00004 */  beqz  $fp, .L7F0B2084
/* 0E6BA4 7F0B2074 27A400B0 */   addiu $a0, $sp, 0xb0
/* 0E6BA8 7F0B2078 02802825 */  move  $a1, $s4
/* 0E6BAC 7F0B207C 03C0F809 */  jalr  $fp
/* 0E6BB0 7F0B2080 02E03025 */  move  $a2, $s7
.L7F0B2084:
/* 0E6BB4 7F0B2084 2402FFFE */  li    $v0, -2
.L7F0B2088:
/* 0E6BB8 7F0B2088 8FBF0074 */  lw    $ra, 0x74($sp)
/* 0E6BBC 7F0B208C D7B40020 */  ldc1  $f20, 0x20($sp)
/* 0E6BC0 7F0B2090 D7B60028 */  ldc1  $f22, 0x28($sp)
/* 0E6BC4 7F0B2094 D7B80030 */  ldc1  $f24, 0x30($sp)
/* 0E6BC8 7F0B2098 D7BA0038 */  ldc1  $f26, 0x38($sp)
/* 0E6BCC 7F0B209C D7BC0040 */  ldc1  $f28, 0x40($sp)
/* 0E6BD0 7F0B20A0 D7BE0048 */  ldc1  $f30, 0x48($sp)
/* 0E6BD4 7F0B20A4 8FB00050 */  lw    $s0, 0x50($sp)
/* 0E6BD8 7F0B20A8 8FB10054 */  lw    $s1, 0x54($sp)
/* 0E6BDC 7F0B20AC 8FB20058 */  lw    $s2, 0x58($sp)
/* 0E6BE0 7F0B20B0 8FB3005C */  lw    $s3, 0x5c($sp)
/* 0E6BE4 7F0B20B4 8FB40060 */  lw    $s4, 0x60($sp)
/* 0E6BE8 7F0B20B8 8FB50064 */  lw    $s5, 0x64($sp)
/* 0E6BEC 7F0B20BC 8FB60068 */  lw    $s6, 0x68($sp)
/* 0E6BF0 7F0B20C0 8FB7006C */  lw    $s7, 0x6c($sp)
/* 0E6BF4 7F0B20C4 8FBE0070 */  lw    $fp, 0x70($sp)
/* 0E6BF8 7F0B20C8 03E00008 */  jr    $ra
/* 0E6BFC 7F0B20CC 27BD0150 */   addiu $sp, $sp, 0x150
)
#endif




s32 sub_GAME_7F0B20D0(StandTile **tileStack, f32 target_x, f32 target_z, f32 unknown) {
    return sub_GAME_7F0B1DDC(tileStack, target_x, target_z, unknown, NULL, NULL, NULL, NULL);
}


/**
 * Address: 7F0B2110
 * 
 * Callback for stan locus traversal.
 * 
 * Adds the current tile's room ID to the caller-provided room list if it
 * has not already been recorded. The callback always returns 0 so traversal
 * continues.
 */
s32 stanLocusAddTileRoomIfNew(StandTile *tile, struct StandTileLocusCallbackRecord *rec)
{
    s32 roomCount;
    s32 room;
    StandTile *t;
    struct StandTileLocusCallbackRecord *record;
    s32 i;
    s32 *ptr;
    
    record = rec;
    i = 0;
    roomCount = record->count;
    t = tile;
    
    // Only search for duplicates if more than 0 rooms have been collected.
    if (roomCount > 0)
    {
        room = t->room;
        ptr = record->rooms;
        
        //If the tile's room is already in the room list, return immediately.
        do
        {
            if (room == (*ptr))
            {
                return 0;
            }
            
            i++;
            ptr++;
        }
        while (i < record->count);
    }
    
    // The room has not been collected yet so append it.
    if (roomCount < rec->bufMax)
    {
        rec->rooms[roomCount] = tile->room;
        rec->count = rec->count + 1;
    }
    
    return 0;
}


s32 incrNearEdgeCount(StandTile **tileStack, s32 stackHeight, struct StandTileLocusCallbackRecord* data) {
    data->nearEdgeCount += 1;
    return 1;
}


/**
 * Address: 7F0B21B0
 */
s32 sub_GAME_7F0B21B0(StandTile **tileStack, f32 target_x, f32 target_z, f32 radius, s32 *rooms, s32 *count_rtn, s32 bufMax)
{
    struct StandTileLocusCallbackRecord data;
    s32 rtn;

    data.rooms = rooms;
    data.count = 0;
    data.bufMax = bufMax;
    data.nearEdgeCount = 0;

    rtn = sub_GAME_7F0B1DDC(tileStack, target_x, target_z, radius,
        stanLocusAddTileRoomIfNew, NULL, incrNearEdgeCount, &data
    );

    *count_rtn = data.count;

    if (1 < data.nearEdgeCount) {
        return 2;
    }

    return rtn;
}


/**
 * Address 0x7F0B2244.
*/
s32 stanIsSpecialBit1Set(StandTile *arg0, struct StandTileLocusCallbackRecord *arg1)
{
    s32 val = arg0->mid.half >> 0xC;
    if (g_StanTileSpecialFlags[val] & STANTILEFLAG_FORCECROUCH)
    {
        arg1->rooms = 1;
    }

    return 0;
}


/**
 * Address: 7F0B2274
 */
s32 stanCheckLinkedSpecialTile(StandTile *tile, s32 pointIdx, s32 arg2, s32 arg3, s32 arg4, s32 *outFlags)
{
    u16 link;
    StandTile *target;
    s32 mid;

    link = tile->points[pointIdx].link;

    if ((link >> 4) != 0) {
        target = (StandTile *)(link + (StandTile *)standTileStart);

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_FORCECROUCH) {
            outFlags[0] = 1;
            return 1;
        }

        mid = target->mid.half;

        if (g_StanTileSpecialFlags[mid >> 0xc] & STANTILEFLAG_LADDER) {
            dword_CODE_bss_8007BA0C = target;
            outFlags[1] = 1;
            return 0;
        }
    }

    return 0;
}


/**
 * Address 0x7F0B2314.
*/
s32 stanTileDistanceRelated(StandTile **arg0, f32 arg1, f32 arg2, f32 arg3, struct StandTileLocusCallbackRecord *arg4)
{
    s32 i;

    // HACK:
    for(i=0;;)
    {
        ((s32*)arg4)[i+0] = 0;
        ((s32*)arg4)[i+1] = 0;
        ((s32*)arg4)[i+2] = 0;
        ((s32*)arg4)[i+3] = 0;
        i+=4;
        if (i>15) break;
    }

    // maybe something like:
    /*
    for(i=0;i<3;i++)
    {
        arg4[i].unk00 = 0;
        arg4[i].count = 0;
        arg4[i].bufMax = 0;
        arg4[i].nearEdgeCount = 0;
    }
    */

    return sub_GAME_7F0B1DDC(arg0, arg1, arg2, arg3, stanIsSpecialBit1Set, stanCheckLinkedSpecialTile, NULL, arg4);
}


s32 stanGetLocusField0(struct StandTileLocusCallbackRecord *arg0)
{
    return arg0->rooms;
}


s32 stanGetLocusCount(struct StandTileLocusCallbackRecord *arg0)
{
    return arg0->count;
}


/**
 * Address: 7F0B23AC
 */
void stanGetTileOrderedPointWorldPos(StandTile *tile, s32 pointnum, coord3d *out)
{
    StandTilePoint *point;
    f32 scale;

    pointnum = tile->tail.half >> (8 - (pointnum * 4));

    point = &tile->points[pointnum & 0xf];

    scale = inv_level_scale;

    out->x = point->x * scale;
    out->y = point->y * scale;
    out->z = point->z * scale;
}


void stanGetMoveBondCollisionTiles(StandTile **tile1, StandTile **tile2, coord3d *coords)
{
    StandTile *curtileStore;
    StandTile *baseTile;
    StandTile *linktile;
    s32 curtilePointI;
    s32 i;
    s32 j;
    s32 k;
    s32 target;

    baseTile = dword_CODE_bss_8007BA0C;

    target = baseTile->tail.hdrTail.pointCount & 0xf;

    i = 0;

    if (i < target)
    {
        do
        {
            linktile = (StandTile *)((u8 *)standTileStart + (baseTile->points[i].link << 3));

            if ((baseTile->points[i].link >> 4) != 0)
            {
                s32 linkTileMid;
            
                linkTileMid = linktile->mid.half;
            
                if (g_StanTileSpecialFlags[linkTileMid >> 12] & STANTILEFLAG_LADDER)
                {
#ifdef DEBUG
                    assert(getsides(linktile) == 3);
#endif
                    curtilePointI = (i + 2) % 3;
            
                    *tile1 = baseTile;
                    *tile2 = linktile;
            
                    j = 0;
                    curtileStore = baseTile;

                    while (1)
                    {
                        for (k = 0; k < 3; k++)
                        {
                            stanGetTileOrderedPointWorldPos(
                                linktile,
                                ((j >> 2) + k) % 3,
                                (coord3d *)((s32)coords + (((j + k) & 3) * 0xc)));
                        }

                        stanGetTileOrderedPointWorldPos(
                            curtileStore,
                            curtilePointI,
                            (coord3d *)((s32)coords + (((j + 3) & 3) * 0xc)));

                        j++;

                        if (j == 12)
                        {
#ifdef DEBUG
                            osSyncPrintf("rotate==12\n");
#endif
                            break;
                        }

                        if (!(coords[2].y < coords[0].y)
                                && !(coords[2].y < coords[1].y)
                                && !(coords[3].y < coords[0].y)
                                && !(coords[3].y < coords[1].y))
                        {
                            break;
                        }
                    }

                    return;
                }
            }

            i++;

            if (i < target)
            {
                continue;
            }

            break;
        }
        while (TRUE);
    }
#ifdef DEBUG
    osSyncPrintf("Ladder %s has no neighbouring ladder stan\n", GetStanRoomID(baseTile));
#endif
}


/**
 * Address: 7F0B260C
 * 
 * Callback function.
 * 
 * For a given edge, return true if the edge is vertically above yThreshold.
 */
bool stanLocusEdgeIsAboveY(StandTile *tile, s32 edgeIndex, f32 edgeDist, f32 distToPointA, f32 distToPointB, f32 *yThreshold)
{
    s32 nextIndex;
    s32 pointCount;
    f32 *threshold;
    s32 pointCountReload;

    threshold = yThreshold;

    if (*yThreshold < (f32)tile->points[edgeIndex].y)
    {
        /**  
         * The duplicated point count calculation is required for matching.
         * This is really just nextIndex = (edgeIndex + 1) % pointCount;
         */
        pointCount = (tile->tail.half >> 12) & 0xf;
        pointCountReload = (tile->tail.half >> 12) & 0xf;

        nextIndex = (edgeIndex + 1) % pointCount;

        pointCount = pointCountReload;

        if (*threshold < (f32)tile->points[nextIndex].y)
        {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 * US address 7F0B26B8.
*/
s32 stanTestLocusEdgeAboveY(StandTile **tile, f32 target_x, f32 target_z, f32 radius, f32 yThreshold)
{
    f32 data;

    data = yThreshold * level_scale;

    /// TODO: Why is this cast wrong?

    return sub_GAME_7F0B1DDC(tile, target_x, target_z, radius, NULL, stanLocusEdgeIsAboveY, NULL, (struct StandTileLocusCallbackRecord*)&data);
}


#ifdef NONMATCHING
// Horrifc BFS on tiles
//
// Four things look like they could be improved
// 1. The outer loop always restarts to zero
//    The next 'wave' will process tiles it already scanned in the second wave
//    Neighbors are checked again needlessly
//    This has a high cost if the waypoint the game is trying to find
//    is far away on the stans
//    Should reset to the previous value of countForIter
// 2. 'seenCount' can become really big because it's all the stans discovered so far
//    Neighbor checks thus become exponentially expensive
//    There could be a faster way to check that a stan was already visited
// 3. The closest pad to a tile is something that could be precomputed
//    because it's static and never changes at run-time
// 4. This function is called at one single location and the second arg
//    is always the same. It could return the result from tilePred already.
//
// stanFillSearch()
StandTile *sub_GAME_7F0B2718(StandTile *srcTile, tilePredicate_t tilePred)
{
    StandTile *tile;
    u32 i;
    u32 pointCount;
    s32 link;
    StandTile *linkTile;

    s32 countForIter;

    s32 pntI;
    s32 seenCount;
    s32 linkI;
    s32 linkOffset;
    s32 madeProgress;   // bool

    // 0x44(sp) looks to be the ptr onto the stack

    // madeProgress =
    if (tilePred(srcTile)) {
        return srcTile;
    }

    bfsTileStack[0] = srcTile;
    seenCount = 1;
    countForIter = seenCount;

    do {
        // Loop over everyone for this iteration
        for(pntI=0; pntI < countForIter; pntI++)
        {

            tile = bfsTileStack[pntI];
            pointCount = STAN_POINT_COUNT(tile);

            // Loop over linked tiles
            for (linkI = 0; linkI < pointCount; linkI++)
            {
                link = tile->points[linkI].link;
                linkOffset = link * 8;  // somehow it's doing this << 3 before testing the >> 4, but this isn't working.

                if (link >> 4 != 0) {
                    linkTile = (struct StandTile*)((s32)standTileStart + linkOffset);

                    // Loop to see if this is a new tile (disgusting)
                    // If was a < and the while was a != though
                    for (i = 0; i < seenCount; i++)
                    {
                        if (bfsTileStack[i] == linkTile) {
                            goto LAB_7f0b283c;  // continue named loop?
                        }
                    }

                    if (tilePred(linkTile)) {
                        return linkTile;
                    }

                    bfsTileStack[seenCount] = linkTile;
                    if (350 < seenCount + 1)
                    {    // as written
                        #ifdef DEBUG
                        osSyncPrintf("Out of confs[] in stanFillSearch()\n");
                        #endif
                        return 0;
                    }

                    pointCount = STAN_POINT_COUNT(tile);
                    seenCount = seenCount + 1;

                }

                // they may also have needed something to put by the label
                LAB_7f0b283c: linkI = linkI;
            }

        }

        // We only continue if we made progress with this iteration
        if (seenCount == countForIter){
            break;
        }

        // does appear to be down here - two li,X,1s initially and a mov zero in the delay slot
        countForIter = seenCount;

    } while (1);


    return 0;
}
#else
GLOBAL_ASM(
.text
glabel sub_GAME_7F0B2718
/* 0E7248 7F0B2718 27BDFFA0 */  addiu $sp, $sp, -0x60
/* 0E724C 7F0B271C AFBF003C */  sw    $ra, 0x3c($sp)
/* 0E7250 7F0B2720 AFB5002C */  sw    $s5, 0x2c($sp)
/* 0E7254 7F0B2724 AFB40028 */  sw    $s4, 0x28($sp)
/* 0E7258 7F0B2728 0080A025 */  move  $s4, $a0
/* 0E725C 7F0B272C 00A0A825 */  move  $s5, $a1
/* 0E7260 7F0B2730 AFBE0038 */  sw    $fp, 0x38($sp)
/* 0E7264 7F0B2734 AFB70034 */  sw    $s7, 0x34($sp)
/* 0E7268 7F0B2738 AFB60030 */  sw    $s6, 0x30($sp)
/* 0E726C 7F0B273C AFB30024 */  sw    $s3, 0x24($sp)
/* 0E7270 7F0B2740 AFB20020 */  sw    $s2, 0x20($sp)
/* 0E7274 7F0B2744 AFB1001C */  sw    $s1, 0x1c($sp)
/* 0E7278 7F0B2748 00A0F809 */  jalr  $a1
/* 0E727C 7F0B274C AFB00018 */  sw    $s0, 0x18($sp)
/* 0E7280 7F0B2750 10400003 */  beqz  $v0, .L7F0B2760
/* 0E7284 7F0B2754 3C018008 */   lui   $at, %hi(bfsTileStack)
/* 0E7288 7F0B2758 10000049 */  b     .L7F0B2880
/* 0E728C 7F0B275C 02801025 */   move  $v0, $s4
.L7F0B2760:
/* 0E7290 7F0B2760 240E0001 */  li    $t6, 1
/* 0E7294 7F0B2764 3C178008 */  lui   $s7, %hi(bfsTileStack)
/* 0E7298 7F0B2768 3C168004 */  lui   $s6, %hi(standTileStart)
/* 0E729C 7F0B276C AC34BA10 */  sw    $s4, %lo(bfsTileStack)($at)
/* 0E72A0 7F0B2770 24110001 */  li    $s1, 1
/* 0E72A4 7F0B2774 26D60F58 */  addiu $s6, %lo(standTileStart) # addiu $s6, $s6, 0xf58
/* 0E72A8 7F0B2778 26F7BA10 */  addiu $s7, %lo(bfsTileStack) # addiu $s7, $s7, -0x45f0
/* 0E72AC 7F0B277C AFAE0058 */  sw    $t6, 0x58($sp)
/* 0E72B0 7F0B2780 0000F025 */  move  $fp, $zero
.L7F0B2784:
/* 0E72B4 7F0B2784 1A200037 */  blez  $s1, .L7F0B2864
/* 0E72B8 7F0B2788 3C0F8008 */   lui   $t7, %hi(bfsTileStack)
/* 0E72BC 7F0B278C 25EFBA10 */  addiu $t7, %lo(bfsTileStack) # addiu $t7, $t7, -0x45f0
/* 0E72C0 7F0B2790 AFAF0044 */  sw    $t7, 0x44($sp)
.L7F0B2794:
/* 0E72C4 7F0B2794 8FB80044 */  lw    $t8, 0x44($sp)
/* 0E72C8 7F0B2798 00009025 */  move  $s2, $zero
/* 0E72CC 7F0B279C 8F140000 */  lw    $s4, ($t8)
/* 0E72D0 7F0B27A0 86840006 */  lh    $a0, 6($s4)
/* 0E72D4 7F0B27A4 02809825 */  move  $s3, $s4
/* 0E72D8 7F0B27A8 0004CB03 */  sra   $t9, $a0, 0xc
/* 0E72DC 7F0B27AC 3324000F */  andi  $a0, $t9, 0xf
/* 0E72E0 7F0B27B0 58800026 */  blezl $a0, .L7F0B284C
/* 0E72E4 7F0B27B4 8FB90044 */   lw    $t9, 0x44($sp)
.L7F0B27B8:
/* 0E72E8 7F0B27B8 9662000E */  lhu   $v0, 0xe($s3)
/* 0E72EC 7F0B27BC 8ECA0000 */  lw    $t2, ($s6)
/* 0E72F0 7F0B27C0 000248C0 */  sll   $t1, $v0, 3
/* 0E72F4 7F0B27C4 00025903 */  sra   $t3, $v0, 4
/* 0E72F8 7F0B27C8 1160001B */  beqz  $t3, .L7F0B2838
/* 0E72FC 7F0B27CC 012A8021 */   addu  $s0, $t1, $t2
/* 0E7300 7F0B27D0 1A200009 */  blez  $s1, .L7F0B27F8
/* 0E7304 7F0B27D4 00001025 */   move  $v0, $zero
/* 0E7308 7F0B27D8 3C038008 */  lui   $v1, %hi(bfsTileStack)
/* 0E730C 7F0B27DC 2463BA10 */  addiu $v1, %lo(bfsTileStack) # addiu $v1, $v1, -0x45f0
.L7F0B27E0:
/* 0E7310 7F0B27E0 8C6C0000 */  lw    $t4, ($v1)
/* 0E7314 7F0B27E4 24420001 */  addiu $v0, $v0, 1
/* 0E7318 7F0B27E8 520C0014 */  beql  $s0, $t4, .L7F0B283C
/* 0E731C 7F0B27EC 26520001 */   addiu $s2, $s2, 1
/* 0E7320 7F0B27F0 1451FFFB */  bne   $v0, $s1, .L7F0B27E0
/* 0E7324 7F0B27F4 24630004 */   addiu $v1, $v1, 4
.L7F0B27F8:
/* 0E7328 7F0B27F8 02A0F809 */  jalr  $s5
/* 0E732C 7F0B27FC 02002025 */  move  $a0, $s0
/* 0E7330 7F0B2800 10400003 */  beqz  $v0, .L7F0B2810
/* 0E7334 7F0B2804 00116880 */   sll   $t5, $s1, 2
/* 0E7338 7F0B2808 1000001D */  b     .L7F0B2880
/* 0E733C 7F0B280C 02001025 */   move  $v0, $s0
.L7F0B2810:
/* 0E7340 7F0B2810 26310001 */  addiu $s1, $s1, 1
/* 0E7344 7F0B2814 02ED7021 */  addu  $t6, $s7, $t5
/* 0E7348 7F0B2818 2E21015F */  sltiu $at, $s1, 0x15f
/* 0E734C 7F0B281C 14200003 */  bnez  $at, .L7F0B282C
/* 0E7350 7F0B2820 ADD00000 */   sw    $s0, ($t6)
/* 0E7354 7F0B2824 10000016 */  b     .L7F0B2880
/* 0E7358 7F0B2828 00001025 */   move  $v0, $zero
.L7F0B282C:
/* 0E735C 7F0B282C 86840006 */  lh    $a0, 6($s4)
/* 0E7360 7F0B2830 00047B03 */  sra   $t7, $a0, 0xc
/* 0E7364 7F0B2834 31E4000F */  andi  $a0, $t7, 0xf
.L7F0B2838:
/* 0E7368 7F0B2838 26520001 */  addiu $s2, $s2, 1
.L7F0B283C:
/* 0E736C 7F0B283C 0244082A */  slt   $at, $s2, $a0
/* 0E7370 7F0B2840 1420FFDD */  bnez  $at, .L7F0B27B8
/* 0E7374 7F0B2844 26730008 */   addiu $s3, $s3, 8
/* 0E7378 7F0B2848 8FB90044 */  lw    $t9, 0x44($sp)
.L7F0B284C:
/* 0E737C 7F0B284C 8FA90058 */  lw    $t1, 0x58($sp)
/* 0E7380 7F0B2850 27DE0001 */  addiu $fp, $fp, 1
/* 0E7384 7F0B2854 27280004 */  addiu $t0, $t9, 4
/* 0E7388 7F0B2858 03C9082A */  slt   $at, $fp, $t1
/* 0E738C 7F0B285C 1420FFCD */  bnez  $at, .L7F0B2794
/* 0E7390 7F0B2860 AFA80044 */   sw    $t0, 0x44($sp)
.L7F0B2864:
/* 0E7394 7F0B2864 8FAA0058 */  lw    $t2, 0x58($sp)
/* 0E7398 7F0B2868 0000F025 */  move  $fp, $zero
/* 0E739C 7F0B286C 522A0004 */  beql  $s1, $t2, .L7F0B2880
/* 0E73A0 7F0B2870 00001025 */   move  $v0, $zero
/* 0E73A4 7F0B2874 1000FFC3 */  b     .L7F0B2784
/* 0E73A8 7F0B2878 AFB10058 */   sw    $s1, 0x58($sp)
/* 0E73AC 7F0B287C 00001025 */  move  $v0, $zero
.L7F0B2880:
/* 0E73B0 7F0B2880 8FBF003C */  lw    $ra, 0x3c($sp)
/* 0E73B4 7F0B2884 8FB00018 */  lw    $s0, 0x18($sp)
/* 0E73B8 7F0B2888 8FB1001C */  lw    $s1, 0x1c($sp)
/* 0E73BC 7F0B288C 8FB20020 */  lw    $s2, 0x20($sp)
/* 0E73C0 7F0B2890 8FB30024 */  lw    $s3, 0x24($sp)
/* 0E73C4 7F0B2894 8FB40028 */  lw    $s4, 0x28($sp)
/* 0E73C8 7F0B2898 8FB5002C */  lw    $s5, 0x2c($sp)
/* 0E73CC 7F0B289C 8FB60030 */  lw    $s6, 0x30($sp)
/* 0E73D0 7F0B28A0 8FB70034 */  lw    $s7, 0x34($sp)
/* 0E73D4 7F0B28A4 8FBE0038 */  lw    $fp, 0x38($sp)
/* 0E73D8 7F0B28A8 03E00008 */  jr    $ra
/* 0E73DC 7F0B28AC 27BD0060 */   addiu $sp, $sp, 0x60
)
#endif




/**
 * @param pntA: out parameter, will contain stanSavedColl_pntA (x,z)
 * @param pntB: out parameter, will contain stanSavedColl_pntB (x,z)
 */
bool getCollisionEdge_maybe(coord3d *pntA, coord3d *pntB)
{
    if (stanSavedColl_tile)
    {
        getTileEdgePoints(stanSavedColl_tile, stanSavedColl_pointI, pntA, pntB);

        return TRUE;
    }
    else
    {
        if (D_800413BC)
        {
            pntA->x = stanSavedColl_pntA.f[0];
            pntA->y = 0;
            pntA->z = stanSavedColl_pntA.f[1];

            pntB->x = stanSavedColl_pntB.f[0];
            pntB->y = 0;
            pntB->z = stanSavedColl_pntB.f[1];

            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}



void setLevelScale(f32 ls)
{
    level_scale = ls;
    inv_level_scale = (1.0f / ls);
    #ifdef DEBUG
    if (level_scale != 1.0)
    {
        osSyncPrintf("%5.2fm squared total area\n", /*lots of math*/ 1* inv_level_scale * inv_level_scale * 0.01 * 0.01);
        //...
        osSyncPrintf("%5.2fm squared BB extent\n\n",/*more maths*/ 1 * inv_level_scale * inv_level_scale * 0.01 * 0.01);
    }
    #endif
    return;
}




/**
 * Calculates y value on a tile, according to (x,z) position.
 *
 * Address 0x7F0B2970.
 */
f32 stanGetPositionYValue(StandTile *tile, f32 p_x, f32 p_z)
{
    f32 a[3]; // sp 132, vector a
    f32 b[3]; // sp 120, vector b
    s64 cp[3]; // sp 96, cross product vector (a x b)
    s64 rsum;
    s32 temp_a3;
    s32 temp_t6;
    s32 temp_t7;

    p_x *= level_scale;
    temp_t6 = STAN_TAIL_D(tile);
    temp_t7 = STAN_TAIL_C(tile);
    temp_a3 = STAN_TAIL_POINT_COUNT(tile);
    p_z *= level_scale;

    a[0] = (f32) (tile->points[temp_t7].x - tile->points[temp_t6].x);
    a[1] = (f32) (tile->points[temp_t7].y - tile->points[temp_t6].y);
    a[2] = (f32) (tile->points[temp_t7].z - tile->points[temp_t6].z);

    b[0] = (f32) (tile->points[temp_a3].x - tile->points[temp_t6].x);
    b[1] = (f32) (tile->points[temp_a3].y - tile->points[temp_t6].y);
    b[2] = (f32) (tile->points[temp_a3].z - tile->points[temp_t6].z);

    // implicit call to __f_to_ll
    // This is the cross product, a x b
    cp[0] = (s64)((a[1] * b[2]) - (a[2] * b[1]));
    cp[1] = (s64)((a[2] * b[0]) - (a[0] * b[2]));
    cp[2] = (s64)((a[0] * b[1]) - (a[1] * b[0]));

    // implicit call to __ll_mul
    rsum = ((s64)cp[0] * (s64)tile->points[temp_t6].x)
        + ((s64)cp[1] * (s64)tile->points[temp_t6].y)
        + ((s64)cp[2] * (s64)tile->points[temp_t6].z);

    // don't divide by zero
    if (cp[1] == 0)
    {
        return (f32) tile->points[temp_t6].y * inv_level_scale;
    }

    return (f32) ((((f64)(rsum) - ((f64) p_x * (f64)cp[0])) - ((f64) p_z * (f64)cp[2])) / (f64)(cp[1])) * inv_level_scale;
}


void copy_tile_RGB_as_24bit(StandTile *tile, f32 p_x, f32 p_z, u8 *rtn)
{
    u8 B = (tile->mid.half >> 0x8) & 0xF;
    u8 C = (tile->mid.half >> 0x4) & 0xF;
    u8 D = (tile->mid.half >> 0x0) & 0xF;
    rtn[0] = (B << 0x4) | B;
    rtn[1] = (C << 0x4) | C;
    rtn[2] = (D << 0x4) | D;
}


/**
 * Address: 7F0B2C74
 */
void stanGetTileHeaderCYBounds(StandTile *tile, f32 *out)
{
    f32 y0;
    f32 y1;
    f32 y2;
    f32 min;
    f32 max;

     /*
     * This seems like a bug.
     * The function is structured like it wants the min/max Y of the
     * three packed indices headerC/headerD/headerE, but
     * all three reads use headerC: (tail >> 8) & 0xf.
     * 
     * Ultimately the function call chain leads nowhere so this is
     * dead code and the bug doesn't matter.
     */
    y0 = (f32)tile->points[(tile->tail.half >> 8) & 0xf].y;
    y1 = (f32)tile->points[(tile->tail.half >> 8) & 0xf].y;
    y2 = (f32)tile->points[(tile->tail.half >> 8) & 0xf].y;

    min = y1;

    if (y0 < y1)
    {
        min = y0;
    }

    if (y2 < min)
    {
        min = y2;
    }

    max = y0;

    if (y0 < y1)
    {
        max = y1;
    }

    if (max < y2)
    {
        max = y2;
    }

    out[0] = min * inv_level_scale;
    out[1] = max * inv_level_scale;
}


/**
 * Address: 7F0B2D14
 */
f32 stanGetTileHeaderCMinY(StandTile *tile) {
    f32 vs[2];

    stanGetTileHeaderCYBounds(tile, vs);
    return vs[0];
}


void debugStanView(s8 joyX, s8 joyY, u16 joyBtns) {
    return;
}


/**
 * Address: 7F0B2D48
 */
Gfx * stanRenderDebugStanView(Gfx *arg0) {
    return arg0;
}


 /**
 * Get 24bit id stanIdHi from id string
 * @param stanIdHi: 1bit Type, 15bit Integer ID.
 * @param stanIdLo: 5bit stanIdLo File (a-z) and 3bit subtri 0-7
 * canonically Named
 */
void stanPackId(char *id, u16 *stanIdHi, u8 *stanIdLo)
{
    u32   bitsnumber; // sp3c
    char *str_end;    // sp38
    s32   y;          // sp34
    s32   bitsletter; // sp30
    s32   bitsfile;   // sp2c
    s32   bitssubtri; // sp28
    s32   var1;       // sp24

    var1 = id[0] - 'p';

    if (var1 < 0 || var1 > 1) // is p or q the first char (q never used?)
    {
        #ifdef ENABLE_LOG
            osSyncPrintf("stanPackId(): Bad letter chr \'%c\' in \"%s\"\n", id[0], id);
        #endif
    }
    else
    {
        bitsletter = id[0] - 'p'; // yes this is right, Im duplicated.
        bitsnumber = strtol(id + 1, &str_end, 10);

        if (id == str_end - 1)
        {
            #ifdef ENABLE_LOG
                osSyncPrintf("stanPackId(): Bad integer in \"%s\"\n", id);
            #endif
        }
        else if (bitsnumber <= 32767)
        {
            if (str_end[0] - 'a' < 0 || str_end[0] - 'a' >= 26)
            {
                #ifdef ENABLE_LOG
                    osSyncPrintf("stanPackId(): Bad file chr \'%c\' in \"%s\"\n", str_end[0], id);
                #endif
            }
            else
            {
                bitsfile = str_end[0] - 'a';
                if (1)
                    ;
                bitssubtri = str_end[1];

                if (bitssubtri != 0 && bitssubtri != '0')
                {
                    bitssubtri -= '0';
                }

                if (bitssubtri < 0 || bitssubtri >= 8)
                {
                    #ifdef ENABLE_LOG
                        osSyncPrintf("stanPackId(): Bad subtri chr \'%c\' in \"%s\"\n", str_end[1], id);
                    #endif
                }
                else
                {
                    if (str_end[1] == 0 || str_end[2] == 0)
                    {
                        #ifdef DEBUG
                        assert(bitsletter>=0&&bitsletter<=1);     // # 1094 "stan.c"
                        assert(bitsnumber>=0&&bitsnumber<=32767); // # 1095 "stan.c"
                        assert(bitsfile >=0&&bitsfile <=31);      // # 1096 "stan.c"
                        assert(bitssubtri>=0&&bitssubtri<=7);     // # 1097 "stan.c"
                        #endif
                        *stanIdHi = bitsletter << 0xf | bitsnumber;
                        *stanIdLo = bitsfile << 3 | bitssubtri;
                        return;
                    }
                    else
                    {
                        #ifdef ENABLE_LOG
                            osSyncPrintf("stanPackId(): Stan id too long \"%s\"\n", id);
                        #endif
                    }
                }
            }
        }
        else
        {
            #ifdef ENABLE_LOG
                osSyncPrintf("stanPackId(): Integer %d out of range in \"%s\"\n", bitsnumber, id);
            #endif
        }
    }
    *stanIdHi = -1;
    *stanIdLo = -1;
    return;
}


struct StandTilePoint *stanMatchTileName(char *id)
{
    StandTilePoint *tile;
    u16 stanIdHi;
    u8 stanIdLo;
    s16 tmp;

    if (*id == '\0') {
        return NULL;
    }

    stanPackId(id, &stanIdHi, &stanIdLo);

    tile = stan_prefix->ptr_firstroom;

    while (*(u32 *)tile != 0) {
        if ((u16)tile->x == stanIdHi) {
            if (*((u8 *)&tile->y) == stanIdLo) {
                return tile;
            }
        }

        tmp = tile->link;
        tile = (StandTilePoint *)((u8 *)tile +
            list_of_tilesizes[(tmp >> 12) & 0xf]);
    }

    return NULL;
}


#ifdef XBLADEBUG
StandTile RemovedDebugFunctionOrXBLAUnique_7F0B2EFC()
{
    lVar1 = param_2;
    local_10 = *(stanPrefix + 4);
    cStack00000017 = param_1;
    sStack0000001e = param_2;
    while( true ) {
    if (*local_10 == 0) {
        return NULL;
    }
    cVar2 = sStack0000001e;
    if ((*local_10 == sStack0000001e) && (cVar2 = cStack00000017, *(local_10 + 2) == cStack00000017)
        ) break;
    local_10 = Function_8238ED08(local_10,lVar1,in_r5,in_r6,in_r7,in_r8,in_r9,cVar2,
                                    in_stack_ffffffab,in_stack_ffffffaf,in_stack_ffffffb4);
    }
    return local_10;
}
#endif


void sub_GAME_7F0B2F00(StandTilePoint** arg0) {
    *arg0 = stanMatchTileName(*arg0);
}


void stanDetermineEOF(struct StanPrefixRecord *file /* canonically r */, s32 origBase, u8 *newBase)
{
    s32 delta;
    void **roomPtr;
    StandTile *tile;
    u8 *tileSizes;
    
    delta = ((s32) newBase) - origBase;
    stan_prefix = file;
    
    #ifdef DEBUG
    assert(*r==0);
    #endif
  
    standTileStart = (StandTile *)(((s32)file->ptr_firstroom + delta) - 0x80);
    ptr_firstroom_0 = (s32)file->ptr_firstroom + delta;
    
    newBase = list_of_tilesizes;
    roomPtr = (void **)&file->ptr_firstroom;
    
    if (file->ptr_firstroom != NULL)
    {
        do
        {
            *roomPtr = (void *) ((s32) (*roomPtr) + delta);
            roomPtr++;
        }
        while (*roomPtr != NULL);
    }
    
    tile = (StandTile *) (roomPtr + ((0, 1)));
    
    if ((*(s32 *) tile) != 0)
    {
        do
        {
            stanTileEnd = tile;

            // Fake but required for matching.
            if (tile->tail.half);
            
            tile = (StandTile *)((s32)tile
                + (tileSizes = newBase)[(tile->tail.half >> 0xc) & 0xf]);
        } 
        while (*(s32 *) tile != 0);
    }
    
    stan_prefix = file;
}


/**
 Get the room the tile belongs to
 @param tile: Tile to quiry
 @return the room number the tile is located in
 @exception Although room is u8, this needs to be s32 for matching ai.
 */
s32 getTileRoom(StandTile *tile)
{
    return tile->room;
}


//incorrect here so that both this and sub_GAME_7F0B4F9C match
extern s32 sub_GAME_7F0B4F9C(u8 arg0) ;

s32 sub_GAME_7F0B2FE0(StandTile *tile)
{
    // u8 -> s32 -> u8 causes the odd asm

    s32 room = tile->room;

    return sub_GAME_7F0B4F9C(room);
}

/**
 * Address: 7F0B3004
 * 
 * Unused.
 */
f32 stanGetTileHeaderCMinYWrapper(StandTile *tile) {
    return stanGetTileHeaderCMinY(tile);
}

Gfx * sub_GAME_7F0B3024(Gfx *ptrdl, StandTilePoint *tile_point, u32 RGBAColor) {
    return ptrdl;
}

Gfx * sub_GAME_7F0B3034(Gfx *arg0) {
    return arg0;
}

Gfx * sub_GAME_7F0B303C(Gfx * arg0) {
    return arg0;
}

s32 sub_GAME_7F0B3044(void) {
    s32 sp1C;
    f32 temp_f0;

    sp1C = 0;
    if (((dynGetFreeGfx() < 0x1000) || (dynGetFreeVtx() < 0x1000)) && (*D_800413D0 == 0)) {
        D_800413C0 = 0.0f;
        D_800413C4 = 0.0f;
        D_800413C8 = D_800413CC;
        *D_800413D0 = 1;
    }
    if (*D_800413D0 == 0) {
        D_800413C0 += D_800413C4;
        temp_f0 = D_800413C0;
        if (temp_f0 > 1.0f) {
            sp1C = 1;
            D_800413C0 = temp_f0 - 1.0f;
        }
    }
    D_800413CC += 1;
    return sp1C;
}

Gfx * sub_GAME_7F0B312C(Gfx *arg0, s32 arg1)
{
    #ifdef DEBUG
      qword *pqVar1;
      qword *pqVar2;
      int iVar3;
      char cVar4;
      ulonglong uVar5;
      ulonglong uVar6;
      uint uStack00000014;
      stanRecord *psStack0000001c;
      char cStack00000027;
      ushort **ppuStack0000002c;
      uint uStack00000034;
      dword local_80;
      char *local_30;
      dword local_2c;
      uint local_28;
      dword local_24;
      dword local_20;
      qword local_10;
      dword local_8;

      uStack00000034 = param_5;
      ppuStack0000002c = param_4;
      uVar6 = ZEXT48(param_2);
      *ppuStack0000002c = param_2;
      local_30 = &DAT_00000001;
      uStack00000014 = param_1;
      psStack0000001c = param_2;
      cStack00000027 = param_3;
      pqVar1 = Function_8237F158();
      pqVar2 = Function_8237F058();
      uVar5 = 0;
      Function_82278968(pqVar2,pqVar1,0,param_4,param_5,param_6,uVar6);
      do {
        do {
          if (local_30 == NULL) {
            Function_82279088();
            return;
          }
          local_30 = local_30 + -1;
          psStack0000001c = ppuStack0000002c[local_30];
        } while (*psStack0000001c >> 0xf == cStack00000027);
        *psStack0000001c = *psStack0000001c ^ 0x8000;
        stanTileHasZeroArea(psStack0000001c);
        iVar3 = sub_GAME_7F0B3044();
        if (iVar3 != 0) {
          Function_8238AC90(psStack0000001c,uStack00000014,uVar5,param_4,param_5,param_6,uVar6);
        }
        for (local_28 = 0; cVar4 = getsides(psStack0000001c), local_28 < cVar4; local_28 = local_28 + 1)
        {
          if (psStack0000001c[local_28 + 1].tail >> 4 == 0) {
            iVar3 = sub_GAME_7F0B3044();
            if (iVar3 != 0) {
              uVar5 = local_28 + 1;
              cVar4 = getsides(psStack0000001c);
              trapWord(6,cVar4,0);
              trapWord(5,cVar4 & ~(((uVar5 & 0x7fffffff) << 1 | (uVar5 << 0x20) >> 0x3f) - 1),0xffff);
              uVar5 = uVar5 - (uVar5 / cVar4) * cVar4;
              param_4 = 0xffffffffffffffc0;
              Function_8238B8F0(psStack0000001c,local_28,uVar5,0xffffffffffffffc0,param_5,param_6,uVar6)
              ;
            }
          }
          else if (*(stanTileStart + psStack0000001c[local_28 + 1].tail) >> 0xf != cStack00000027) {
            uVar6 = ZEXT48(ppuStack0000002c);
            ppuStack0000002c[local_30] = stanTileStart + psStack0000001c[local_28 + 1].tail;
            local_30 = local_30 + 1;
            if (uStack00000034 < local_30) {
              osSyncPrintf("stanFillinVis: Stack overflow %d>%d",local_30,uStack00000034);
              return;
            }
          }
        }
      } while( TRUE );
    #endif
    return arg0;
}


/**
 * Unreferenced.
 */
s32 sub_GAME_7F0B3138(StandTile *tile, StandTile **pTile, f32 p_x, f32 p_z, f32 dest_x, f32 dest_z, s32 cdtypes, f32 unkHeight, f32 unkA)
{
    // Fake but needed for matching.
    if (pTile);

    return stanTestLineUnobstructed(pTile, p_x, p_z, dest_x, dest_z, cdtypes, unkHeight, unkA, 0.0f, 1.0f);
}


void sub_GAME_7F0B31A4(s32 arg0, StandTile *arg1, f32 arg2, f32 arg3, f32 arg4, s32 arg5, f32 arg6, f32 arg7) {
    stanTestVolume(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
}



