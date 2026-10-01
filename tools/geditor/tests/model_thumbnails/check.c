#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
typedef int BOOL;
typedef uint32_t DWORD;
typedef intptr_t HWND, HDC, HGLRC, HINSTANCE;
typedef unsigned int BgRenderFlags;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define GWLP_HINSTANCE 1
#define BG_TEX_NONE 4095
#define BG_TEX_ID(tag) ((tag) & BG_TEX_NONE)
#define MODEL_LOD_HIGH 0
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 2
#define max(a,b) ((a)>(b)?(a):(b))
typedef struct { float x, y, z; } BgVertex;
typedef struct { unsigned char r, g, b, a; } TexPixel;
typedef struct { DWORD count; BgVertex *vertices; unsigned short *tags; BgRenderFlags *flags; } ModelSource;
static int modelrevision = 1, imagecolor = 2, reads, renders, contexts, failread, failrender, badvertex, nowrites;
static HDC currentdc = 11; static HGLRC currentcontext = 12;
static BOOL ModelEditsReadSource(const char *project, const char *name, ModelSource *s, DWORD *revision, const char **why)
{
    reads++;
    if (failread) return FALSE;
    s->count = 2; s->vertices = calloc(6, sizeof(*s->vertices));
    s->tags = calloc(2, sizeof(*s->tags)); s->flags = calloc(2, sizeof(*s->flags));
    assert(s->vertices && s->tags && s->flags);
    s->vertices[0].x = badvertex ? NAN : 3;
    s->tags[0] = !strcmp(name, "PoneZ") ? 7 : 9; s->tags[1] = 50;
    *revision = modelrevision; return TRUE;
}
static BOOL ModelSourceFaceInLod(const ModelSource *s, DWORD face, int lod) { return face == 0; }
static void ModelFreeSource(ModelSource *s)
{ free(s->vertices); free(s->tags); free(s->flags); memset(s, 0, sizeof(*s)); }
static BOOL TexLoadProjectImage(const char *project, DWORD id, TexPixel *pixels, int *w, int *h)
{ *w = *h = 2; for (int i = 0; i < 4; i++) pixels[i] = (TexPixel){imagecolor, 4, 5, 255}; return TRUE; }
static HDC wglGetCurrentDC(void) { return currentdc; }
static HGLRC wglGetCurrentContext(void) { return currentcontext; }
static BOOL wglMakeCurrent(HDC dc, HGLRC rc) { currentdc = dc; currentcontext = rc; contexts++; return TRUE; }
static intptr_t GetWindowLongPtr(HWND hwnd, int which) { return 1; }
static HWND ViewportCreateThumbnail(HWND owner, HINSTANCE instance) { return 42; }
static BOOL ViewportSetScene(HWND hwnd, const BgVertex *vertices, const unsigned short *tags,
    const BgRenderFlags *flags, const void *a, const void *b, const void *c, int first,
    const void *d, int count, const char *project, BOOL frame)
{
    assert(hwnd == 42 && count == 1 && vertices[0].x == 3 && frame);
    currentdc = 21; currentcontext = 22; return TRUE;
}
static BOOL ViewportCaptureThumbnail(HWND hwnd, unsigned char *pixels, int size)
{
    assert(hwnd == 42 && size == 64); renders++;
    memset(pixels, modelrevision + imagecolor, (size_t)size * size * 4); return !failrender;
}
static BOOL EditorPathJoin(char *out, size_t size, const char *folder, const char *leaf)
{ int n = snprintf(out, size, "%s/%s", folder, leaf); return n >= 0 && (size_t)n < size; }
static BOOL CreateDirectory(const char *path, void *security) { return !mkdir(path, 0700); }
static BOOL GetTempFileName(const char *folder, const char *prefix, int id, char *out)
{
    if (nowrites) return FALSE;
    if (strlen(folder) + 12 > MAX_PATH) return FALSE;
    strcpy(out, folder); strcat(out, "/tempXXXXXX");
    int file = mkstemp(out); if (file < 0) return FALSE;
    close(file); return TRUE;
}
static BOOL MoveFileEx(const char *from, const char *to, int flags) { return !rename(from, to); }
static BOOL DeleteFile(const char *path) { return !unlink(path); }
#include "thumbnail.inc"

