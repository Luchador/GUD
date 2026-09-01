#include <ultra64.h>
#include <bondgame.h>
#include <memp.h>
#include "language.h"
#include "ob.h"

s32 g_LangBanks[45];

struct jpncharpixels* g_JpnCharCachePixels;
struct  jpncacheitem *g_JpnCacheCacheItems;

void *LnameX_lookuptable[45][2] = {
    {NULL, NULL},                    /* Null (unused) */
    {"LameE", "LameJ"},              /* Library (multi) */
    {"LarchE", "LarchJ"},            /* Archives */
    {"LarkE", "LarkJ"},              /* Facility */
    {"LashE", "LashJ"},              /* Stack (multi) */
    {"LaztE", "LaztJ"},              /* Aztec */
    {"LcatE", "LcatJ"},              /* Citadel (multi) */
    {"LcaveE", "LcaveJ"},            /* Caverns */
    {"LarecE", "LarecJ"},            /* Control */
    {"LcradE", "LcradJ"},            /* Cradle */
    {"LcrypE", "LcrypJ"},            /* Egypt */
    {"LdamE", "LdamJ"},              /* Dam */
    {"LdepoE", "LdepoJ"},            /* Depot */
    {"LdestE", "LdestJ"},            /* Frigate */
    {"LdishE", "LdishJ"},            /* Temple (multi) */
    {"LearE", "LearJ"},              /* Ear (unused) */
    {"LeldE", "LeldJ"},              /* Eld (unused) */
    {"LimpE", "LimpJ"},              /* Basement (multi) */
    {"LjunE", "LjunJ"},              /* Jungle */
    {"LleeE", "LleeJ"},              /* Lee (unused) */
    {"LlenE", "LlenJ"},              /* Cuba */
    {"LlipE", "LlipJ"},              /* Lip (unused) */
    {"LlueE", "LlueJ"},              /* Lue (unused) */
    {"LoatE", "LoatJ"},              /* Cave (multi) */
    {"LpamE", "LpamJ"},              /* Pam (unused) */
    {"LpeteE", "LpeteJ"},            /* Streets */
    {"LrefE", "LrefJ"},              /* Complex (multi) */
    {"LritE", "LritJ"},              /* Rit (unused) */
    {"LrunE", "LrunJ"},              /* Runway */
    {"LsevbE", "LsevbJ"},            /* Bunker 2 */
    {"LsevE", "LsevJ"},              /* Bunker 1 */
    {"LsevxE", "LsevxJ"},            /* Surface 1 */
    {"LsevxbE", "LsevxbJ"},          /* Surface 2 */
    {"LshoE", "LshoJ"},              /* Shooting Range (unused) */
    {"LsiloE", "LsiloJ"},            /* Silo */
    {"LstatE", "LstatJ"},            /* Statue */
    {"LtraE", "LtraJ"},              /* Train */
    {"LwaxE", "LwaxJ"},              /* Wax (unused) */
    {"LgunE", "LgunJ"},              /* Guns */
    {"LtitleE", "LtitleJ"},          /* Stage and menu titles */
    {"LmpmenuE", "LmpmenuJ"},        /* Multi menus */
    {"LpropobjE", "LpropobjJ"},      /* In-game pickups */
    {"LmpweaponsE", "LmpweaponsJ"},  /* Multi weapon select */
    {"LoptionsE", "LoptionsJ"},      /* Solo in-game menus */
    {"LmiscE", "LmiscJ"}};           /* Cheat options */

LEVELID langGetLangBankIndexFromStagenum(LEVELID level)
{
    LEVELID return_id;

    switch(level)
    {
        case LEVELID_DAM:
            return_id = LDAM;
            break;
        case LEVELID_FACILITY:
            return_id = LARK;
            break;
        case LEVELID_RUNWAY:
            return_id = LRUN;
            break;
        case LEVELID_SURFACE:
            return_id = LSEVX;
            break;
        case LEVELID_BUNKER1:
            return_id = LSEV;
            break;
        case LEVELID_SILO:
            return_id = LSILO;
            break;
        case LEVELID_FRIGATE:
            return_id = LDEST;
            break;
        case LEVELID_SURFACE2:
            return_id = LSEVXB;
            break;
        case LEVELID_BUNKER2:
            return_id = LSEVB;
            break;
        case LEVELID_STATUE:
            return_id = LSTAT;
            break;
        case LEVELID_ARCHIVES:
            return_id = LARCH;
            break;
        case LEVELID_STREETS:
            return_id = LPETE;
            break;
        case LEVELID_DEPOT:
            return_id = LDEPO;
            break;
        case LEVELID_TRAIN:
            return_id = LTRA;
            break;
        case LEVELID_JUNGLE:
            return_id = LJUN;
            break;
        case LEVELID_CONTROL:
            return_id = LAREC;
            break;
        case LEVELID_CAVERNS:
            return_id = LCAVE;
            break;
        case LEVELID_CRADLE:
            return_id = LCRAD;
            break;
        case LEVELID_AZTEC:
            return_id = LAZT;
            break;
        case LEVELID_EGYPT:
            return_id = LCRYP;
            break;
        case LEVELID_TEMPLE:
            return_id = LDISH;
            break;
        case LEVELID_COMPLEX:
            return_id = LREF;
            break;
        case LEVELID_LIBRARY:
            return_id = LAME;
            break;
        case LEVELID_BASEMENT:
            return_id = LIMP;
            break;
        case LEVELID_STACK:
            return_id = LASH;
            break;
        case LEVELID_CAVES:
            return_id = LOAT;
            break;
        case LEVELID_CUBA:
            return_id = LLEN;
            break;
        default:
        {
            /* infinite loop on invalid text bank */
            while(1) {};
        }
    }

    return return_id;
}


