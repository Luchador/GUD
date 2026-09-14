static Vertex vertices[] = {
    {{-10, -10, 10}, 0, 0, 0, 255, 255, 255, 255},
    {{10, -10, 10}, 0, 0, 0, 255, 255, 255, 255},
    {{0, 10, 10}, 0, 0, 0, 255, 255, 255, 255}
};

static u32 loadTexture(s32 id)
{
    struct texdataprefix *prefix = (void *)(ram + 0x1000);
    struct tex *tex = (void *)(ram + 0x4000);
    memset(tex, 0, sizeof(*tex));
    tex->texturenum = id;
    tex->data = (u8 *)(prefix + 1);
    prefix->magic = TEX_DATA_PREFIX_MAGIC;
    prefix->descriptor = tex;
    return K0_TO_PHYS(tex->data);
}

static void displayList(Gfx *gdl, u32 texture, s32 packed)
{
    memset(gdl, 0, 5 * sizeof(*gdl));
    gdl[0].bytes[0] = 0xe7; /* pipe sync/list sentinel */
    gdl[1].bytes[0] = G_SETTIMG;
    word(gdl[1].bytes + 4, texture);
    gdl[2].bytes[0] = G_VTX;
    word(gdl[2].bytes + 4, 0x0e000000);
    gdl[3].bytes[0] = packed ? (u8)G_TRI4 : (u8)G_TRI1;
    if (packed) {
        gdl[3].bytes[3] = 2;
        gdl[3].bytes[7] = 0x10; /* first tri: 0, 1, 2; remaining tris degenerate */
    } else {
        gdl[3].bytes[6] = 10;
        gdl[3].bytes[7] = 20;
    }
    gdl[4].bytes[0] = (u8)G_ENDDL;
}

static void checkHit(s32 texture, s32 packed)
{
    Gfx gdl[5], empty[1];
    HitThing hit;
    RoomVtxBatchBounds batch = {2};
    coord3d from = {{{0, 0, 0}}}, to = {{{0, 0, 20}}}, dir = {{{0, 0, 1}}};
    ShotData shot = {0, {{{0, 0, 1}}}};
    u32 address = texture >= 0 ? loadTexture(texture) : 0;
    displayList(gdl, address, packed);
    g_BgRoomInfo[1].primaryGdl = gdl;
    g_BgRoomInfo[1].vertices = vertices;
    memset(&hit, 0, sizeof(hit));
    assert(bgTestRayIntersectionInRoom(&from, &to, &dir, &batch, 1, &hit) == (texture != 0x4fd));
    if (texture != 0x4fd) {
        assert(hit.texturenum == texture);
        assert(hit.tricmd == &gdl[3] && hit.unk28 == packed);
        breaks = 0;
        chrpropCreateBgShotImpact(&shot, &hit, 1);
        assert(soundTexture == texture);
        assert(breaks == check_if_imageID_is_light(texture));
        if (breaks) assert(brokenCommand == &gdl[3] && brokenTri == packed && brokenRoom == 1);
    }
    /* The same material lookup must work on either object display list. */
    memset(empty, 0, sizeof(empty));
    empty[0].bytes[0] = (u8)G_ENDDL;
    assert(bgTestHitOnObj(&from, &to, &dir, gdl, NULL, vertices, &hit));
    assert(hit.texturenum == texture && hit.tricmd == &gdl[3]);
    assert(bgTestHitOnObj(&from, &to, &dir, empty, gdl, vertices, &hit));
    assert(hit.texturenum == texture && hit.tricmd == &gdl[3]);
}

int main(void)
{
    s32 i, packed;
    const s32 textures[] = {IMAGE_WALL_LAMP, IMAGE_203_LIGHT, IMAGE_205_LIGHT,
        IMAGE_252_LIGHT, 0xfd, IMAGE_PANEL_LAMP, IMAGE_255_LIGHT, IMAGE_256_LIGHT,
        IMAGE_HANGING_LAMP, IMAGE_NEON_LAMP, IMAGE_LINEAR_LAMP, 0, 123, 3000, 0x4fd, -1};
    u32 address = loadTexture(IMAGE_WALL_LAMP);
    struct texdataprefix *prefix = (void *)(ram + 0x1000);
    assert(sizeof(Vertex) == 16 && sizeof(Gfx) == 8);
    assert(legacyTextureHalfword(address - 8) == 0x4755);
    assert(!check_if_imageID_is_light(0x4755));
    assert(texFindByData(address)->texturenum == IMAGE_WALL_LAMP);
    assert(texFindByData(address | 0x80000000)->texturenum == IMAGE_WALL_LAMP);
    assert(!texFindByData(0) && !texFindByData(osMemSize));
    prefix->magic = 0;
    assert(!texFindByData(address));
    prefix->magic = TEX_DATA_PREFIX_MAGIC;
    prefix->descriptor->data++;
    assert(!texFindByData(address));
    for (i = 0; i < (s32)(sizeof(textures) / sizeof(*textures)); i++)
        for (packed = 0; packed < 2; packed++) checkHit(textures[i], packed);
    puts("PASS: TRI1/TRI4 hits identify all 11 light textures and dispatch light breaks.");
    puts("PASS: ordinary/custom materials, Archives shaft exclusion, unknown texture fallback, object primary/secondary hits.");
    puts("PASS: current metadata resolves physical/KSEG0 addresses and rejects invalid prefixes.");
    return 0;
}
