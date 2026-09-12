/* Host regression driver. Native model/glTF/edit-store code is production;
 * only project texture lookup, PNG encoding and the ROM directory are mocked.
 * The file shim is shared with the image import tests. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "modeledits.h"
#include "texload.h"

static const char *why = "";
static const char *assetpath;
#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #expr, why); exit(1); } } while (0)

static unsigned char *Read(const char *path, DWORD *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *data;
    long length;
    CHECK(file != NULL);
    CHECK(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file); CHECK(length > 0);
    CHECK(fseek(file, 0, SEEK_SET) == 0);
    *size = (DWORD)length; data = malloc(*size); CHECK(data != NULL);
    CHECK(fread(data, 1, *size, file) == *size); CHECK(fclose(file) == 0);
    return data;
}

BOOL TexGetProjectImageSize(const char *project, DWORD id, int *w, int *h)
{
    (void)project;
    *w = id == 0xeee ? 64 : 32; *h = 16;
    return TRUE;
}
BOOL TexLoadProjectImage(const char *project, DWORD id, TexPixel *out, int *w, int *h)
{
    TexGetProjectImageSize(project, id, w, h);
    memset(out, 255, (size_t)*w * *h * sizeof(*out));
    return TRUE;
}
BOOL TexEncodePng(const TexPixel *pixels, int w, int h, unsigned char **data, DWORD *size)
{
    /* The importer resolves GUD image IDs; it does not decode these PNGs. */
    static const unsigned char png[] = {137,80,78,71,13,10,26,10};
    (void)pixels; (void)w; (void)h;
    *size = sizeof(png); *data = malloc(*size); CHECK(*data != NULL);
    memcpy(*data, png, *size); return TRUE;
}
BOOL RomLoad(const char *path, RomFile *rom, const char **reason)
{
    (void)path; (void)reason;
    memset(rom, 0, sizeof(*rom)); rom->data = Read(assetpath, &rom->size); return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); }
BOOL RomFindFile(const RomFile *rom, const char *name, DWORD *offset, DWORD *size, const char **reason)
{
    (void)name; (void)reason;
    *offset = 0; *size = rom->size; return TRUE;
}

static GltfModelImport Copy(const ModelSource *source)
{
    GltfModelImport copy = {0};
    DWORD i;
    copy.count = source->count;
    copy.vertices = malloc(copy.count * 3 * sizeof(*copy.vertices));
    copy.tags = malloc(copy.count * sizeof(*copy.tags));
    copy.sourcevertices = malloc(copy.count * 3 * sizeof(*copy.sourcevertices));
    CHECK(copy.vertices && copy.tags && copy.sourcevertices);
    memcpy(copy.vertices, source->vertices, copy.count * 3 * sizeof(*copy.vertices));
    memcpy(copy.tags, source->tags, copy.count * sizeof(*copy.tags));
    for (i = 0; i < copy.count * 3; i++)
    {
        int w = 1, h = 1;
        if (BG_TEX_ID(copy.tags[i / 3]) != BG_TEX_NONE)
        { TexGetProjectImageSize("", BG_TEX_ID(copy.tags[i / 3]), &w, &h); }
        copy.vertices[i].s /= w; copy.vertices[i].t /= h;
        copy.sourcevertices[i] = i;
    }
    return copy;
}
static void Change(GltfModelImport *copy)
{
    DWORD i;
    for (i = 0; i < copy->count * 3; i++)
    {
        /* Nonuniform deformation, fractional quantization, wrapped UVs. */
        copy->vertices[i].x *= 0.75f;
        copy->vertices[i].y += 2.25f;
        copy->vertices[i].z -= 3.75f;
        copy->vertices[i].s += 2.0f;
        copy->vertices[i].t -= 1.0f;
    }
}
static int S16(const unsigned char *p)
{ int n = p[0] * 256 + p[1]; return n >= 32768 ? n - 65536 : n; }

