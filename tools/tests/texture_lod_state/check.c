typedef struct Draw {
    u32 image, texture, scale;
} Draw;

static unsigned int Check(Gfx *src, unsigned int count, const char *label)
{
    Gfx *out = calloc(count * 16 + 16, sizeof(*out));
    Gfx *copy = malloc(count * sizeof(*copy));
    Draw *expected = calloc(count, sizeof(*expected));
    unsigned int image = 4096, texture = 0xbb000001, scale = 0xffffffff;
    unsigned int draws = 0, requests = 0, seen = 0, i;
    assert(out && copy && expected);
    memcpy(copy, src, count * sizeof(*copy));
    for (i = 0; i < count; i++) {
        u32 a = src[i].words.w0, b = src[i].words.w1, op = a >> 24;
        if (op == 0xbb) { texture = a; scale = b; }
        if (op == 0xc0 && !BG_EDITOR_IS_MARKER(a, b)) {
            image = b & 0xfff;
            requests += (a & 7) == TEXTURETYPE_DETAIL ? 2 : 1;
        }
        if ((op == 0xb1 || op == 0xbf) && image < 4096) {
            u32 levels = g_Textures[image].maxlod;
            expected[draws++] = (Draw){image,
                (texture & ~0x3800u) | ((levels ? levels - 1 : 0) << 11), scale};
        }
    }
    g_Requests = 0;
    int bytes = texLoadFromGdl(src, count * 8, out, NULL);
    assert(bytes >= 0 && bytes % 8 == 0 && (unsigned)bytes / 8 <= count * 16 + 16);
    assert(!memcmp(copy, src, count * sizeof(*copy)) && g_Requests == (int)requests);
    image = 4096; texture = scale = 0;
    for (i = 0; i < (unsigned)bytes / 8; i++) {
        u32 a = out[i].words.w0, b = out[i].words.w1, op = a >> 24;
        if (op == 0xbb) { texture = a; scale = b; }
        if (op == 0xfd) { image = b; }
        if ((op == 0xb1 || op == 0xbf) && image < 4096) {
            assert(seen < draws);
            const Draw *want = &expected[seen++];
            u32 mask = (want->texture & 0xff) ? 0xffffffffu : ~0x3800u;
            if (image != want->image || (texture & mask) != (want->texture & mask) || scale != want->scale) {
                fprintf(stderr, "%s draw %u image %u: texture %08x/%08x, expected %08x/%08x\n",
                    label, seen, image, texture, scale, want->texture, want->scale);
                abort();
            }
        }
    }
    assert(seen == draws);
    free(out); free(copy); free(expected);
    return draws;
}

static Gfx *Put(Gfx *p, u32 a, u32 b)
{ p->words.w0 = a; p++->words.w1 = b; return p; }
static Gfx *Image(Gfx *p, int id) { return Put(p, 0xc0080002, id); }
static Gfx *Triangle(Gfx *p, int tri4)
{ return Put(p, tri4 ? 0xb1002222 : 0xbf000000, tri4 ? 0x10101010 : 0x00000a14); }

static void Synthetic(void)
{
    Gfx src[64], *p;
    /* All first/second/final LOD combinations, with both triangle opcodes.
     * A repeated binding before the third image also exercises the case
     * where no new command was emitted and the saved command is in use. */
    for (int a = 0; a <= 7; a++) for (int b = 0; b <= 7; b++)
    for (int c = 0; c <= 7; c++) for (int tri4 = 0; tri4 < 2; tri4++) {
        g_Textures[1].maxlod = a; g_Textures[2].maxlod = b; g_Textures[3].maxlod = c;
        p = Put(src, 0xbb002b01, 0x43218765);
        p = Triangle(Image(p, 1), tri4);
        p = Triangle(Image(p, 2), tri4);
        p = Image(p, 2); /* Same LOD after a draw: must not make the old command writable. */
        p = Put(p, BG_SURFACE_MARKER, BG_FOG_TAG | BG_FOG_OFF);
        p = Triangle(Image(p, 3), tri4);
        p = Triangle(Image(p, 1), tri4);
        p = Put(p, 0xb8000000, 0);
        Check(src, p - src, "LOD transitions");
    }
    g_Textures[1].maxlod = 6; g_Textures[2].maxlod = 1; g_Textures[3].maxlod = 7;
    p = Image(src, 1); /* No authored G_TEXTURE: save the generated one. */
    p = Triangle(Image(p, 2), 0);
    p = Triangle(Image(p, 1), 1);
    p = Triangle(Image(p, 2), 1);
    p = Put(p, 0xb8000000, 0);
    Check(src, p - src, "Generated texture command");

    p = Put(src, 0xbb002b00, 0x12345678); /* Preserve tile, scale and G_OFF. */
    p = Triangle(Image(p, 1), 0);
    p = Triangle(Image(p, 2), 0);
    p = Put(p, 0xbb001201, 0x87654321);
    p = Image(p, 3); p = Image(p, 2); p = Triangle(Image(p, 1), 1);
    p = Put(p, BG_SURFACE_MARKER, BG_SURFACE_TAG_VALUE(BG_SURFACE_OPAQUE, 0));
    p = Put(p, BG_SURFACE_MARKER, BG_ALPHA_TAG | BG_ALPHA_VERTEX);
    p = Put(p, BG_SURFACE_MARKER, BG_ENV_TAG | BG_ENV_OFF);
    p = Put(p, BG_ENV_NORMAL_MARKER, BG_ENV_NORMAL_TAG | 0x1234);
    p = Triangle(Image(p, 2), 0);
    p = Put(p, 0xb8000000, 0);
    Check(src, p - src, "Authored state and editor tags");
    puts("PASS: 1,024 LOD transitions, TRI1/TRI4, pending uploads, generated commands, scales, tiles, enable state and editor tags.");
}

static u32 Read32(FILE *f)
{
    unsigned char b[4]; assert(fread(b, 1, 4, f) == 4);
    return ((u32)b[0] << 24) | ((u32)b[1] << 16) | ((u32)b[2] << 8) | b[3];
}

int main(int argc, char **argv)
{
    for (int i = 0; i < 4096; i++) { g_Textures[i].texturenum = i; }
    Synthetic();
    if (argc > 1) {
        FILE *f = fopen(argv[1], "rb"); assert(f);
        unsigned int count = Read32(f), total = 0; assert(count <= 4096);
        for (unsigned int i = 0; i < count; i++) {
            int level = fgetc(f); assert(level >= 0 && level <= 7); g_Textures[i].maxlod = level;
        }
        unsigned int streams = Read32(f);
        for (unsigned int i = 0; i < streams; i++) {
            unsigned int room = Read32(f), layer = Read32(f), size = Read32(f);
            char label[64]; snprintf(label, sizeof(label), "Room %u layer %u", room, layer);
            assert(size && size % 8 == 0);
            Gfx *src = malloc(size); assert(src);
            for (unsigned int c = 0; c < size / 8; c++) {
                src[c].words.w0 = Read32(f); src[c].words.w1 = Read32(f);
            }
            total += Check(src, size / 8, label); free(src);
        }
        assert(fgetc(f) == EOF); fclose(f);
        printf("PASS: %u real BG streams, %u draw commands using the ROM's actual mip counts.\n", streams, total);
    }
    return 0;
}
