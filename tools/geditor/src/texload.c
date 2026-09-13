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

#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "texload.h"
#include "imageedits.h"

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

static DWORD texbe16(const unsigned char *p)
{
    return ((DWORD)p[0] << 8) | p[1];
}

static DWORD texbe32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static DWORD texle16(const unsigned char *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8);
}

static DWORD texle32(const unsigned char *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8)
         | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
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

BOOL TexDecodeRecord(const unsigned char *data, DWORD size, TexPixel *pixels, int *width, int *height)
{
    TexInfoRecord info;
    if (!TexInfoReadRecord(data, size, &info)) { return FALSE; }
    *width = data[17]; *height = data[18];
    return TexDecodeImage(data, info.size, data[16], data[17], data[18],
                          texbe32(data + 20), texbe16(data + 8), pixels);
}

/* WIC ignores the fourth byte in legacy 32-bit BMPs, including Photoshop's
 * A8R8G8B8 output and our extracted project BMPs. Recover it before rotating
 * the decoded pixels into native GE order. WIC still owns RGB conversion and
 * all other BMP formats (indexed, 16/24-bit, RLE, etc.).
 *
 * A legacy BI_RGB header cannot distinguish all-transparent alpha from unused
 * zero padding. Keep WIC's opaque result if every fourth byte is zero; otherwise
 * retain the full channel. An explicit alpha mask is authoritative even when
 * every alpha value is zero. */
static BOOL TexRestoreImportBmpAlpha(const char *path, TexPixel *pixels,
                                      DWORD width, DWORD height)
{
    unsigned char header[138], *raw = NULL;
    HANDLE file;
    DWORD got, filesize, headersize, compression, offset, bytes;
    DWORD mask = 0, shift = 0, maximum, x, y, headerend;
    LONG rawheight;
    BOOL legacy = FALSE, ok = FALSE;

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) { return FALSE; }
    filesize = GetFileSize(file, NULL);
    if (filesize == INVALID_FILE_SIZE
        || !ReadFile(file, header, sizeof(header), &got, NULL)) { goto done; }
    /* Unknown headers remain the system decoder's responsibility. */
    if (got < 18 || header[0] != 'B' || header[1] != 'M') { ok = TRUE; goto done; }
    headersize = texle32(header + 14);
    if (headersize != 40 && headersize != 52 && headersize != 56
        && headersize != 108 && headersize != 124) { ok = TRUE; goto done; }
    if (got < 14 + headersize) { goto done; }
    compression = texle32(header + 30);
    if (texle16(header + 28) != 32
        || (compression != BI_RGB && compression != 3 && compression != 6))
    { ok = TRUE; goto done; }

    headerend = 14 + headersize;
    if (headersize >= 56)
    {
        mask = texle32(header + 66); /* V3/V4/V5 explicit alpha mask. */
    }
    else if (headersize == 40 && compression == 6) /* BI_ALPHABITFIELDS */
    {
        headerend += 16; /* Four masks follow BITMAPINFOHEADER. */
        if (got < headerend) { goto done; }
        mask = texle32(header + 66);
    }
    if (!mask && headersize == 40 && compression == BI_RGB)
    { mask = 0xFF000000u; legacy = TRUE; }
    if (!mask) { ok = TRUE; goto done; }

    /* Never treat colour bits as alpha. Explicit masks may use fewer than
       eight bits, but must be contiguous and separate from the RGB channels. */
    if (!legacy)
    {
        DWORD colourmask = compression == BI_RGB ? 0x00FFFFFFu
            : texle32(header + 54) | texle32(header + 58) | texle32(header + 62);
        if (mask & colourmask) { goto done; }
    }
    while (!((mask >> shift) & 1u)) { shift++; }
    maximum = mask >> shift;
    if (maximum & (maximum + 1u)) { goto done; }
    rawheight = (LONG)texle32(header + 22);
    if (!pixels || !width || !height || width > 255 || height > 255
        || texle16(header + 26) != 1 || texle32(header + 18) != width
        || (rawheight != (LONG)height && rawheight != -(LONG)height)) { goto done; }
    offset = texle32(header + 10);
    bytes = width * height * 4; /* Every 32-bit row is already DWORD aligned. */
    if (offset < headerend || offset > 0x7FFFFFFFu || offset > filesize
        || bytes > filesize - offset
        || SetFilePointer(file, (LONG)offset, NULL, FILE_BEGIN) != offset) { goto done; }
    raw = (unsigned char *)malloc(bytes);
    if (!raw || !ReadFile(file, raw, bytes, &got, NULL) || got != bytes) { goto done; }
    if (legacy)
    {
        DWORD i;
        for (i = 3; i < bytes && raw[i] == 0; i += 4) { }
        if (i >= bytes) { ok = TRUE; goto done; }
    }
    for (y = 0; y < height; y++)
    {
        DWORD sourcey = rawheight > 0 ? height - 1 - y : y;
        const unsigned char *row = raw + sourcey * width * 4;
        for (x = 0; x < width; x++)
        {
            DWORD alpha = (texle32(row + x * 4) & mask) >> shift;
            pixels[y * width + x].a = (unsigned char)
                (((ULONGLONG)alpha * 255 + maximum / 2) / maximum);
        }
    }
    ok = TRUE;
