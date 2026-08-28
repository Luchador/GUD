#include <ultra64.h>
#include <bondgame.h>
#include "ramrom.h"
#include "deb.h"
#include "str.h"
#include "memp.h"
#include "crash.h"
#include <PR/os.h>
#include "fr.h"
#include "tlb_manage.h"
#include <PR/R4300.h>
#include "thread_config.h"

struct deblistentry
{
    struct deblistentry *next;
    void                *data;
    const char          *name;
    s32                  unused;
};

#if defined(LEFTOVERDEBUG)
u8 g_DebBuffer[0x400];
#else
u8 g_DebBuffer[0x10];
#endif

u32 g_DebDebugData[] = {0, 0};
struct deblistentry *g_DebList = NULL;
s32 D_800232EC[] = {0, 0, 0};
u8 *g_DebMemPos = g_DebBuffer;

struct deblistentry *debFind(const char *name)
{
    struct deblistentry *entry = g_DebList;
    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    return NULL;
}

u8 *debAllocate(s32 size) {
#ifdef LEFTOVERDEBUG
    u8 **pos = &g_DebMemPos;
    u8 *curr = *pos;
    u8 *prev = curr;
    curr += size;

    if (curr > (g_DebBuffer + 0x400))
    {
        curr -= size;
        *pos = curr;
        return mempAllocBytesInBank(size, MEMPOOL_PERMANENT);
    }
    else
    {
        *pos = curr;
    }

    return prev;
#else
    return &g_DebBuffer;
#endif
}


void debInit(void)
{
    crashInit();
}
