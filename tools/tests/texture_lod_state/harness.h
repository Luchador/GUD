#define G_ON 1
static struct tex g_Textures[4096];
static int ptr_texture_alloc_start;
static int g_Requests;
static void renderInvalidateDisplayListCache(void) {}
static void sub_GAME_7F0CC4C8(void) {}
static void texLoadFromTextureNum(int id, void *pool)
{ assert(id >= 0 && id < 4096 && pool); g_Requests++; }
static struct tex *texFindInPool(int id, void *pool)
{ return &g_Textures[id]; }

/* Record the selected image in a command, without emulating TMEM or pixels. */
static Gfx *testUpload(Gfx *out, struct tex *tex, ...)
{ out->words.w0 = 0xfd000000; out++->words.w1 = tex->texturenum; return out; }
#define texHandleType0 testUpload
#define texHandleType1 testUpload
#define texHandleType2 testUpload
#define texHandleType3 testUpload
#define texHandleType4 testUpload
static Gfx *dyntexConfigureTwoLayerWater(Gfx *out, int enabled) { return out; }
static Gfx *dyntexConfigureTwoLayerCiWater(Gfx *out) { return out; }
static int check_if_imageID_is_light(int id) { return id == 123; }
static void lightFixtureEntryBegin(Gfx *out) { assert(out); }
static void lightFixtureEntryEnd(Gfx *out) { assert(out); }
static void modelOneCycleInvalidateGdlRange(Gfx *first, Gfx *last) { assert(last >= first); }