done:
    free(raw);
    CloseHandle(file);
    return ok;
}

/* WIC handles indexed, RGB, bitfield and RLE BMPs. Restore legacy/explicit
 * BMP alpha that its decoder ignores. Rotate once at the import boundary
 * to match the native orientation used by existing project textures/UVs. */
BOOL TexReadImportBmp(const char *path, TexPixel **pixels, int *width, int *height, const char **reasonout)
{
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory *factory = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *converter = NULL;
    WCHAR wide[MAX_PATH];
    GUID container;
    UINT w, h, i;
    TexPixel *result = NULL;
    BOOL ok = FALSE;
    *pixels = NULL; *width = *height = 0;
    *reasonout = "The selected file could not be decoded as a BMP image.";
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) { return FALSE; }
    if (!MultiByteToWideChar(CP_ACP, 0, path, -1, wide, MAX_PATH)
        || FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                    &IID_IWICImagingFactory, (void **)&factory))
        || FAILED(IWICImagingFactory_CreateDecoderFromFilename(factory, wide, NULL,
                    GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder))
        || FAILED(IWICBitmapDecoder_GetContainerFormat(decoder, &container))
        || !IsEqualGUID(&container, &GUID_ContainerFormatBmp)
        || FAILED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame))
        || FAILED(IWICBitmapFrameDecode_GetSize(frame, &w, &h))) { goto done; }
    if (w == 0 || h == 0 || w > 255 || h > 255)
    {
        *reasonout = "GUD image dimensions must be between 1 and 255 pixels on each axis. Resize this BMP before importing.";
        goto done;
    }
    result = (TexPixel *)malloc(w * h * sizeof(*result));
    if (result == NULL) { *reasonout = "Out of memory reading the BMP."; goto done; }
    if (FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter))
        || FAILED(IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)frame,
            &GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom))
        || FAILED(IWICFormatConverter_CopyPixels(converter, NULL, w * 4, w * h * 4, (BYTE *)result))) { goto done; }
    if (!TexRestoreImportBmpAlpha(path, result, w, h))
    {
        *reasonout = "The BMP alpha data could not be read. Check that the file is complete and has a valid header.";
        goto done;
    }
    for (i = 0; i < w * h / 2; i++)
    {
        TexPixel temp = result[i]; result[i] = result[w * h - 1 - i]; result[w * h - 1 - i] = temp;
    }
    *pixels = result; result = NULL; *width = w; *height = h; *reasonout = ""; ok = TRUE;
done:
    free(result);
    if (converter) { IWICFormatConverter_Release(converter); }
    if (frame) { IWICBitmapFrameDecode_Release(frame); }
    if (decoder) { IWICBitmapDecoder_Release(decoder); }
    if (factory) { IWICImagingFactory_Release(factory); }
    if (SUCCEEDED(initialized)) { CoUninitialize(); }
    return ok;
}

/* Writes a bottom-up 32-bit BMP. */
BOOL TexWriteBmp(const char *path, const TexPixel *pixels,
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

    ok = ok && WriteFile(f, &bfh, sizeof(bfh), &written, NULL) && written == sizeof(bfh);
    ok = ok && WriteFile(f, &bih, sizeof(bih), &written, NULL) && written == sizeof(bih);

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

        ok = WriteFile(f, line, width * 4, &written, NULL) && written == width * 4;
    }

    CloseHandle(f);
    return ok;
}

DWORD TexExtractImages(const RomFile *rom, const char *projectdir, const char **reasonout)
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


