/* Exercise the actual texture expander's dispatch. Texture allocation is
 * stubbed so policy tags cannot silently turn into requests for image IDs. */
static int ptr_texture_alloc_start;
static int g_TestTextureRequests;
static int g_TestLightStarts, g_TestLightEnds;
static void sub_GAME_7F0CC4C8(void) {}
static void texLoadFromTextureNum(int id, void *pool)
{ (void)id; assert(pool); g_TestTextureRequests++; }
static struct tex *texFindInPool(int id, void *pool)
{ (void)id; (void)pool; return &g_TestTexture; }
static Gfx *testTextureCommand(Gfx *out, ...)
{ return out; }
static Gfx *testTextureUpload(Gfx *out, ...)
{
    if (getenv("GUD_ENV_FIXTURE")) { out->words.w0=0xfd100000u; out++->words.w1=0x123450; }
    return out;
}
#define texWriteTextureCmd testTextureCommand
#define texHandleType0 testTextureUpload
#define texHandleType1 testTextureUpload
#define texHandleType2 testTextureUpload
#define texHandleType3 testTextureUpload
#define texHandleType4 testTextureUpload
#define dyntexConfigureTwoLayerWater testTextureCommand
#define dyntexConfigureTwoLayerCiWater testTextureCommand
static int check_if_imageID_is_light(int id) { return id == 123; }
static void lightFixtureEntryBegin(Gfx *out) { assert(out); g_TestLightStarts++; }
static void lightFixtureEntryEnd(Gfx *out) { assert(out); g_TestLightEnds++; }
static void modelOneCycleInvalidateGdlRange(Gfx *first, Gfx *last) { assert(last >= first); }
s32 texLoadFromGdl(Gfx *src, s32 srcsize, Gfx *dst, void *texpool);

static void texture_marker_checks(void)
{
    Gfx src[16], out[32], *p;
    int policy, size, bytes, i, tags;
    p = src;
    for (policy = BG_SURFACE_AUTO; policy <= BG_SURFACE_BLEND; policy++) {
        gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(policy, policy ? G_RM_AA_ZB_XLU_SURF2 : 0));
        gSP1Triangle(p++, 0, 1, 2, 0);
    }
    gSPEndDisplayList(p++);
    size = (p - src) * 8;
    bytes = texLoadFromGdl(src, size, out, NULL);
    assert(bytes == size && !memcmp(src, out, size));
    assert(!g_TestTextureRequests && !g_TestLightStarts && !g_TestLightEnds);

    p = src;
    /* A normal type-0 request can share the tag's high byte, but never its
     * reserved bits. Type-1 uses those bits, but a different first word. */
    p->words.w0 = 0xc0000000; p++->words.w1 = 0x4700007b;
    gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND, G_RM_AA_ZB_XLU_SURF2));
    gSP1Triangle(p++, 0, 1, 2, 0);
    gDPNoOpTag(p++, BG_SURFACE_TAG_VALUE(BG_SURFACE_AUTO, 0));
    p->words.w0 = 0xc0000001; p++->words.w1 = BG_SURFACE_TAG_VALUE(BG_SURFACE_BLEND, G_RM_AA_ZB_XLU_SURF2);
    gSP1Triangle(p++, 0, 1, 2, 0);
    gSPEndDisplayList(p++);
    bytes = texLoadFromGdl(src, (p - src) * 8, out, NULL);
    assert(g_TestTextureRequests == 3); /* Type 0 plus two type-1 images. */
    assert(g_TestLightStarts == 1 && g_TestLightEnds == 1);
    for (tags = i = 0; i < bytes / 8; i++) if (BG_SURFACE_IS_MARKER(out[i].words.w0, out[i].words.w1)) tags++;
    assert(tags == 2);
}