void langInit(void)
{
    s32 i;

	for (i = 0; i < 45; i++)
    {
		g_LangBanks[i] = 0;
	}

    g_LangBanks[LGUN] = _fileNameLoadToBank(LnameX_lookuptable[LGUN][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LTITLE] = _fileNameLoadToBank(LnameX_lookuptable[LTITLE][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMPMENU] = _fileNameLoadToBank(LnameX_lookuptable[LMPMENU][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LPROPOBJ] = _fileNameLoadToBank(LnameX_lookuptable[LPROPOBJ][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMPWEAPONS] = _fileNameLoadToBank(LnameX_lookuptable[LMPWEAPONS][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LOPTIONS] = _fileNameLoadToBank(LnameX_lookuptable[LOPTIONS][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMISC] = _fileNameLoadToBank(LnameX_lookuptable[LMISC][0], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
}


extern u8 _efontchardataSegmentRomStart;
extern u8 _jfontchardataSegmentRomStart;
void romCopy(void *target, void *source, u32 size);

struct jpncharpixels *langGetJpnCharPixels(s32 codepoint)
{
	s32 i;
	s32 freeindexsingle = -1;
	s32 freeindexmulti = -1;
	s32 multibyte = 0;


	if (codepoint & 0x2000) {
		multibyte = 1;
	}


#define SHIFTAMOUNT 1
#define TMUL 8

	for (i = 0; i < 0x7C; i++) {
		if (!multibyte && (codepoint >> SHIFTAMOUNT) == g_JpnCacheCacheItems[i].codepoint) {
			break;
		}

		if (multibyte && i + 1 < 0x7C
				&& (codepoint >> SHIFTAMOUNT) == g_JpnCacheCacheItems[i].codepoint
				&& (codepoint >> SHIFTAMOUNT) == g_JpnCacheCacheItems[i + 1].codepoint) {
			break;
		}

		if (g_JpnCacheCacheItems[i].ttl == 0) {
			freeindexsingle = i;
		}

		if (g_JpnCacheCacheItems[i].ttl == 0 && g_JpnCacheCacheItems[i + 1].ttl == 0 && i + 1 < 0x7C) {
			freeindexmulti = i;
		}
	}

	if (i < 0x7C) {
		if (!multibyte) {
			g_JpnCacheCacheItems[i].ttl = 2;

			return &g_JpnCharCachePixels[i * TMUL];
		} else {
			g_JpnCacheCacheItems[i + 0].ttl = 2;
			g_JpnCacheCacheItems[i + 1].ttl = 2;

			return &g_JpnCharCachePixels[TMUL * i];
		}
	}


	if (!multibyte && freeindexsingle >= 0) {
		g_JpnCacheCacheItems[freeindexsingle].ttl = 2;
		g_JpnCacheCacheItems[freeindexsingle].codepoint = codepoint >> 1;

		romCopy(&g_JpnCharCachePixels[freeindexsingle * 8], (romptr_t) &_jfontchardataSegmentRomStart + (codepoint >> SHIFTAMOUNT) * 0x60, 0x60);

		return &g_JpnCharCachePixels[freeindexsingle * 8];
	}

	if (multibyte && freeindexmulti >= 0) {
		g_JpnCacheCacheItems[freeindexmulti + 0].ttl = 2;
		g_JpnCacheCacheItems[freeindexmulti + 1].ttl = 2;
		g_JpnCacheCacheItems[freeindexmulti + 0].codepoint = codepoint >> 1;
		g_JpnCacheCacheItems[freeindexmulti + 1].codepoint = codepoint >> 1;

		romCopy(&g_JpnCharCachePixels[freeindexmulti * 8], (romptr_t) &_efontchardataSegmentRomStart + ((codepoint & 0x1fff) >> SHIFTAMOUNT) * 0x80, 0x80);

		return &g_JpnCharCachePixels[freeindexmulti * 8];
	}

	return &g_JpnCharCachePixels[0];
}


void langLoadToAddr(u32 id)
{
    g_LangBanks[id] = _fileNameLoadToBank(LnameX_lookuptable[id][0],1,0x100,MEMPOOL_STAGE);
}


void langLoadToBank(int id,u8 *target,int size)
{
    g_LangBanks[id] = _fileNameLoadToAddr(LnameX_lookuptable[id][0],1,target,size);
}


void langClearBank(s32 textBank)
{
    g_LangBanks[textBank] = 0;
}


/**
 * Get pointer of a string based on language of game (E/J)
 * @param slotID: UniqueID of string (a combination of Bank ID and string index)
 * @return char* string.
 */
u8 * langGet(s32 slotID)
{
    u32 * textbank_ptr = g_LangBanks[slotID >> 10]; /* get the text file bank ID index the text ptr table */
    u32 textslot_offset = textbank_ptr[slotID & 0x03FF]; /* load the textbank ptr table then get the slot's offset */

    u32 output_slot = textslot_offset; /* add the text slot offset to the base ptr to get the ptr to text file's slot */
    output_slot += (u32)textbank_ptr;
    return (textslot_offset != 0) ? (u8*)output_slot : NULL;
}