/*
 * Reads one of our own extracted BMPs (32-bit, bottom-up, BI_RGB) and
 * scales it to a thumbnail with nearest-neighbour sampling, writing
 * top-down RGBA into dst. Files that are not ours are skipped.
 */
static BOOL TexReadBmpThumb(const char *path, unsigned char *dst,
                            TexThumb *thumb)
{
    HANDLE f;
    DWORD got = 0;
    unsigned char header[54];
    unsigned char *pix = NULL;
    LONG w;
    LONG h;
    DWORD size;
    int x;
    int y;
    BOOL ok = FALSE;

    f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                   OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE)
    {
        return FALSE;
    }

    if (ReadFile(f, header, sizeof(header), &got, NULL) && got == sizeof(header)
        && header[0] == 'B' && header[1] == 'M'
        && header[28] == 32 /* biBitCount */)
    {
        w = (LONG)(header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24));
        h = (LONG)(header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24));

        if (w > 0 && h > 0 && w <= 256 && h <= 256)
        {
            size = (DWORD)(w * h * 4);
            pix = (unsigned char *)malloc(size);

            if (pix != NULL && ReadFile(f, pix, size, &got, NULL) && got == size)
            {
                /* fit inside TEX_THUMB_MAX, preserving aspect */
                int longest = w > h ? w : h;
                int sw = longest > TEX_THUMB_MAX ? (int)(w * TEX_THUMB_MAX / longest) : (int)w;
                int sh = longest > TEX_THUMB_MAX ? (int)(h * TEX_THUMB_MAX / longest) : (int)h;

                if (sw < 1) { sw = 1; }
                if (sh < 1) { sh = 1; }

                for (y = 0; y < sh; y++)
                {
                    /* BMP rows are bottom-up; thumbs are top-down */
                    int sy = (int)((LONG)y * h / sh);
                    const unsigned char *srow = pix + (h - 1 - sy) * w * 4;
                    unsigned char *drow = dst + (y * TEX_THUMB_MAX) * 4;

                    for (x = 0; x < sw; x++)
                    {
                        const unsigned char *s = srow + (LONG)x * w / sw * 4;

                        /* Keep BGRA byte order: the BMP already
                           stores it, and StretchDIBits expects it.
                           The only transform here is the row flip
                           to top-down. */
                        drow[x * 4 + 0] = s[0];
                        drow[x * 4 + 1] = s[1];
                        drow[x * 4 + 2] = s[2];
                        drow[x * 4 + 3] = s[3];
                    }
                }

                thumb->w = sw;
                thumb->h = sh;
                thumb->imagewidth = (int)w;
                thumb->imageheight = (int)h;
                ok = TRUE;
            }
        }
    }

    free(pix);
    CloseHandle(f);
    return ok;
}

/* Decode a built-in browser image using the same fixed-stride BGRA layout
 * as project thumbnails. The executable owns the PNG; no project copy is
 * needed, including when the Images folder is empty. */
BOOL TexLoadResourceThumbnail(HINSTANCE instance, int resourceid,
                              TexThumb *thumb, unsigned char *pixels)
{
    HRESULT initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    IWICImagingFactory *factory = NULL;
    IWICStream *stream = NULL;
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICBitmapScaler *scaler = NULL;
    IWICFormatConverter *converter = NULL;
    HRSRC resource;
    HGLOBAL data;
    BYTE *bytes;
    DWORD size;
    UINT width, height, longest;
    BOOL success = FALSE;

    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) { return FALSE; }
    resource = FindResource(instance, MAKEINTRESOURCE(resourceid), RT_RCDATA);
    if (resource == NULL) { goto done; }
    data = LoadResource(instance, resource);
    bytes = (BYTE *)LockResource(data);
    size = SizeofResource(instance, resource);
    if (bytes == NULL || size == 0) { goto done; }
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL,
            CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory))
        || FAILED(IWICImagingFactory_CreateStream(factory, &stream))
        || FAILED(IWICStream_InitializeFromMemory(stream, bytes, size))
        || FAILED(IWICImagingFactory_CreateDecoderFromStream(factory,
            (IStream *)stream, NULL, WICDecodeMetadataCacheOnLoad, &decoder))
        || FAILED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame))
        || FAILED(IWICBitmapFrameDecode_GetSize(frame, &width, &height))
        || width == 0 || height == 0) { goto done; }

    longest = width > height ? width : height;
    if (longest > TEX_THUMB_MAX)
    {
        width = (UINT)((ULONGLONG)width * TEX_THUMB_MAX / longest);
        height = (UINT)((ULONGLONG)height * TEX_THUMB_MAX / longest);
        if (width == 0) { width = 1; }
        if (height == 0) { height = 1; }
    }
    if (FAILED(IWICImagingFactory_CreateBitmapScaler(factory, &scaler))
        || FAILED(IWICBitmapScaler_Initialize(scaler, (IWICBitmapSource *)frame,
            width, height, WICBitmapInterpolationModeNearestNeighbor))
        || FAILED(IWICImagingFactory_CreateFormatConverter(factory, &converter))
        || FAILED(IWICFormatConverter_Initialize(converter, (IWICBitmapSource *)scaler,
            &GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
            NULL, 0.0, WICBitmapPaletteTypeCustom))) { goto done; }

    ZeroMemory(pixels, TEX_THUMB_MAX * TEX_THUMB_MAX * 4);
    success = SUCCEEDED(IWICFormatConverter_CopyPixels(converter, NULL,
        TEX_THUMB_MAX * 4, TEX_THUMB_MAX * TEX_THUMB_MAX * 4, pixels));
    if (success) { thumb->w = (int)width; thumb->h = (int)height; thumb->pixeloffset = 0; }

