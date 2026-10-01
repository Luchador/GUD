#ifndef GEDITOR_MODELTHUMBNAIL_H
#define GEDITOR_MODELTHUMBNAIL_H
#include <windows.h>

#define MODEL_THUMBNAIL_SIZE 64
#define MODEL_THUMBNAIL_BYTES (MODEL_THUMBNAIL_SIZE * MODEL_THUMBNAIL_SIZE * 4)

typedef struct ModelThumbnail {
    unsigned char *pixels; /* Owned, top-down BGRA. Last good image survives a queued refresh. */
    unsigned short *textures;
    DWORD texturecount;
    BOOL pending, failed;
} ModelThumbnail;

void ModelThumbnailFree(ModelThumbnail *thumbnail);
BOOL ModelThumbnailUsesImage(const ModelThumbnail *thumbnail, DWORD image);
/* One job, called between UI messages. The private renderer is created lazily
 * on a cache miss and reused. Owner destroys it when the project changes. */
void ModelThumbnailUpdate(ModelThumbnail *thumbnail, HWND owner, HWND *renderer,
    const char *project, const char *name);
#endif
