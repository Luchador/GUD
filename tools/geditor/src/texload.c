/*
 * GUTX record parsing and N64 texel decoding.
 *
 * Record layout (from tools/mktex/src/tex2raw.c, the writer):
 *   +0  "GUTX"
 *   +4  has_explicit_lods   +5 lod_count   +6 image_count
 *   +8  numcolours (BE16)   +10 header_size (BE16)
 *   +12 record_size (BE32)
 *   +16 descriptors[7], 12 bytes each:
 *         +0 format  +1 width  +2 height
 *         +4 data_offset (BE32, from record start)  +8 data_size (BE32)
 *   +100 palette, numcolours entries of 2 bytes
 * Pixel rows are padded to the RDP's per-format alignment.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "texload.h"

#define GUTX_DESC_OFFSET   16
#define GUTX_DESC_SIZE     12
#define GUTX_PALETTE_OFFSET (GUTX_DESC_OFFSET + 7 * GUTX_DESC_SIZE)

enum {
    PDFORMAT_RGBA32 = 0,
    PDFORMAT_RGBA16,
    PDFORMAT_RGB24,
    PDFORMAT_RGB15,
    PDFORMAT_IA16,
    PDFORMAT_IA8,
    PDFORMAT_IA4,
    PDFORMAT_I8,
    PDFORMAT_I4,
    PDFORMAT_RGBA16_CI8,
    PDFORMAT_RGBA16_CI4,
    PDFORMAT_IA16_CI8,
    PDFORMAT_IA4_CI4
};

typedef struct TexPixel { unsigned char r, g, b, a; } TexPixel;

static DWORD texbe16(const unsigned char *p)
{
    return ((DWORD)p[0] << 8) | p[1];
}

static DWORD texbe32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static DWORD texalign(DWORD v, DWORD a)
{
    return (v + a - 1) & ~(a - 1);
}

/* Bytes per stored pixel row, per format - mirrors output_row_size in
   the writer. */
static DWORD TexRowStride(int format, DWORD width)
{
    switch (format)
    {
    case PDFORMAT_RGBA32:
    case PDFORMAT_RGB24:
        return texalign(width, 4) * 4;
    case PDFORMAT_RGBA16:
    case PDFORMAT_RGB15:
    case PDFORMAT_IA16:
        return texalign(width, 4) * 2;
    case PDFORMAT_IA8:
    case PDFORMAT_I8:
    case PDFORMAT_RGBA16_CI8:
    case PDFORMAT_IA16_CI8:
        return texalign(width, 8);
    case PDFORMAT_IA4:
    case PDFORMAT_I4:
    case PDFORMAT_RGBA16_CI4:
    case PDFORMAT_IA4_CI4:
        return texalign(width, 16) / 2;
    }

    return 0;
}

static TexPixel TexFromRgba16(DWORD t)
{
    TexPixel p;

    p.r = (unsigned char)(((t >> 11) & 31) << 3);
    p.g = (unsigned char)(((t >> 6) & 31) << 3);
    p.b = (unsigned char)(((t >> 1) & 31) << 3);
    p.a = (unsigned char)((t & 1) ? 255 : 0);

    return p;
}

static TexPixel TexFromIa16(DWORD t)
{
    TexPixel p;

    p.r = p.g = p.b = (unsigned char)(t >> 8);
    p.a = (unsigned char)(t & 0xFF);

    return p;
}

/*
 * Decodes one image of one record into caller-provided RGBA storage
 * (width*height TexPixels). Returns FALSE if anything is out of
 * bounds or the format is unknown.
 */
static BOOL TexDecodeImage(const unsigned char *rec, DWORD recsize,
                           int format, DWORD width, DWORD height,
                           DWORD dataoff, DWORD ncolours, TexPixel *out)
{
    DWORD stride = TexRowStride(format, width);
    const unsigned char *pal = rec + GUTX_PALETTE_OFFSET;
    DWORD x;
    DWORD y;

    if (stride == 0 || dataoff + stride * height > recsize)
    {
        return FALSE;
    }

    for (y = 0; y < height; y++)
    {
        const unsigned char *row = rec + dataoff + y * stride;

        for (x = 0; x < width; x++)
        {
            TexPixel p = { 0, 0, 0, 255 };
            DWORD b;
            DWORD ci;

            switch (format)
            {
            case PDFORMAT_RGBA32:
                p.r = row[x * 4 + 0];
                p.g = row[x * 4 + 1];
                p.b = row[x * 4 + 2];
                p.a = row[x * 4 + 3];
                break;

            case PDFORMAT_RGB24:
                /* stored as RGBA with a marker alpha - it has none */
                p.r = row[x * 4 + 0];
                p.g = row[x * 4 + 1];
                p.b = row[x * 4 + 2];
                p.a = 255;
                break;

            case PDFORMAT_RGBA16:
                p = TexFromRgba16(texbe16(row + x * 2));
                break;

            case PDFORMAT_RGB15:
                p = TexFromRgba16(texbe16(row + x * 2));
                p.a = 255; /* the low bit is not alpha in this format */
                break;

            case PDFORMAT_IA16:
                p = TexFromIa16(texbe16(row + x * 2));
                break;

            case PDFORMAT_IA8:
                b = row[x];
                p.r = p.g = p.b = (unsigned char)((b >> 4) * 17);
                p.a = (unsigned char)((b & 0xF) * 17);
                break;

            case PDFORMAT_IA4:
                b = row[x >> 1];
                b = (x & 1) ? (b & 0xF) : (b >> 4);
                p.r = p.g = p.b = (unsigned char)(((b >> 1) * 255) / 7);
                p.a = (unsigned char)((b & 1) ? 255 : 0);
                break;

            case PDFORMAT_I8:
                p.r = p.g = p.b = row[x];
                p.a = 255; /* the RDP uses I as alpha; opaque reads better on disk */
                break;

            case PDFORMAT_I4:
                b = row[x >> 1];
                b = (x & 1) ? (b & 0xF) : (b >> 4);
                p.r = p.g = p.b = (unsigned char)(b * 17);
                p.a = 255;
                break;

            case PDFORMAT_RGBA16_CI8:
            case PDFORMAT_IA16_CI8:
                ci = row[x];
                if (ci >= ncolours) { return FALSE; }
                p = (format == PDFORMAT_RGBA16_CI8)
                    ? TexFromRgba16(texbe16(pal + ci * 2))
                    : TexFromIa16(texbe16(pal + ci * 2));
                break;

            case PDFORMAT_RGBA16_CI4:
            case PDFORMAT_IA4_CI4:
                b = row[x >> 1];
                ci = (x & 1) ? (b & 0xF) : (b >> 4);
                if (ci >= ncolours) { return FALSE; }
                p = (format == PDFORMAT_RGBA16_CI4)
                    ? TexFromRgba16(texbe16(pal + ci * 2))
                    : TexFromIa16(texbe16(pal + ci * 2));
                break;

            default:
                return FALSE;
            }

            out[y * width + x] = p;
        }
    }

    return TRUE;
}