done:
    if (converter != NULL) { IWICFormatConverter_Release(converter); }
    if (scaler != NULL) { IWICBitmapScaler_Release(scaler); }
    if (frame != NULL) { IWICBitmapFrameDecode_Release(frame); }
    if (decoder != NULL) { IWICBitmapDecoder_Release(decoder); }
    if (stream != NULL) { IWICStream_Release(stream); }
    if (factory != NULL) { IWICImagingFactory_Release(factory); }
    if (SUCCEEDED(initialized)) { CoUninitialize(); }
    return success;
}

static int TexThumbCompare(const void *a, const void *b)
{
    return strcmp(((const TexThumb *)a)->label, ((const TexThumb *)b)->label);
}

DWORD TexLoadProjectThumbnails(const char *projectdir, TexThumb **items,
                               unsigned char **pixelblock,
                               const char **reasonout)
{
    char pattern[MAX_PATH];
    char path[MAX_PATH];
    WIN32_FIND_DATA find;
    HANDLE search;
    TexThumb *list = NULL;
    unsigned char *pixels = NULL;
    DWORD capacity = 0;
    DWORD count = 0;
    DWORD thumbbytes = TEX_THUMB_MAX * TEX_THUMB_MAX * 4;

    *items = NULL;
    *pixelblock = NULL;
    *reasonout = "";

    wsprintf(pattern, "%s\\images\\*.bmp", projectdir);

    search = FindFirstFile(pattern, &find);
    if (search == INVALID_HANDLE_VALUE)
    {
        goto thumbnails;
    }

    do
    {
        TexThumb *item;
        char *dot;

        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            continue;
        }

        if (count == capacity)
        {
            DWORD next = capacity ? capacity * 2 : 512;
            TexThumb *growni = (TexThumb *)realloc(list, next * sizeof(TexThumb));
            unsigned char *grownp = (unsigned char *)realloc(pixels, next * thumbbytes);

            if (growni != NULL) { list = growni; }
            if (grownp != NULL) { pixels = grownp; }
            if (growni == NULL || grownp == NULL)
            {
                break; /* keep what we have */
            }

            capacity = next;
        }

        item = &list[count];
        ZeroMemory(item, sizeof(*item));
        ZeroMemory(pixels + count * thumbbytes, thumbbytes);

        lstrcpyn(item->label, find.cFileName, sizeof(item->label));
        dot = strrchr(item->label, '.');
        if (dot != NULL)
        {
            *dot = '\0';
        }

        wsprintf(path, "%s\\images\\%s", projectdir, find.cFileName);

        if (TexReadBmpThumb(path, pixels + count * thumbbytes, item))
        {
            item->pixeloffset = count * thumbbytes;
            count++;
        }
    }
    while (FindNextFile(search, &find));

    FindClose(search);

