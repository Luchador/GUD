#include "modelthumbnail.h"
#include "modeledits.h"
#include "viewport.h"
#include "texload.h"
#include "editorpath.h"
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

/* Change this when camera, lighting, filtering or the capture format changes. */
#define THUMBNAIL_VERSION 1

static DWORD ThumbnailHash(DWORD hash, const void *bytes, size_t count)
{
    const unsigned char *p = bytes;
    while (count--) { hash = (hash ^ *p++) * 16777619u; }
    return hash;
}

void ModelThumbnailFree(ModelThumbnail *thumbnail)
{
    free(thumbnail->pixels); free(thumbnail->textures);
    memset(thumbnail, 0, sizeof(*thumbnail));
}

BOOL ModelThumbnailUsesImage(const ModelThumbnail *thumbnail, DWORD image)
{
    if (thumbnail->pending || thumbnail->failed) { return TRUE; }
    for (DWORD i = 0; i < thumbnail->texturecount; i++)
        if (thumbnail->textures[i] == image) { return TRUE; }
    return FALSE;
}

static BOOL ThumbnailPath(const char *project, const char *name, char folder[MAX_PATH], char path[MAX_PATH])
{
    char cache[MAX_PATH], leaf[80];
    /* Model names are file-table stems, never paths. */
    if (!name[0] || strlen(name) >= 64 || strpbrk(name, "\\/:*?\"<>|.") != NULL) { return FALSE; }
    if (!EditorPathJoin(cache, sizeof(cache), project, "cache")
        || !EditorPathJoin(folder, MAX_PATH, cache, "model-thumbnails")) { return FALSE; }
    CreateDirectory(cache, NULL); CreateDirectory(folder, NULL);
    snprintf(leaf, sizeof(leaf), "%s.mthumb", name);
    return EditorPathJoin(path, MAX_PATH, folder, leaf);
}

static BOOL ThumbnailRead(const char *path, DWORD revision, DWORD images, unsigned char *pixels)
{
    DWORD header[6]; FILE *file = fopen(path, "rb");
    if (!file) { return FALSE; }
    BOOL ok = fread(header, sizeof(header), 1, file) == 1
        && header[0] == 0x3148544du && header[1] == THUMBNAIL_VERSION
        && header[2] == revision && header[3] == images && header[4] == MODEL_THUMBNAIL_BYTES
        && fread(pixels, MODEL_THUMBNAIL_BYTES, 1, file) == 1 && fgetc(file) == EOF
        && header[5] == ThumbnailHash(2166136261u, pixels, MODEL_THUMBNAIL_BYTES);
    fclose(file); return ok;
}

static void ThumbnailWrite(const char *folder, const char *path, DWORD revision, DWORD images,
    const unsigned char *pixels)
{
    char temporary[MAX_PATH];
    DWORD header[] = {0x3148544du, THUMBNAIL_VERSION, revision, images, MODEL_THUMBNAIL_BYTES,
        ThumbnailHash(2166136261u, pixels, MODEL_THUMBNAIL_BYTES)};
    if (!GetTempFileName(folder, "mth", 0, temporary)) { return; }
    FILE *file = fopen(temporary, "wb");
    BOOL ok = file && fwrite(header, sizeof(header), 1, file) == 1
        && fwrite(pixels, MODEL_THUMBNAIL_BYTES, 1, file) == 1;
    if (file && fclose(file)) { ok = FALSE; }
    if (!ok || !MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        DeleteFile(temporary);
}

void ModelThumbnailUpdate(ModelThumbnail *thumbnail, HWND owner, HWND *renderer,
    const char *project, const char *name)
{
    ModelSource source = {0}; DWORD revision, count = 0, images = 2166136261u;
    const char *why = "";
    BgVertex *vertices = NULL; unsigned short *tags = NULL; BgRenderFlags *flags = NULL;
    TexPixel *texture = NULL; unsigned char *pixels = NULL, *used = NULL;
    char folder[MAX_PATH], path[MAX_PATH]; BOOL ok = FALSE;
    HDC previousdc = wglGetCurrentDC(); HGLRC previous = wglGetCurrentContext();
    thumbnail->pending = FALSE;
    if (!ModelEditsReadSource(project, name, &source, &revision, &why)
        || source.count > INT_MAX / 3) { goto done; }
    used = calloc(BG_TEX_NONE, 1);
    texture = malloc(256 * 256 * sizeof(*texture));
    pixels = malloc(MODEL_THUMBNAIL_BYTES);
    if (!used || !texture || !pixels) { goto done; }
    for (DWORD face = 0; face < source.count; face++) if (ModelSourceFaceInLod(&source, face, MODEL_LOD_HIGH))
    {
        DWORD id = BG_TEX_ID(source.tags[face]);
        if (id < BG_TEX_NONE) { used[id] = 1; }
        count++;
    }
    free(thumbnail->textures); thumbnail->texturecount = 0;
    thumbnail->textures = malloc(BG_TEX_NONE * sizeof(*thumbnail->textures));
    if (!thumbnail->textures) { goto done; }
    for (DWORD id = 0; id < BG_TEX_NONE; id++) if (used[id])
    {
        int dimensions[2] = {0, 0};
        thumbnail->textures[thumbnail->texturecount++] = (unsigned short)id;
        images = ThumbnailHash(images, &id, sizeof(id));
        if (TexLoadProjectImage(project, id, texture, &dimensions[0], &dimensions[1]))
            images = ThumbnailHash(images, texture, (size_t)dimensions[0] * dimensions[1] * sizeof(*texture));
        images = ThumbnailHash(images, dimensions, sizeof(dimensions));
    }
    /* Fingerprint actual native data and decoded images, including unsaved
     * edits. Discarding edits or rebasing therefore cannot reuse a stale image. */
    BOOL cacheable = ThumbnailPath(project, name, folder, path);
    if (cacheable && ThumbnailRead(path, revision, images, pixels)) { ok = TRUE; goto done; }
    vertices = malloc((size_t)max(count, 1) * 3 * sizeof(*vertices));
    tags = malloc((size_t)max(count, 1) * sizeof(*tags));
    flags = malloc((size_t)max(count, 1) * sizeof(*flags));
    if (!vertices || !tags || !flags) { goto done; }
    DWORD at = 0;
    for (DWORD face = 0; face < source.count; face++) if (ModelSourceFaceInLod(&source, face, MODEL_LOD_HIGH))
    {
        for (int k = 0; k < 3; k++)
        {
            const BgVertex *v = &source.vertices[face * 3 + k];
            if (!isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z)) { goto done; }
            vertices[at * 3 + k] = *v;
        }
        tags[at] = source.tags[face]; flags[at++] = source.flags[face];
    }
    if (!*renderer) *renderer = ViewportCreateThumbnail(owner, (HINSTANCE)GetWindowLongPtr(owner, GWLP_HINSTANCE));
    if (!*renderer || !ViewportSetScene(*renderer, vertices, tags, flags,
        NULL, NULL, NULL, 0, NULL, (int)count, project, TRUE)) { goto done; }
    ok = ViewportCaptureThumbnail(*renderer, pixels, MODEL_THUMBNAIL_SIZE);
    if (ok && cacheable) { ThumbnailWrite(folder, path, revision, images, pixels); }
done:
    if (ok) { free(thumbnail->pixels); thumbnail->pixels = pixels; pixels = NULL; }
    thumbnail->failed = !ok;
    free(vertices); free(tags); free(flags); free(texture); free(pixels); free(used);
    ModelFreeSource(&source);
    wglMakeCurrent(previousdc, previous);
}
