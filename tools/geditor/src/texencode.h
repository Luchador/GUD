#ifndef GEDITOR_TEXENCODE_H
#define GEDITOR_TEXENCODE_H
#include "texload.h"

typedef struct TexImportOptions {
    unsigned char format, mipmaps, hitsound, hittexture;
} TexImportOptions;

DWORD TexImportRowBytes(int format, int width);
DWORD TexImportTmemBytes(int width, int height, int format, int mipmaps);
DWORD TexImportTmemLimit(int format);
int TexImportMaxMipmaps(int width, int height);
BOOL TexEncodeRecord(const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, unsigned char **data, DWORD *size, const char **reasonout);

#endif