static void Positions(const unsigned char *base, const ModelSource *source,
    const unsigned char *compiled, DWORD size, const GltfModelImport *edited)
{
    ModelSource check = {0};
    DWORD i;
    CHECK(ModelReadSource(compiled, size, &check, &why));
    CHECK(check.count == edited->count);
    for (i = 0; i < edited->count * 3; i++)
    {
        DWORD id = edited->sourcevertices[i], offset = source->vertexoffsets[id];
        const BgVertex *a = &edited->vertices[i], *b = &source->vertices[id];
        int x = (int)round(S16(base + offset) + (double)a->x - b->x);
        int y = (int)round(S16(base + offset + 2) + (double)a->y - b->y);
        int z = (int)round(S16(base + offset + 4) + (double)a->z - b->z);
        CHECK(S16(compiled + offset) == x && S16(compiled + offset + 2) == y
            && S16(compiled + offset + 4) == z);
        CHECK(fabsf(check.vertices[i].x - (b->x - S16(base + offset) + x)) < 0.001f);
        CHECK(fabsf(check.vertices[i].y - (b->y - S16(base + offset + 2) + y)) < 0.001f);
        CHECK(fabsf(check.vertices[i].z - (b->z - S16(base + offset + 4) + z)) < 0.001f);
        if (BG_TEX_ID(edited->tags[i / 3]) != BG_TEX_NONE)
        {
            CHECK(S16(compiled + offset + 8) == (int)round((double)a->s * 32 * 32));
            CHECK(S16(compiled + offset + 10) == (int)round((double)a->t * 16 * 32));
        }
    }
    ModelFreeSource(&check);
}
static void Reject(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *edit, const char *message)
{
    unsigned char *out = NULL;
    DWORD length = 0;
    CHECK(!ModelCompileImport(data, size, source, edit, "", &out, &length, &why));
    CHECK(out == NULL && length == 0 && strstr(why, message) != NULL);
}

static void Unit(const unsigned char *data, DWORD size, ModelSource *source)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out = NULL, *mask = calloc(size, 1);
    DWORD length, i, first = 0, second = 0;
    CHECK(mask != NULL);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    Change(&edit);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size); Positions(data, source, out, length, &edit);
    for (i = 0; i < source->count * 3; i++)
    {
        DWORD offset = source->vertexoffsets[i];
        memset(mask + offset, 1, 6); memset(mask + offset + 8, 1, 4);
    }
    for (i = 0; i < size; i++) { CHECK(mask[i] || data[i] == out[i]); }
    CHECK(memcmp(out, data, size)); free(out); free(mask);
    GltfFreeModelImport(&edit);

    /* These records must remain in their original matrix-local spaces. */
    if (strstr(assetpath, "mixed"))
    {
        CHECK(source->count == 2);
        CHECK(source->vertices[0].x == 10 && source->vertices[2].x == 130);
    }
    for (i = 1; i < source->count * 3 && !second; i++)
    {
        DWORD j;
        for (j = 0; j < i; j++)
        { if (source->vertexoffsets[i] == source->vertexoffsets[j]) { first = j; second = i; break; } }
    }
    CHECK(second != 0);
    edit = Copy(source);
    edit.vertices[first].x += 8;
    Reject(data, size, source, &edit, "different positions");
    edit.vertices[first].x -= 8; edit.vertices[first].s += 1;
    Reject(data, size, source, &edit, "different UVs");
    GltfFreeModelImport(&edit);
    edit = Copy(source);
    edit.vertices[0].x = NAN; Reject(data, size, source, &edit, "position");
    edit.vertices[0].x = 100000; Reject(data, size, source, &edit, "position");
    edit.vertices[0].x = source->vertices[0].x;
    edit.vertices[0].s = INFINITY; Reject(data, size, source, &edit, "UV");
    edit.vertices[0].s = 100000; Reject(data, size, source, &edit, "UV");
    GltfFreeModelImport(&edit);
    edit = Copy(source); edit.vertices[0].r ^= 128;
    Reject(data, size, source, &edit, "colors"); GltfFreeModelImport(&edit);
    edit = Copy(source); edit.sourcevertices[1] = 2; edit.sourcevertices[2] = 1;
    Reject(data, size, source, &edit, "winding"); GltfFreeModelImport(&edit);
    edit = Copy(source); edit.tags[0] = 0xeee;
    Reject(data, size, source, &edit, "dimensions"); GltfFreeModelImport(&edit);
    edit = Copy(source);
    {
        BgRenderFlags saved = source->flags[0];
        source->flags[0] |= BG_RENDER_ENVIRONMENT; edit.vertices[0].s += 1;
        Reject(data, size, source, &edit, "reflection UVs"); source->flags[0] = saved;
    }
    GltfFreeModelImport(&edit);

    /* Exporters may rotate a triangle's corner order without reversing it. */
    edit = Copy(source);
    {
        BgVertex saved = edit.vertices[0];
        edit.vertices[0] = edit.vertices[1]; edit.vertices[1] = edit.vertices[2]; edit.vertices[2] = saved;
        edit.sourcevertices[0] = 1; edit.sourcevertices[1] = 2; edit.sourcevertices[2] = 0;
        CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
        CHECK(length == size && !memcmp(out, data, size)); free(out);
    }
    GltfFreeModelImport(&edit);

    /* Material removal and same-size reassignment can accompany deformation. */
    edit = Copy(source); Change(&edit);
    for (i = 0; i < edit.count; i++) { edit.tags[i] = i ? 0xffd : BG_TEX_NONE; }
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    Positions(data, source, out, length, &edit);
    {
        ModelSource check = {0};
        CHECK(ModelReadSource(out, length, &check, &why));
        for (i = 0; i < edit.count; i++) { CHECK(BG_TEX_ID(check.tags[i]) == edit.tags[i]); }
        ModelFreeSource(&check);
    }
    free(out); GltfFreeModelImport(&edit);

    /* Exercise triangle-command rebuilding as well as the in-place path. */
    edit = Copy(source); Change(&edit); edit.count--;
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    Positions(data, source, out, length, &edit);
    {
        ModelSource rebuilt = {0};
        GltfModelImport again;
        unsigned char *twice;
        DWORD twicesize;
        CHECK(ModelReadSource(out, length, &rebuilt, &why));
        again = Copy(&rebuilt);
        CHECK(ModelCompileImport(out, length, &rebuilt, &again, "", &twice, &twicesize, &why));
        CHECK(twicesize == length && !memcmp(twice, out, length));
        free(twice); GltfFreeModelImport(&again); ModelFreeSource(&rebuilt);
    }
    free(out); GltfFreeModelImport(&edit);
    printf("PASS native edits, joint spaces, preserved commands, validation, face deletion: %s\n", assetpath);
}

