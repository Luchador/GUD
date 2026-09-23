/* Appended to the production functions by run.py. */
static void CheckBytes(const u8 *expected, const u8 *resident, int bytes,
        int format, int width, int height, int dxt)
{
    u8 tmem[4096];
    int rowbytes = TexImportRowBytes(format, width);
    int swapbit = g_TexFormatDepths[format] == G_IM_SIZ_32b ? 8 : 4;
    int i;
    assert(bytes <= sizeof(tmem));
    /* Model LoadBlock's odd-line word exchange, independently of the CPU
     * swap function. These fixtures have power-of-two row lengths. */
    for (i = 0; i < bytes; i++) {
        int word = i / 8;
        int source = dxt && ((word * dxt / 2048) & 1) ? i ^ swapbit : i;
        assert(source < bytes);
        tmem[i] = resident[source];
    }
    /* Texture fetch uses the interleaved address on every odd texel row. */
    for (i = 0; i < rowbytes * height; i++) {
        int address = ((i / rowbytes) & 1) ? i ^ swapbit : i;
        if (tmem[address] != expected[i]) {
            fprintf(stderr, "Wrong texel: format=%d row=%d byte=%d DXT=%d\n",
                    format, i / rowbytes, i % rowbytes, dxt);
            abort();
        }
    }
}

static int LoadDxt(Gfx *start, Gfx *end, int expected)
{
    int found = 0;
    for (; start != end; start++) {
        if ((start->words.w0 >> 24) == G_LOADBLOCK) {
            int dxt = start->words.w1 & 0xfff;
            if (dxt != expected) {
                fprintf(stderr, "LoadBlock DXT %d, expected %d\n", dxt, expected);
                abort();
            }
            found++;
        }
    }
    assert(found == 1);
    return expected;
}

static Gfx *Upload(Gfx *gdl, struct tex *tex, int route)
{
    sImageTableEntry config = {0};
    sub_GAME_7F0CC4C8();
    switch (route) {
    case 0: return texWriteLoadToTmemZero(gdl, tex);
    case 1: return texHandleType3(gdl, (u32 *)tex, 2, 0, 2); /* Mirrored door. */
    case 2: return texHandleType4(gdl, (u32 *)tex, 1, 1, 0);
    default:
        config.index = 0x10000;
        config.width = tex->width; config.height = tex->height;
        config.format = tex->gbiformat; config.depth = tex->depth;
        config.flagsS = G_TX_MIRROR;
        g_Lookup = route == 4 ? NULL : tex;
        texSelect(&gdl, &config, 1, 1, 0);
        return gdl;
    }
}

