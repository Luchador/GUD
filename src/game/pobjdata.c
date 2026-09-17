#include <ultra64.h>
#include <bondgame.h>
#include "chrobjdata.h"
#include "gedmanifest.h"
#include "customprops.h"
#include <macro.h>
#include <memp.h>
#include <assets/obseg/file_resource_id_enums.h>

//D:800375D0
#include <assets/obseg/prop/propModelFileHeaders.inc.c>

//D:8003A050
#include <assets/embedded/skeletons/props.inc.c>



//D:8003A228                     .globl PitemZ_entries
#include <assets/obseg/prop/propItemModelFileRecord.inc.c>
GEDM_TABLE(g_GedPropModels, PitemZ_entries, 1);

//D:8003b224
#include <assets/obseg/prop/propExplosionDetailsRecords.inc.c>

/* The editor patches only this descriptor. Meshes stay in ROM until used;
 * per-stage RAM grows with the number of added models, with no reserved pool. */
CustomPropRomConfig g_CustomPropRomConfig = { CUSTOM_PROP_CONFIG_VERSION, 0, 0, 0 };
typedef struct CustomPropRuntime {
    ModelFileHeader header;
    ItemModelFileRecord model;
    fileentry file;
    resource_lookup_data_entry info;
    char name[64];
} CustomPropRuntime;
static CustomPropRuntime *g_CustomProps;
static s32 g_CustomPropCount;

void customPropsReset(void)
{
    g_CustomProps = NULL;
    g_CustomPropCount = 0;
}

void customPropsInit(void)
{
    /* PI DMA destinations and cache invalidation must be 16-byte aligned. */
    u8 scratch[CUSTOM_PROP_ENTRY_SIZE + 15];
    u32 *row = (u32 *)ALIGN16_a((u32)scratch);
    u32 count, i, offset, size;
    CustomPropRuntime *items;
    customPropsReset();
    if (g_CustomPropRomConfig.version != CUSTOM_PROP_CONFIG_VERSION
        || !g_CustomPropRomConfig.romStart || g_CustomPropRomConfig.romSize < 16) return;
    romCopy(row, (void *)g_CustomPropRomConfig.romStart, CUSTOM_PROP_HEADER_SIZE);
    count = row[1];
    if (row[0] != CUSTOM_PROP_MAGIC || !count || count > CUSTOM_PROP_CAPACITY
        || row[2] != CUSTOM_PROP_ENTRY_SIZE || row[3] != CUSTOM_PROP_BASE
        || count > (g_CustomPropRomConfig.romSize - 16) / CUSTOM_PROP_ENTRY_SIZE) return;
    items = mempAllocBytesInBank(count * sizeof(*items), MEMPOOL_STAGE);
    if (!items) return;
    bzero(items, count * sizeof(*items));
    for (i = 0; i < count; i++)
    {
        CustomPropRuntime *item = &items[i];
        romCopy(row, (void *)(g_CustomPropRomConfig.romStart + 16 + i * CUSTOM_PROP_ENTRY_SIZE), CUSTOM_PROP_ENTRY_SIZE);
        offset = row[16]; size = row[17];
        if (((char *)row)[0] != 'P' || ((char *)row)[63] != 0
            || offset < 16 + count * CUSTOM_PROP_ENTRY_SIZE || (offset & 15)
            || size < 176 || (size & 15) || offset > g_CustomPropRomConfig.romSize
            || size > g_CustomPropRomConfig.romSize - offset) return;
        memcpy(item->name, row, 64);
        item->header.Skeleton = &SKELETON(standard_object);
        item->header.numSwitches = 1;
        item->header.numMatrices = 1;
        memcpy(&item->header.BoundingVolumeRadius, &row[18], 4);
        item->model.header = &item->header;
        item->model.filename = item->name;
        memcpy(&item->model.scale, &row[20], 4);
        item->file.index = OBJ_INDEX_MAX + i;
        item->file.filename = item->name;
        item->file.hw_address = (u8 *)(g_CustomPropRomConfig.romStart + offset);
        item->info.rom_size = size;
    }
    g_CustomProps = items;
    g_CustomPropCount = count;
}

ItemModelFileRecord *propModelGet(s32 modelid)
{
    if (modelid >= 0 && modelid < ARRAYCOUNT(PitemZ_entries) - 1) return &PitemZ_entries[modelid];
    modelid -= CUSTOM_PROP_BASE;
    return modelid >= 0 && modelid < g_CustomPropCount ? &g_CustomProps[modelid].model : NULL;
}

ExplosionDetailsRecord *propExplosionGet(s32 modelid)
{
    static ExplosionDetailsRecord fallback = { EXPLOSION_SMALL, {0, 0, 0, 0, 0, 0} };
    return modelid >= 0 && modelid < ARRAYCOUNT(object_explosion_details)
        ? &object_explosion_details[modelid] : &fallback;
}

s32 customPropFileIndex(const char *name)
{
    s32 i;
    for (i = 0; i < g_CustomPropCount; i++)
        if (!strcmp(name, g_CustomProps[i].name)) return OBJ_INDEX_MAX + i;
    return -1;
}

fileentry *customPropFile(s32 index)
{
    index -= OBJ_INDEX_MAX;
    return index >= 0 && index < g_CustomPropCount ? &g_CustomProps[index].file : NULL;
}

resource_lookup_data_entry *customPropFileInfo(s32 index)
{
    index -= OBJ_INDEX_MAX;
    return index >= 0 && index < g_CustomPropCount ? &g_CustomProps[index].info : NULL;
}
