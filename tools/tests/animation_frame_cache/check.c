static u8 rom[65536] __attribute__((aligned(256)));
static u8 scratchBytes[4096] __attribute__((aligned(16)));
static u8 otherScratch[4096] __attribute__((aligned(16)));
static struct ModelAnimationScratch scratch, other;
static u32 transfers;

static void finish(int status) __attribute__((noreturn));
static void finish(int status)
{
    __asm__ volatile("int $0x80" : : "a"(1), "b"(status) : "memory");
    __builtin_unreachable();
}
#define CHECK(test) do { if (!(test)) finish(__LINE__ % 255 + 1); } while (0)
#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

static void *memcpy(void *dest, const void *source, u32 size)
{
    for (u32 i = 0; i < size; i++) ((u8 *)dest)[i] = ((const u8 *)source)[i];
    return dest;
}
static void fill(u8 *dest, u32 size, u8 value)
{
    for (u32 i = 0; i < size; i++) dest[i] = value;
}
static void equal(const u8 *a, const u8 *b, u32 size)
{
    for (u32 i = 0; i < size; i++) CHECK(a[i] == b[i]);
}
static int contains(const void *base, u32 bytes, const void *p, u32 size)
{
    return (u32)p >= (u32)base && size <= bytes && (u32)p - (u32)base <= bytes - size;
}
static void romCopy(void *dest, void *source, u32 size)
{
    CHECK(!((u32)dest & 15) && !((u32)source & 1) && !(size & 15));
    CHECK(contains(rom, sizeof(rom), source, size) || contains(stockRom, sizeof(stockRom), source, size));
    CHECK(contains(g_ModelAnimFrameCacheData, sizeof(g_ModelAnimFrameCacheData), dest, size)
        || contains(scratchBytes, sizeof(scratchBytes), dest, size)
        || contains(otherScratch, sizeof(otherScratch), dest, size));
    transfers++;
    memcpy(dest, source, size);
}
static void reset(void)
{
    fill(scratchBytes, sizeof(scratchBytes), 0xa5);
    fill(otherScratch, sizeof(otherScratch), 0xa5);
    scratch.bufferStart = scratch.nextFree = (char *)scratchBytes;
    other.bufferStart = other.nextFree = (char *)otherScratch;
    initAnimationsBuffer(&scratch);
    transfers = 0;
}
static u8 *load(const u8 *source, u32 bytes)
{
    ModelAnimation animation = {0};
    animation.address = (s32)source;
    animation.unk0E = bytes * 8;
    u32 start = ((u32)g_ModelAnimationScratch->nextFree + 15) & ~15u;
    u32 size = (bytes + ((u32)source & 1) + 15) & ~15u;
    u8 *result = loadAnimationFrame(&animation, 0);
    CHECK((u32)result == start + ((u32)source & 1));
    CHECK((u32)g_ModelAnimationScratch->nextFree == start + size);
    equal(result, source, bytes);
    return result;
}
static void stock(void)
{
    reset();
    for (u32 i = 0; i < COUNT(stockFrames); i++) {
        const u8 *source = stockRom + stockFrames[i].offset;
        modelResetAnimationsScratchBuffer();
        load(source, stockFrames[i].size);
        u32 count = transfers;
        modelResetAnimationsScratchBuffer();
        load(source, stockFrames[i].size);
        CHECK(transfers == count);
    }
}
static void capacity(void)
{
    reset();
    /* Each 4096-byte step collides; the 16 two-byte offsets span all sets. */
    for (u32 pass = 0; pass < 3; pass++) {
        for (u32 way = 0; way < 4; way++) for (u32 set = 0; set < 16; set++) {
            modelResetAnimationsScratchBuffer();
            load(rom + way * 4096 + set * 2, 90);
        }
        CHECK(transfers == 64);
    }
    reset();
    for (u32 update = 0; update < 120; update++) for (u32 guard = 0; guard < 8; guard++) {
        u8 *frames[4];
        modelResetAnimationsScratchBuffer();
        for (u32 f = 0; f < 4; f++) {
            u32 n = guard * 4 + f;
            frames[f] = load(rom + (n / 16) * 4096 + (n % 16) * 2, 90);
        }
        /* Production resets scratch before building matrices from these pointers. */
        modelResetAnimationsScratchBuffer();
        for (u32 f = 0; f < 4; f++) {
            u32 n = guard * 4 + f;
            equal(frames[f], rom + (n / 16) * 4096 + (n % 16) * 2, 90);
        }
    }
    CHECK(transfers == 32);
}
static void collisions(void)
{
    reset();
    u8 *held[4];
    for (u32 i = 0; i < 4; i++) held[i] = load(rom + i * 4096, 90);
    CHECK(transfers == 4);
    /* Refresh the oldest; the second entry must now be evicted first. */
    load(rom, 90); CHECK(transfers == 4);
    load(rom + 4 * 4096, 90); CHECK(transfers == 5);
    load(rom, 90); CHECK(transfers == 5);
    load(rom + 4096, 90); CHECK(transfers == 6);
    for (u32 i = 0; i < 4; i++) equal(held[i], rom + i * 4096, 90);
    /* Sustained set contention must return exact bytes, including after ranks
     * have changed many times; the cache must not depend on a wrapping clock. */
    for (u32 i = 0; i < 10000; i++) {
        modelResetAnimationsScratchBuffer();
        load(rom + (i % 8) * 4096, 90);
    }
}
static void sizes(void)
{
    reset();
    for (u32 bytes = 1; bytes <= 192; bytes++) for (u32 odd = 0; odd < 2; odd++) {
        const u8 *source = rom + 1000 + odd;
        modelResetAnimationFrameCache();
        modelResetAnimationsScratchBuffer();
        g_ModelAnimationScratch->nextFree++; /* Exercise unaligned scratch starts. */
        load(source, bytes);
        u32 count = transfers;
        modelResetAnimationsScratchBuffer();
        load(source, bytes);
        CHECK(transfers == count + ((bytes + odd + 15) / 16 > 8));
    }
    /* The same ROM address with a different transfer length is a different tag. */
    reset();load(rom, 16);load(rom, 90);CHECK(transfers == 2);
    load(rom, 16);load(rom, 90);CHECK(transfers == 2);
    /* Two logical frames may share an aligned transfer. Their returned offsets
     * still differ; the odd frame must not pick up the preceding byte. */
    reset();load(rom, 16);load(rom + 1, 15);CHECK(transfers == 1);
    /* Exercise actual frame indexing with odd-length strides. */
    ModelAnimation animation = {0};animation.address = (s32)rom;animation.unk0E = 89 * 8;
    for (s32 frame = 0; frame < 40; frame++) {
        modelResetAnimationsScratchBuffer();
        u8 *result = loadAnimationFrame(&animation, frame);
        equal(result, rom + frame * 89, 89);
    }
}
static void lifecycle(void)
{
    reset();load(rom, 90);CHECK(transfers == 1);
    modelResetAnimationsScratchBuffer();load(rom, 90);CHECK(transfers == 1);
    /* Reinitializing the same buffer must invalidate old ROM tags. */
    initAnimationsBuffer(&scratch);load(rom, 90);CHECK(transfers == 2);
    initAnimationsBuffer(&other);load(rom, 90);CHECK(transfers == 3);
    initAnimationsBuffer(NULL);
    ModelAnimation animation = {0};animation.address = (s32)rom;animation.unk0E = 90 * 8;
    CHECK(loadAnimationFrame(&animation, 0) == NULL && transfers == 3);
    animation.address = 0x80001234; /* RAM frames are returned directly, including without scratch. */
    CHECK((u32)loadAnimationFrame(&animation, 3) == 0x80001234u + 3 * 90 && transfers == 3);
    modelResetAnimationsScratchBuffer();
    initAnimationsBuffer(&scratch);
    CHECK((u32)loadAnimationFrame(&animation, 3) == 0x80001234u + 3 * 90 && transfers == 3);
}
void _start(void)
{
    CHECK(sizeof(void *) == 4);
    for (u32 i = 0; i < sizeof(rom); i++) rom[i] = (i * 31u) ^ (i >> 6) ^ (i >> 12);
    fill(g_ModelAnimFrameCacheData, sizeof(g_ModelAnimFrameCacheData), 0x5a);
    stock();capacity();collisions();sizes();lifecycle();
    u32 base = ((u32)g_ModelAnimFrameCacheData + 15) & ~15u;
    for (u32 p = (u32)g_ModelAnimFrameCacheData; p < base; p++) CHECK(*(u8 *)p == 0x5a);
    for (u32 p = base + 64 * 128; p < (u32)g_ModelAnimFrameCacheData + sizeof(g_ModelAnimFrameCacheData); p++) CHECK(*(u8 *)p == 0x5a);
    finish(0);
}
