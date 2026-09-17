/* This runs without libc to retain N64 pointer/record widths on a 64-bit host.
 * Bank words use host byte order: the test targets allocation/DMA boundaries,
 * not the already-covered project/ROM serializer. */
static u8 bankData[16 + CUSTOM_PROP_CAPACITY * (CUSTOM_PROP_ENTRY_SIZE + 176)]
    __attribute__((aligned(16)));
static u8 pool[65536] __attribute__((aligned(16)));
static u32 used, transfers, metadataSize;

static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    __asm__ volatile("int $0x80" : : "a"(1), "b"(status) : "memory");
    __builtin_unreachable();
}
#define CHECK(test) do { if (!(test)) finish(__LINE__ % 255 + 1); } while (0)

static void *memcpy(void *dst, const void *src, u32 bytes)
{
    u32 i;
    for (i = 0; i < bytes; i++) ((u8 *)dst)[i] = ((const u8 *)src)[i];
    return dst;
}
static void bzero(void *dst, u32 bytes)
{
    u32 i;
    for (i = 0; i < bytes; i++) ((u8 *)dst)[i] = 0;
}
static void *mempAllocBytesInBank(u32 bytes, u8 bank)
{
    void *result = pool + used;
    CHECK(bank == MEMPOOL_STAGE && bytes <= sizeof(pool) - used);
    /* Same exact-size contract as the production stage allocator. */
    used += bytes;
    return result;
}
static void romCopy(void *dst, void *src, u32 bytes)
{
    CHECK(((u32)dst & 15) == 0 && (bytes & 15) == 0);
    CHECK((u8 *)src >= bankData && (u8 *)src + bytes <= bankData + metadataSize);
    transfers++;
    memcpy(dst, src, bytes);
}

static void fixture(u32 count)
{
    u32 i, *header = (u32 *)bankData;
    metadataSize = 16 + count * CUSTOM_PROP_ENTRY_SIZE;
    bzero(bankData, sizeof(bankData));
    header[0] = CUSTOM_PROP_MAGIC;
    header[1] = count;
    header[2] = CUSTOM_PROP_ENTRY_SIZE;
    header[3] = CUSTOM_PROP_BASE;
    for (i = 0; i < count; i++)
    {
        u32 *row = (u32 *)(bankData + 16 + i * CUSTOM_PROP_ENTRY_SIZE);
        char *name = (char *)row;
        f32 radius = 10.0f + i, scale = 0.1f;
        name[0] = 'P'; name[1] = 'a' + i / 26; name[2] = 'a' + i % 26; name[3] = 'Z';
        row[16] = metadataSize + i * 176;
        row[17] = 176;
        memcpy(&row[18], &radius, 4);
        memcpy(&row[20], &scale, 4);
    }
    g_CustomPropRomConfig.version = CUSTOM_PROP_CONFIG_VERSION;
    g_CustomPropRomConfig.romStart = (u32)bankData;
    g_CustomPropRomConfig.romSize = metadataSize + count * 176;
    used = transfers = 0;
}

void _start(void)
{
    u32 count, i;
    CHECK(sizeof(void *) == 4 && sizeof(CustomPropRuntime) == 140);
    for (count = 0; count <= CUSTOM_PROP_CAPACITY; count++)
    {
        u8 *setup;
        fixture(count);
        customPropsInit();
        CHECK(g_CustomPropCount == (s32)count);
        CHECK(transfers == count + 1);
        CHECK((used & 15) == 0);
        CHECK(used >= count * sizeof(CustomPropRuntime));
        CHECK(used < count * sizeof(CustomPropRuntime) + 16);
        for (i = 0; i < count; i++)
        {
            CustomPropRuntime *item = &g_CustomProps[i];
            u32 scaleBits;
            CHECK(item->header.RootNode == NULL);
            CHECK(item->header.Skeleton == &skeleton_standard_object);
            CHECK(item->header.numMatrices == 1 && item->header.numSwitches == 1);
            CHECK(item->header.BoundingVolumeRadius == 10.0f + i);
            CHECK(item->model.header == &item->header && item->model.filename == item->name);
            memcpy(&scaleBits, &item->model.scale, 4);
            CHECK(scaleBits == 0x3dcccccdu);
            CHECK(item->file.index == OBJ_INDEX_MAX + (s32)i);
            CHECK(item->file.hw_address == bankData + metadataSize + i * 176);
            CHECK(item->info.rom_size == 176);
        }
        /* No placed instances: merely registering models must leave the next
         * setup transfer aligned and its bytes intact. */
        setup = mempAllocBytesInBank(64, MEMPOOL_STAGE);
        romCopy(setup, bankData, 16);
        for (i = 0; i < 16; i++) CHECK(setup[i] == bankData[i]);
        customPropsReset();
        CHECK(g_CustomPropCount == 0 && g_CustomProps == NULL);
    }
    fixture(1);
    g_CustomPropRomConfig.romStart = 0;
    customPropsInit();
    CHECK(used == 0 && transfers == 0 && g_CustomPropCount == 0);
    fixture(1);
    ((u32 *)bankData)[1] = CUSTOM_PROP_CAPACITY + 1;
    customPropsInit();
    CHECK(used == 0 && g_CustomPropCount == 0);
    finish(0);
}
