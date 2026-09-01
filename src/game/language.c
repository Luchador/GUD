#include <ultra64.h>
#include <bondgame.h>
#include <memp.h>
#include "language.h"
#include "ob.h"

s32 g_LangBanks[45];

char *g_TextBankNames[45] = {
    NULL,            /* Null (unused) */
    "LameE",        /* Library (multi) */
    "LarchE",       /* Archives */
    "LarkE",        /* Facility */
    "LashE",        /* Stack (multi) */
    "LaztE",        /* Aztec */
    "LcatE",        /* Citadel (multi) */
    "LcaveE",       /* Caverns */
    "LarecE",       /* Control */
    "LcradE",       /* Cradle */
    "LcrypE",       /* Egypt */
    "LdamE",        /* Dam */
    "LdepoE",       /* Depot */
    "LdestE",       /* Frigate */
    "LdishE",       /* Temple (multi) */
    "LearE",        /* Ear (unused) */
    "LeldE",        /* Eld (unused) */
    "LimpE",        /* Basement (multi) */
    "LjunE",        /* Jungle */
    "LleeE",        /* Lee (unused) */
    "LlenE",        /* Cuba */
    "LlipE",        /* Lip (unused) */
    "LlueE",        /* Lue (unused) */
    "LoatE",        /* Cave (multi) */
    "LpamE",        /* Pam (unused) */
    "LpeteE",       /* Streets */
    "LrefE",        /* Complex (multi) */
    "LritE",        /* Rit (unused) */
    "LrunE",        /* Runway */
    "LsevbE",       /* Bunker 2 */
    "LsevE",        /* Bunker 1 */
    "LsevxE",       /* Surface 1 */
    "LsevxbE",      /* Surface 2 */
    "LshoE",        /* Shooting Range (unused) */
    "LsiloE",       /* Silo */
    "LstatE",       /* Statue */
    "LtraE",        /* Train */
    "LwaxE",        /* Wax (unused) */
    "LgunE",        /* Guns */
    "LtitleE",      /* Stage and menu titles */
    "LmpmenuE",     /* Multi menus */
    "LpropobjE",    /* In-game pickups */
    "LmpweaponsE",  /* Multi weapon select */
    "LoptionsE",    /* Solo in-game menus */
    "LmiscE"        /* Cheat options */
};

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

    g_LangBanks[LGUN] = _fileNameLoadToBank(g_TextBankNames[LGUN], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LTITLE] = _fileNameLoadToBank(g_TextBankNames[LTITLE], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMPMENU] = _fileNameLoadToBank(g_TextBankNames[LMPMENU], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LPROPOBJ] = _fileNameLoadToBank(g_TextBankNames[LPROPOBJ], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMPWEAPONS] = _fileNameLoadToBank(g_TextBankNames[LMPWEAPONS], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LOPTIONS] = _fileNameLoadToBank(g_TextBankNames[LOPTIONS], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
    g_LangBanks[LMISC] = _fileNameLoadToBank(g_TextBankNames[LMISC], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_PERMANENT);
}


void langLoadToAddr(u32 id)
{
    g_LangBanks[id] = _fileNameLoadToBank(g_TextBankNames[id], FILELOADMETHOD_DEFAULT, 0x100, MEMPOOL_STAGE);
}


void langLoadToBank(int id,u8 *target,int size)
{
    g_LangBanks[id] = _fileNameLoadToAddr(g_TextBankNames[id], FILELOADMETHOD_DEFAULT, target, size);
}


void langClearBank(s32 textBank)
{
    g_LangBanks[textBank] = 0;
}


/**
 * Get a string from a loaded text bank.
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