/* Writes a bottom-up 32-bit BMP. */
static BOOL TexWriteBmp(const char *path, const TexPixel *pixels,
                        DWORD width, DWORD height)
{
    BITMAPFILEHEADER bfh;
    BITMAPINFOHEADER bih;
    HANDLE f;
    DWORD written;
    DWORD y;
    BOOL ok = TRUE;

    ZeroMemory(&bfh, sizeof(bfh));
    ZeroMemory(&bih, sizeof(bih));

    bfh.bfType = 0x4D42; /* "BM" */
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
    bfh.bfSize = bfh.bfOffBits + width * height * 4;

    bih.biSize = sizeof(bih);
    bih.biWidth = (LONG)width;
    bih.biHeight = (LONG)height; /* positive: bottom-up */
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;

    f = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                   FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    ok = ok && WriteFile(f, &bfh, sizeof(bfh), &written, NULL);
    ok = ok && WriteFile(f, &bih, sizeof(bih), &written, NULL);

    /*
     * GE's texel data is stored rotated 180 degrees from viewing
     * orientation - the game's UV mapping compensates in-engine, so
     * the decoder keeps the native order (the future GL path wants
     * it). Only here, at the disk boundary, do we rotate into human
     * orientation: bottom-up BMP encoding plus forward row order
     * flips vertically, and reversing x flips horizontally.
     */
    for (y = 0; ok && y < height; y++)
    {
        const TexPixel *row = pixels + y * width;
        DWORD x;
        unsigned char line[256 * 4];

        for (x = 0; x < width; x++)
        {
            const TexPixel *p = &row[width - 1 - x];

            line[x * 4 + 0] = p->b;
            line[x * 4 + 1] = p->g;
            line[x * 4 + 2] = p->r;
            line[x * 4 + 3] = p->a;
        }

        ok = WriteFile(f, line, width * 4, &written, NULL);
    }

    CloseHandle(f);
    return ok;
}

DWORD TexExtractImages(const RomFile *rom, const char *projectdir,
                       const char **reasonout)
{
    const RomManifestEntry *imgs = NULL;
    char dir[MAX_PATH];
    char path[MAX_PATH];
    DWORD pos;
    DWORD id = 0;
    DWORD writtencount = 0;
    DWORD i;
    TexPixel *pixels;

    *reasonout = "";

    for (i = 0; i < rom->info.entrycount; i++)
    {
        if (rom->info.entries[i].kind == 0x494D4753) /* 'IMGS' */
        {
            imgs = &rom->info.entries[i];
        }
    }

    if (imgs == NULL)
    {
        *reasonout = "the ROM has no IMGS entry.";
        return 0;
    }

    wsprintf(dir, "%s\\images", projectdir);
    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the images folder could not be created.";
        return 0;
    }

    /* Largest GUTX image is 255x255; one reusable buffer serves all. */
    pixels = (TexPixel *)malloc(256 * 256 * sizeof(TexPixel));
    if (pixels == NULL)
    {
        *reasonout = "out of memory decoding textures.";
        return 0;
    }

    pos = imgs->romstart;

    while (pos + GUTX_PALETTE_OFFSET <= imgs->romend
           && memcmp(rom->data + pos, "GUTX", 4) == 0)
    {
        const unsigned char *rec = rom->data + pos;
        DWORD recsize = texbe32(rec + 12);
        DWORD ncolours = texbe16(rec + 8);
        int format = rec[GUTX_DESC_OFFSET + 0];
        DWORD width = rec[GUTX_DESC_OFFSET + 1];
        DWORD height = rec[GUTX_DESC_OFFSET + 2];
        DWORD dataoff = texbe32(rec + GUTX_DESC_OFFSET + 4);

        if (recsize == 0 || (recsize & 0xF) || pos + recsize > imgs->romend)
        {
            break; /* the walk has left the rails; keep what we have */
        }

        if (width > 0 && height > 0
            && TexDecodeImage(rec, recsize, format, width, height,
                              dataoff, ncolours, pixels))
        {
            wsprintf(path, "%s\\%04X.bmp", dir, id);

            if (TexWriteBmp(path, pixels, width, height))
            {
                writtencount++;
            }
        }

        id++;
        pos += recsize;
    }

    free(pixels);

    if (writtencount == 0)
    {
        *reasonout = "no textures could be extracted.";
    }

    return writtencount;
}