#define BROWSER_MODEL_TIMER 100
static int timers;
typedef struct { ModelThumbnail *modelthumbnails; int modelcount; struct { char label[64]; } models[2]; DWORD modelrefreshafter; } BrowserState;
static BrowserState browser;
static BrowserState *BrowserGetState(HWND hwnd) { return &browser; }
static int lstrcmpi(const char *a, const char *b) { return strcasecmp(a,b); }
static DWORD GetTickCount(void) { return 1000; }
static void SetTimer(HWND hwnd, int id, int delay, void *callback) { assert(id == BROWSER_MODEL_TIMER); timers++; }
#include "refresh.inc"

static void Update(ModelThumbnail *t, HWND *renderer, const char *name)
{
    t->pending = TRUE;
    ModelThumbnailUpdate(t, 1, renderer, ".", name);
    assert(!t->pending && currentdc == 11 && currentcontext == 12);
}
int main(void)
{
    ModelThumbnail thumbs[2] = {{0}}; HWND renderer = 0;
    Update(&thumbs[0], &renderer, "PoneZ");
    assert(thumbs[0].pixels && !thumbs[0].failed && renders == 1);
    assert(ModelThumbnailUsesImage(&thumbs[0], 7) && !ModelThumbnailUsesImage(&thumbs[0], 9));
    assert(!ModelThumbnailUsesImage(&thumbs[0], 50)); /* Hidden low-LOD texture. */
    ModelThumbnailFree(&thumbs[0]);
    Update(&thumbs[0], &renderer, "PoneZ");
    assert(renders == 1); /* Reopen reads the saved cache, without rendering. */
    modelrevision++;
    Update(&thumbs[0], &renderer, "PoneZ"); assert(renders == 2);
    modelrevision--; /* Undo/discard must not retain the edited preview. */
    Update(&thumbs[0], &renderer, "PoneZ"); assert(renders == 3);
    imagecolor++;
    Update(&thumbs[0], &renderer, "PoneZ"); assert(renders == 4);
    FILE *file = fopen("cache/model-thumbnails/PoneZ.mthumb", "r+b"); assert(file);
    assert(!fseek(file, 60, SEEK_SET)); fputc(255, file); fclose(file);
    Update(&thumbs[0], &renderer, "PoneZ"); assert(renders == 5); /* Corrupt payload. */
    Update(&thumbs[1], &renderer, "PtwoZ"); assert(renders == 6);
    browser.modelthumbnails = thumbs; browser.modelcount = 2;
    strcpy(browser.models[0].label, "PoneZ"); strcpy(browser.models[1].label, "PtwoZ");
    BrowserRefreshModelThumbnail(1, "ponez");
    assert(thumbs[0].pending && !thumbs[1].pending && browser.modelrefreshafter == 1150);
    thumbs[0].pending = FALSE;
    BrowserRefreshModelImage(1, 9);
    assert(!thumbs[0].pending && thumbs[1].pending);
    thumbs[1].pending = FALSE;
    BrowserRefreshModelThumbnail(1, "missing"); assert(!thumbs[0].pending && !thumbs[1].pending);
    unsigned char old = thumbs[0].pixels[0];
    failrender = TRUE; modelrevision++;
    Update(&thumbs[0], &renderer, "PoneZ");
    assert(thumbs[0].failed && thumbs[0].pixels[0] == old && renders == 7);
    failrender = FALSE; nowrites = TRUE;
    Update(&thumbs[0], &renderer, "PoneZ"); assert(!thumbs[0].failed && renders == 8);
    ModelThumbnailFree(&thumbs[0]); failread = TRUE;
    Update(&thumbs[0], &renderer, "PoneZ"); assert(thumbs[0].failed && !thumbs[0].pixels);
    failread = FALSE; badvertex = TRUE;
    Update(&thumbs[0], &renderer, "PoneZ"); assert(thumbs[0].failed && renders == 8);
    ModelThumbnailFree(&thumbs[0]); ModelThumbnailFree(&thumbs[1]);
    assert(contexts == reads && timers == 2);
    puts("PASS: cache reuse, edit/undo/image invalidation, checksum recovery, high-LOD dependencies, targeted refresh, read-only cache, failed capture, invalid geometry, GL context restoration and cleanup.");
}