thumbnails:
    /* Cache ROM metadata once on project load, never during mouse movement.
     * The BMP remains the source for the displayed image and its dimensions. */
    {
        RomFile rom = {0};
        const char *why = "";
        int length = snprintf(path, sizeof(path), "%s\\base.z64", projectdir);
        if (length > 0 && length < (int)sizeof(path) && RomLoad(path, &rom, &why))
        {
            TexSetRomThumbnailInfo(&rom, list, count);
            RomFree(&rom);
        }
    }

    ImageEditsUpdateThumbnails(projectdir, &list, &pixels, &count);
    if (count == 0)
    {
        free(list); free(pixels);
        *reasonout = "the images folder holds no readable BMPs.";
        return 0;
    }
    qsort(list, count, sizeof(TexThumb), TexThumbCompare);
    *items = list;
    *pixelblock = pixels;
    return count;
}


BOOL TexLoadProjectImage(const char *projectdir, DWORD id, TexPixel *out, int *w, int *h)
{
    if (ImageEditsGetPixels(projectdir, id, out, w, h)) { return TRUE; }
    return TexLoadSavedProjectImage(projectdir, id, out, w, h);
}

BOOL TexLoadSavedProjectImage(const char *projectdir, DWORD id,
                         TexPixel *out, int *w, int *h)
{
    char path[MAX_PATH];
    unsigned char header[54];
    unsigned char *pixels = NULL;
    HANDLE file;
    DWORD got;
    DWORD filesize;
    DWORD pixeloffset;
    DWORD pixelsize;
    LONG width;
    LONG rawheight;
    int height;
    int x;
    int y;
    BOOL ok = FALSE;
    int written;

    *w = 0;
    *h = 0;

    written = snprintf(path, sizeof(path), "%s\\images\\%04lX.bmp",
                       projectdir, (unsigned long)id);
    if (written < 0 || written >= (int)sizeof(path))
    {
        return FALSE;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return ImageEditsGetDeletedPixels(projectdir, id, out, w, h);
    }

    filesize = GetFileSize(file, NULL);

    if (filesize == INVALID_FILE_SIZE
        || !ReadFile(file, header, sizeof(header), &got, NULL)
        || got != sizeof(header)
        || header[0] != 'B' || header[1] != 'M'
        || texle32(header + 14) < 40
        || texle16(header + 26) != 1
        || texle16(header + 28) != 32
        || texle32(header + 30) != BI_RGB)
    {
        CloseHandle(file);
        return FALSE;
    }

    width = (LONG)texle32(header + 18);
    rawheight = (LONG)texle32(header + 22);
    pixeloffset = texle32(header + 10);

    if (width <= 0 || width > 256
        || rawheight == 0 || rawheight < -256 || rawheight > 256)
    {
        CloseHandle(file);
        return FALSE;
    }

    height = rawheight < 0 ? (int)-rawheight : (int)rawheight;
    pixelsize = (DWORD)width * (DWORD)height * 4;

    if (pixeloffset < sizeof(header) || pixeloffset > filesize
        || pixelsize > filesize - pixeloffset
        || SetFilePointer(file, (LONG)pixeloffset, NULL, FILE_BEGIN)
            != pixeloffset)
    {
        CloseHandle(file);
        return FALSE;
    }

    pixels = (unsigned char *)malloc(pixelsize);
    if (pixels != NULL
        && ReadFile(file, pixels, pixelsize, &got, NULL)
        && got == pixelsize)
    {
        for (y = 0; y < height; y++)
        {
            /* Positive BMPs store the bottom row first. Our extracted
               files also reverse X, so this is the inverse of
               TexWriteBmp's display-oriented rotation. */
            int sourcey = rawheight > 0 ? y : height - 1 - y;
            const unsigned char *row = pixels
                                     + sourcey * (int)width * 4;

            for (x = 0; x < width; x++)
            {
                const unsigned char *source = row + ((int)width - 1 - x) * 4;
                TexPixel *dest = &out[y * (int)width + x];

                dest->r = source[2];
                dest->g = source[1];
                dest->b = source[0];
                dest->a = source[3];
            }
        }

        *w = (int)width;
        *h = (int)height;
        ok = TRUE;
    }

    free(pixels);
    CloseHandle(file);
    return ok;
}