static void TestImage(int format, int mipmaps, int explicitlods, int zerocount)
{
    TexPixel pixels[32 * 32];
    TexImportOptions options = {0};
    u8 resident[8192];
    u8 *record;
    DWORD size;
    const char *why = "";
    struct tex tex = {0};
    struct texpool pool = {0};
    Gfx commands[128], *end;
    int width = 32, height = 32, i, route, bytes, offset, basedxt;
    options.format = format; options.mipmaps = mipmaps;
    for (i = 0; i < ARRAYCOUNT(pixels); i++) {
        int x = i % width, y = i / width;
        /* Vertical contrast exposes alternating-row errors; also vary rows. */
        pixels[i] = (TexPixel){x * 8, (31 - x) * 8, y * 8, 255};
    }
    assert(TexEncodeRecord(pixels, width, height, &options, &record, &size, &why));
    record[4] = explicitlods;
    if (!explicitlods) record[6] = 1;
    if (zerocount) record[5] = 0;
    g_Record = record; g_RecordSize = size;
    pool.leftpos = resident; pool.rightpos = &tex;
    memset(resident, 0xcc, sizeof(resident));
    g_TexCacheCount = 0;
    bytes = texLoadRaw(record, 0, &pool);
    assert(bytes > 0 && bytes <= sizeof(resident));
    tex.data = resident;
    assert(tex.maxlod == (zerocount ? 0 : mipmaps + 1));
    offset = texReadRawU32(record + 20);
    basedxt = 2048 / (TexImportRowBytes(format, width) / 8);

    /* Pool images with nonzero level counts must load without another swap.
     * Zero-count records retain the width-based upload regardless of flag 4. */
    for (route = 0; route < 4; route++) {
        int expected = tex.maxlod ? 0 : basedxt;
        end = Upload(commands, &tex, route);
        LoadDxt(commands, end, expected);
        CheckBytes(record + offset, resident, TexImportRowBytes(format, width) * height,
                   format, width, height, expected);
    }

    if (!zerocount && explicitlods) {
        /* All authored mip levels keep their own row parity and byte layout. */
        int level, at = 0;
        end = texHandleType2(commands, &tex, 2, 0, 2);
        LoadDxt(commands, end, 0);
        for (level = 0; level <= mipmaps; level++) {
            u8 *desc = record + 16 + level * 12;
            int count = texReadRawU32(desc + 8);
            CheckBytes(record + texReadRawU32(desc + 4), resident + at, count,
                       format, desc[1], desc[2], 0);
            at += count;
        }
    }

    if (zerocount && !explicitlods && format < TEXFORMAT_RGBA16_CI8) {
        /* A caller-supplied pointer has no pool descriptor: retain its DXT. */
        end = Upload(commands, &tex, 4);
        LoadDxt(commands, end, basedxt);
    }
    free(record);
}

static void SpecialBank(void)
{
    /* Explicit flag + zero LOD count is NOT the new imported-image case.
     * Emulate four already-arranged tiles, as used by the Laser flash. */
    TexPixel pixels[32 * 32];
    TexImportOptions options = {TEXFORMAT_I8, 0, 0, 0};
    u8 *record, resident[2048], saved[1024];
    DWORD size;
    const char *why = "";
    struct tex tex = {0};
    struct texpool pool = {0};
    Gfx commands[128], *end;
    int i, offset;
    for (i = 0; i < ARRAYCOUNT(pixels); i++) pixels[i] = (TexPixel){i, i, i, 255};
    assert(TexEncodeRecord(pixels, 32, 32, &options, &record, &size, &why));
    record[5] = 0;
    offset = texReadRawU32(record + 20);
    for (i = 0; i < 4; i++) texSwapAltRowBytes(record + offset + i * 256, 16, 16, TEXFORMAT_I8);
    memcpy(saved, record + offset, sizeof(saved));
    g_Record = record; g_RecordSize = size;
    pool.leftpos = resident; pool.rightpos = &tex; g_TexCacheCount = 0;
    assert(texLoadRaw(record, 0, &pool) == sizeof(saved));
    assert(!memcmp(saved, resident, sizeof(saved)));
    tex.data = resident;
    end = Upload(commands, &tex, 0);
    LoadDxt(commands, end, 512); /* Existing bank handling is unchanged. */
    free(record);
}

int main(void)
{
    int format, mipmaps;
    for (format = 0; format <= TEXFORMAT_IA16_CI4; format++) {
        for (mipmaps = 0; mipmaps <= 2; mipmaps++) {
            if (TexImportTmemBytes(32, 32, format, mipmaps) <= TexImportTmemLimit(format))
                TestImage(format, mipmaps, 1, 0);
        }
        TestImage(format, 0, 0, 0); /* Implicit one-level textures also swap. */
        TestImage(format, 0, 0, 1); /* Ordinary unswapped texture. */
        TestImage(format, 0, 1, 1); /* Explicit flag alone does not imply a swap. */
    }
    TestImage(TEXFORMAT_I8, 2, 0, 0); /* Runtime-generated mip chain. */
    SpecialBank();
    puts("PASS: encoded imports, raw loader and upload/fetch byte agreement across all 13 formats;");
    puts("      base-only and mipmapped images, tile paths, direct draws, raw pointers and zero-count banks.");
    return 0;
}