static void Dynamic(const unsigned char *data, DWORD size, const ModelSource *source)
{
    GltfModelImport edit = Copy(source);
    unsigned char *out;
    DWORD length, i;
    CHECK(source->count == 2 && source->listcount == 2 && source->lists[1].preserve);
    CHECK(ModelCompileImport(data, size, source, &edit, "", &out, &length, &why));
    CHECK(length == size && !memcmp(out, data, size)); free(out);
    for (i = 0; i < edit.count * 3; i++) { edit.vertices[i].y += 1; }
    Reject(data, size, source, &edit, "dynamic model effect");
    GltfFreeModelImport(&edit);
    printf("PASS shared dynamic vertices are preserved\n");
}

int main(int argc, char **argv)
{
    unsigned char *data;
    DWORD size;
    ModelSource source = {0};
    CHECK(argc >= 4); assetpath = argv[2];
    data = Read(assetpath, &size);
    CHECK(ModelReadSource(data, size, &source, &why));
    CHECK(source.count >= 2);
    if (!strcmp(argv[1], "dynamic")) { Dynamic(data, size, &source); }
    else if (!strcmp(argv[1], "export"))
    {
        Unit(data, size, &source);
        CHECK(GltfWriteEditableModel(argv[3], "test", &source, ModelDataHash(data, size), &why));
    }
    else if (!strcmp(argv[1], "reject"))
    {
        GltfModelImport imported = {0};
        CHECK(!GltfReadModelImport(argv[3], ModelDataHash(data, size), &imported, &why));
        CHECK(argc == 5 && strstr(why, argv[4]));
        GltfFreeModelImport(&imported);
    }
    else
    {
        const unsigned char *pending;
        unsigned char *replacement;
        DWORD before, after, editedsize, replacementsize;
        char path[MAX_PATH];
        GltfModelImport expected = Copy(&source);
        BOOL changed = strcmp(argv[1], "noop") != 0;
        CHECK(argc == 5);
        CHECK(ModelEditsImport(argv[4], "Pjungle3_treeZ", argv[3], &before, &after, &why));
        CHECK(before == source.count && after == before);
        CHECK(ModelEditsHasUnsaved() == changed);
        if (changed)
        {
            Change(&expected);
            pending = ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why);
            CHECK(pending && editedsize == size);
            Positions(data, &source, pending, editedsize, &expected);
            /* A stale export fails without replacing the pending result. */
            CHECK(!ModelEditsImport(argv[4], "Pjungle3_treeZ", argv[3], &before, &after, &why));
            CHECK(strstr(why, "revision") && ModelEditsHasUnsaved());
            CHECK(ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why) == pending);
            CHECK(ModelEditsSave(argv[4], &why)); CHECK(!ModelEditsHasUnsaved());
            ModelEditsReset();
            pending = ModelEditsGetData(argv[4], "Pjungle3_treeZ", &editedsize, &why);
            CHECK(pending != NULL); Positions(data, &source, pending, editedsize, &expected);
            CHECK(ModelEditsReadReplacement(argv[4], "Pjungle3_treeZ", data, size,
                &replacement, &replacementsize, &why) == 1);
            CHECK(replacementsize == editedsize && !memcmp(replacement, pending, editedsize));
            free(replacement);
            snprintf(path, sizeof(path), "%s/models/objects/Pjungle3_treeZ.gltf", argv[4]);
            CHECK(ModelEditsImport(argv[4], "Pjungle3_treeZ", path, &before, &after, &why));
            CHECK(!ModelEditsHasUnsaved());
        }
        GltfFreeModelImport(&expected); ModelEditsReset();
        printf("PASS %s glTF/GLB round trip and model store: %s\n", argv[1], assetpath);
    }
    ModelFreeSource(&source); free(data); return 0;
}