BOOL TexEncodePng(const TexPixel *pixels, int width, int height,
                   unsigned char **dataout, DWORD *sizeout)
{
    HRESULT initialized;
    IWICImagingFactory *factory = NULL;
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    IStream *stream = NULL;
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    STATSTG stat;
    LARGE_INTEGER start;
    unsigned char *data = NULL;
    unsigned char *bgra = NULL;
    int pixel;
    ULONG read = 0;
    BOOL ok = FALSE;

    *dataout = NULL;
    *sizeout = 0;
    if (pixels == NULL || width <= 0 || height <= 0 || width > 256 || height > 256)
    { return FALSE; }
    initialized = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) { return FALSE; }
    /* WIC's native PNG encoder accepts BGRA; keep rows and straight alpha. */
    bgra = (unsigned char *)malloc((size_t)width * height * 4);
    if (bgra == NULL) { goto done; }
    for (pixel = 0; pixel < width * height; pixel++)
    {
        bgra[pixel * 4] = pixels[pixel].b;
        bgra[pixel * 4 + 1] = pixels[pixel].g;
        bgra[pixel * 4 + 2] = pixels[pixel].r;
        bgra[pixel * 4 + 3] = pixels[pixel].a;
    }
    start.QuadPart = 0;
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IWICImagingFactory, (void **)&factory))
        || FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))
        || FAILED(IWICImagingFactory_CreateEncoder(factory, &GUID_ContainerFormatPng, NULL, &encoder))
        || FAILED(IWICBitmapEncoder_Initialize(encoder, stream, WICBitmapEncoderNoCache))
        || FAILED(IWICBitmapEncoder_CreateNewFrame(encoder, &frame, NULL))
        || FAILED(IWICBitmapFrameEncode_Initialize(frame, NULL))
        || FAILED(IWICBitmapFrameEncode_SetSize(frame, width, height))
        || FAILED(IWICBitmapFrameEncode_SetPixelFormat(frame, &format))
        || !IsEqualGUID(&format, &GUID_WICPixelFormat32bppBGRA)
        || FAILED(IWICBitmapFrameEncode_WritePixels(frame, height, width * 4,
                                                    width * height * 4, bgra))
        || FAILED(IWICBitmapFrameEncode_Commit(frame))
        || FAILED(IWICBitmapEncoder_Commit(encoder))
        || FAILED(IStream_Stat(stream, &stat, STATFLAG_NONAME))
        || stat.cbSize.QuadPart == 0 || stat.cbSize.QuadPart > 0xffffffffu)
    { goto done; }

    data = (unsigned char *)malloc((size_t)stat.cbSize.QuadPart);
    if (data == NULL || FAILED(IStream_Seek(stream, start, STREAM_SEEK_SET, NULL))
        || FAILED(IStream_Read(stream, data, (ULONG)stat.cbSize.QuadPart, &read))
        || read != stat.cbSize.QuadPart) { goto done; }
    *dataout = data;
    *sizeout = read;
    data = NULL;
    ok = TRUE;
done:
    free(bgra);
    free(data);
    if (frame != NULL) { IWICBitmapFrameEncode_Release(frame); }
    if (encoder != NULL) { IWICBitmapEncoder_Release(encoder); }
    if (stream != NULL) { IStream_Release(stream); }
    if (factory != NULL) { IWICImagingFactory_Release(factory); }
    if (SUCCEEDED(initialized)) { CoUninitialize(); }
    return ok;
}


BOOL TexGetProjectImageSize(const char *projectdir, DWORD id,
                            int *w, int *h)
{
    char path[MAX_PATH];
    unsigned char header[54];
    HANDLE file;
    DWORD got;
    LONG width;
    LONG rawheight;
    int written;

    *w = 0;
    *h = 0;

    if (ImageEditsGetPixels(projectdir, id, NULL, w, h)) { return TRUE; }

    written = snprintf(path, sizeof(path), "%s\\images\\%04lX.bmp",
                       projectdir, (unsigned long)id);
    if (written < 0 || written >= (int)sizeof(path))
    {
        return FALSE;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return ImageEditsGetDeletedPixels(projectdir, id, NULL, w, h);
    }

    if (!ReadFile(file, header, sizeof(header), &got, NULL)
        || got != sizeof(header)
        || header[0] != 'B' || header[1] != 'M'
        || texle32(header + 14) < 40
        || texle16(header + 26) != 1
        || texle16(header + 28) != 32
        || texle32(header + 30) != BI_RGB)
    {
        CloseHandle(file);
        return FALSE;
    }

    width = (LONG)texle32(header + 18);
    rawheight = (LONG)texle32(header + 22);
    CloseHandle(file);

    if (width <= 0 || width > 256
        || rawheight == 0 || rawheight < -256 || rawheight > 256)
    {
        return FALSE;
    }

    *w = (int)width;
    *h = rawheight < 0 ? (int)-rawheight : (int)rawheight;
    return TRUE;
}
