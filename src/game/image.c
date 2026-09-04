#include <ultra64.h>
#include "bondconstants.h"
#include "image.h"
#include "image_bank.h"
#include <assets/image_externs.h>
#include <PR/R4300.h>
#include "ramrom.h"


#define TEX_ALPHA_WEIGHT 961

struct texpool *ptr_texture_alloc_start;
s32 ptr_texture_alloc_end;
s32 ptr_next_available_space;
s32 ptr_last_entry_facemapping;
struct texcacheitem g_TexCacheItems[150];
s32 g_TexCacheCount;
s32 g_TexNumToLoad;
u32 bytes = 0x6DDD0;

// #1 #bytes in pixel data for image
s32 g_TexFormatNumChannels[] = 
{
    4, 3, 3, 3, 2, 2, 1, 1, 1, 1, 1, 1, 1
};
// #2 1=alphagrab.  Grabs 1 bit of alpha data for each pixel
s32 g_TexFormatHas1BitAlpha[] = 
{
    0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0
};

// #3 #bits in 'samples', *2	-1=bitmask
s32 g_TexFormatChannelSizes[] = 
{
    0x100, 0x20, 0x100, 0x20, 0x100, 0x10, 8, 0x100, 0x10, 0x100, 0x10, 0x100, 0x10
};

// #4 bitcount for pixel data
s32 g_TexFormatBitsPerPixel[] = 
{
     0x20, 0x10, 0x18, 0xF, 0x10, 8, 4, 8, 4, 0x10, 0x10, 0x10, 0x10, 
};

// #5 N64 image types (0=color, 1=YUV, 2=indexed, 3=IA, 4=I)
s32 g_TexFormatGbiMappings[] = 
{
    G_IM_FMT_RGBA, G_IM_FMT_RGBA, G_IM_FMT_RGBA, G_IM_FMT_RGBA,	
    G_IM_FMT_IA, G_IM_FMT_IA, G_IM_FMT_IA, 
    G_IM_FMT_I, G_IM_FMT_I, 
    G_IM_FMT_CI, G_IM_FMT_CI, G_IM_FMT_CI, G_IM_FMT_CI,
};

// #6 N64 pixel sizes (0=4bit, 1=8bit, 2=16bit, 3=32bit)
s32 g_TexFormatDepths[] = 
{
	G_IM_SIZ_32b,
    G_IM_SIZ_16b,
	G_IM_SIZ_32b,
	G_IM_SIZ_16b,
	G_IM_SIZ_16b,
	G_IM_SIZ_8b,
	G_IM_SIZ_4b,
	G_IM_SIZ_8b,
	G_IM_SIZ_4b,
	G_IM_SIZ_8b,
	G_IM_SIZ_4b,
	G_IM_SIZ_8b,
	G_IM_SIZ_4b,
};

// #7	imageflip values for indexed types
s32 g_TexFormatLutModes[] = {
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_NONE,
	G_TT_RGBA16,
	G_TT_RGBA16,
	G_TT_IA16,
	G_TT_IA16,
};

#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) \
    {HS, HT, SZ, F3, F4, F5, F6 },

//need way to calculate size at compile time from external data
struct image_entry g_Textures[] = {
    #include <assets/images.raw.def>
    {HIT_DEFAULT, HIT_DEFAULT,0xFFFF,0,0,0,0}
};
#undef IMAGE


/**
 * Shrink a paletted texture to half its size by averaging each each 2x2 group
 * of pixels.
 *
 * Return the number of bytes written.
 */
s32 texShrinkPaletted(u8 *src, u8 *dst, s32 srcwidth, s32 srcheight, s32 format, u16 *palette, s32 numcolours)
{
    s32 j;
    s32 i;
    s32 alignedsrcwidth;
    s32 aligneddstwidth;
    s32 dstheight;
    s16 colour1;
    s16 colour2;
    s16 colour3;
    s16 colour4;
    s32 r;
    s32 g;
    s32 b;
    s32 a;
    s32 nextrow;
    u8 *dst8;
    s32 nextcol;
    s32 c;
    u8 *src8;

    dst8 = dst;
    src8 = src;
    dstheight = (srcheight + 1) >> 1;

    switch (format)
    {
        case TEXFORMAT_RGBA16_CI8:
        case TEXFORMAT_IA16_CI8:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 7) & 0xff8;
            alignedsrcwidth = (srcwidth + 7) & 0xff8;
            break;

        case TEXFORMAT_RGBA16_CI4:
        case TEXFORMAT_IA16_CI4:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 15) & 0xff0;
            alignedsrcwidth = (srcwidth + 15) & 0xff0;
            break;
    }


    switch (format)
    {
        case TEXFORMAT_RGBA16_CI8:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    colour1 = palette[src8[j]];
                    colour2 = palette[src8[nextcol]];
                    colour3 = palette[src8[nextrow + j]];
                    colour4 = palette[src8[nextrow + nextcol]];

                    r = ((((colour1 >> 0xB) & 0x1F) + ((colour2 >> 0xB) & 0x1F) + ((colour3 >> 0xB) & 0x1F) + ((colour4 >> 0xB) & 0x1F)) >> 2) & 0x1F;
                    g = ((((colour1 >> 6) & 0x1F) + ((colour2 >> 6) & 0x1F) + ((colour3 >> 6) & 0x1F) + ((colour4 >> 6) & 0x1F)) >> 2) & 0x1F;
                    b = ((((colour1 >> 1) & 0x1F) + ((colour2 >> 1) & 0x1F) + ((colour3 >> 1) & 0x1F) + ((colour4 >> 1) & 0x1F)) >> 2) & 0x1F;
                    a = (((colour1 & 1) + (colour2 & 1) + (colour3 & 1) + (colour4 & 1) + 2) >> 2) & 1;

                    dst8[j >> 1] = texFindClosestColourIndexRGBA(palette, numcolours, r, g, b, a);
                }

                dst8 += aligneddstwidth;
                src8 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth;

        case TEXFORMAT_IA16_CI8:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    colour1 = palette[src8[j]];
                    colour2 = palette[src8[nextcol]];
                    colour3 = palette[src8[nextrow + j]];
                    colour4 = palette[src8[nextrow + nextcol]];

                    c = ((((colour1 >> 8) & 0xff) + ((colour2 >> 8) & 0xff) + ((colour3 >> 8) & 0xff) + ((colour4 >> 8) & 0xff)) >> 2) & 0xff;
                    a = ((((colour1 >> 0) & 0xff) + ((colour2 >> 0) & 0xff) + ((colour3 >> 0) & 0xff) + ((colour4 >> 0) & 0xff) + 1) >> 2) & 0xff;

                    dst8[j >> 1] = texFindClosestColourIndexIA(palette, numcolours, c, a);
                }

                dst8 += aligneddstwidth;
                src8 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth;

        case TEXFORMAT_RGBA16_CI4:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth >> 1 : 0;

                for (j = 0; j < alignedsrcwidth; j += 4)
                {
                    colour1 = palette[(src8[j >> 1] >> 4) & 0xf];
                    colour2 = palette[src8[j >> 1] >> ((j + 1 < srcwidth ? 0 : 4)) & 0xf];
                    colour3 = palette[(src8[nextrow + (j >> 1)] >> 4) & 0xf];
                    colour4 = palette[src8[nextrow + (j >> 1)] >> ((j + 1 < srcwidth ? 0 : 4)) & 0xf];

                    r = ((((colour1 >> 0xB) & 0x1F) + ((colour2 >> 0xB) & 0x1F) + ((colour3 >> 0xB) & 0x1F) + ((colour4 >> 0xB) & 0x1F)) >> 2) & 0x1F;
                    g = ((((colour1 >> 6) & 0x1F) + ((colour2 >> 6) & 0x1F) + ((colour3 >> 6) & 0x1F) + ((colour4 >> 6) & 0x1F)) >> 2) & 0x1F;
                    b = ((((colour1 >> 1) & 0x1F) + ((colour2 >> 1) & 0x1F) + ((colour3 >> 1) & 0x1F) + ((colour4 >> 1) & 0x1F)) >> 2) & 0x1F;
                    a = ((((colour1 & 1) + (colour2 & 1) + (colour3 & 1) + (colour4 & 1) + 2) >> 2) & 1);

                    dst8[j >> 2] = (texFindClosestColourIndexRGBA(palette, numcolours, r, g, b, a) * 0x10) & 0xFFFF;

                    colour1 = palette[(src8[(j + 2) >> 1] >> 4) & 0xf];
                    colour2 = palette[(src8[(j + 2) >> 1] >> (j + 3 < srcwidth ? 0 : 4)) & 0xf];
                    colour3 = palette[(src8[nextrow + ((j + 2) >> 1)] >> 4) & 0xf];
                    colour4 = palette[(src8[nextrow + ((j + 2) >> 1)] >> (j + 3 < srcwidth ? 0 : 4)) & 0xf];

                    r = ((((colour1 >> 0xB) & 0x1F) + ((colour2 >> 0xB) & 0x1F) + ((colour3 >> 0xB) & 0x1F) + ((colour4 >> 0xB) & 0x1F)) >> 2) & 0x1F;
                    g = ((((colour1 >> 6) & 0x1F) + ((colour2 >> 6) & 0x1F) + ((colour3 >> 6) & 0x1F) + ((colour4 >> 6) & 0x1F)) >> 2) & 0x1F;
                    b = ((((colour1 >> 1) & 0x1F) + ((colour2 >> 1) & 0x1F) + ((colour3 >> 1) & 0x1F) + ((colour4 >> 1) & 0x1F)) >> 2) & 0x1F;
                    a = ((((colour1 & 1) + (colour2 & 1) + (colour3 & 1) + (colour4 & 1) + 2) >> 2) & 1);

                    dst8[j >> 2] |= texFindClosestColourIndexRGBA(palette, numcolours, r, g, b, a) & 0xff;
                }

                dst8 += aligneddstwidth >> 1;
                src8 += alignedsrcwidth;
            }

            return (aligneddstwidth >> 1) * dstheight;

        case TEXFORMAT_IA16_CI4:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth >> 1 : 0;

                for (j = 0; j < alignedsrcwidth; j += 4)
                {
                    // @bug: The brackets are wrong in colour2 and colour4 which
                    // causes the index shift to be part of the ternary condition.
                    // It's done correctly in TEXFORMAT_RGBA16_CI4 (above).
                    // This buggy calculation is repeated further below.
                    colour1 = palette[(src8[j >> 1] >> 4) & 0xf];
                    colour2 = palette[(src8[j >> 1] >> (j + 1 < srcwidth) ? 0 : 4) & 0xf];
                    colour3 = palette[(src8[nextrow + (j >> 1)] >> 4) & 0xf];
                    colour4 = palette[(src8[nextrow + (j >> 1)] >> (j + 1 < srcwidth) ? 0 : 4) & 0xf];

                    c = ((((colour1 >> 8) & 0xff) + ((colour2 >> 8) & 0xff) + ((colour3 >> 8) & 0xff) + ((colour4 >> 8) & 0xff)) >> 2) & 0xff;
                    a = ((((colour1 >> 0) & 0xff) + ((colour2 >> 0) & 0xff) + ((colour3 >> 0) & 0xff) + ((colour4 >> 0) & 0xff) + 1) >> 2) & 0xff;

                    dst8[j >> 2] = (texFindClosestColourIndexIA(palette, numcolours, c, a) * 0x10) & 0xFFFF;

                    colour1 = palette[(src8[(j + 2) >> 1] >> 4) & 0xf];
                    colour2 = palette[(src8[(j + 2) >> 1] >> (j + 3 < srcwidth) ? 0 : 4) & 0xf];
                    colour3 = palette[(src8[nextrow + ((j + 2) >> 1)] >> 4) & 0xf];
                    colour4 = palette[(src8[nextrow + ((j + 2) >> 1)] >> (j + 3 < srcwidth) ? 0 : 4) & 0xf];

                    c = ((((colour1 >> 8) & 0xff) + ((colour2 >> 8) & 0xff) + ((colour3 >> 8) & 0xff) + ((colour4 >> 8) & 0xff)) >> 2) & 0xff;
                    a = ((((colour1 >> 0) & 0xff) + ((colour2 >> 0) & 0xff) + ((colour3 >> 0) & 0xff) + ((colour4 >> 0) & 0xff) + 1) >> 2) & 0xff;

                    dst8[j >> 2] |= texFindClosestColourIndexIA(palette, numcolours, c, a) & 0xff;
                }

                dst8 += aligneddstwidth >> 1;
                src8 += alignedsrcwidth;
            }

            return (aligneddstwidth >> 1) * dstheight;
    }

    return 0;
}


/**
 * Find the palette entry closest to the target colour and return its index.
 * Used for texture LOD generation/shrinking a colour-indexed texture (see texShrinkPaletted): the 2x2
 * box filter averages four palette entries, and the average may not be a colour
 * the palette already holds, so it has to be requantized to the nearest entry. This allows the LOD
 * textures to use existing palettes instead of creating new ones.
 * 
 * The design of the algorithm is to:
 * 1) Return an exact match in the palette for the target colour if there is one.
 * 2) Failing that, a binary search uses cheap scalar ordering to find a promising region in the palette.
 * 3) Perform a more meaningful RGBA comparison on only a small section of the palette.
 * 
 * Stage 2 is an approximation so this function isn't necessarily guaranteed to return the closest matching palette index for a target colour.
 * 
 * Used by TEXFORMAT_RGBA16_CI8, TEXFORMAT_RGBA16_CI4
 *
 * @param palette    Palette to search, RGBA5551, assumed ordered by the stage 2 key
 * @param numcolours Number of entries in palette
 * @param r          Target red, 0..31
 * @param g          Target green, 0..31
 * @param b          Target blue, 0..31
 * @param a          Target alpha, 0..1
 * @return Index of the closest entry, or 0 if numcolours is not positive
 */
s32 texFindClosestColourIndexRGBA(u16 *palette, s32 numcolours, s32 r, s32 g, s32 b, s32 a)
{
    s32 low;
    s32 high;
    s32 i;
    u16 targetcolour;
    s32 targetmagnitude;
 
    // Cursor into the palette: the midpoint in stage 2, the scan position in stage 3.
    s32 paletteidx;
 
    u16 colour;
    s32 red;
    s32 green;
    s32 blue;
    s32 alpha;
    s32 magnitude;
 
    s32 diffr;
    s32 diffg;
    s32 diffb;
    s32 alphapenalty;
    s32 distance;
 
    s32 bestindex;
    s32 bestvalue;
 
    // Stage 1: scan the whole palette for a matching colour and return its index if one is found.
    targetcolour = ((r << 11) | (g << 6) | (b << 1) | a);
 
    for (i = 0; i < numcolours; i++)
    {
        if (targetcolour == palette[i])
        {
            return i;
        }
    }
 
    /** 
     * Stage 2: Find a promising palette neighborhood.
     */
    low = 0;
    high = numcolours - 1;
    // TEX_ALPHA_WEIGHT = 31^2, the maximum possible squared difference in one five-bit color channel.
    targetmagnitude = (r * r) + (g * g) + (b * b) + (a * TEX_ALPHA_WEIGHT);
 
    while (high - low >= 2)
    {
        paletteidx = (high + low) >> 1;
 
        colour = palette[paletteidx];
        red = (colour >> 11) & 0x1F;
        green = (colour >> 6) & 0x1F;
        blue = (colour >> 1) & 0x1F;
        alpha = colour & 1;
 
        magnitude = (red * red) + (green * green) + (blue * blue) + (alpha * TEX_ALPHA_WEIGHT);
 
        if (magnitude < targetmagnitude)
        {
            low = paletteidx;
            continue;
        }
 
        if (targetmagnitude < magnitude)
        {
            high = paletteidx;
        }
        else
        {
            high = paletteidx;
            low = paletteidx;
        }
    }
 
    // Stage 3: Search the nearby palette entries accurately. 
    low = high - 4;
 
    if (low < 0)
    {
        low = 0;
    }
 
    high += 4;
 
    if (high >= numcolours)
    {
        high = numcolours - 1;
    }
 
    bestindex = 0;
    bestvalue = 999999;
 
    for (paletteidx = low; paletteidx <= high; paletteidx++)
    {
        colour = palette[paletteidx];
        diffr = ((colour >> 11) & 0x1F) - r;
        diffg = ((colour >> 6) & 0x1F) - g;
        diffb = ((colour >> 1) & 0x1F) - b;
 
        // An alpha mismatch costs 961, strongly discouraging a palette entry with the wrong transparency.
        alphapenalty = (a == (colour & 1)) ? 0 : TEX_ALPHA_WEIGHT;
 
        distance = alphapenalty;
        distance += diffr * diffr;
        distance += diffg * diffg;
        distance += diffb * diffb;
 
        if (distance < bestvalue)
        {
            bestindex = paletteidx;
            bestvalue = distance;
        }
    }
 
    return bestindex;
}


/**
 * Find the palette entry closest to the given intensity/alpha pair and return its index.
 *
 * Used by TEXFORMAT_IA16_CI8 and TEXFORMAT_IA16_CI4 to
 * requantize box-filtered averages back onto the palette.
 *
 * Colours are IA16, packed iiiiiiiiaaaaaaaa, intensity and alpha both 0..255.
 *
 * @param palette    Palette to search, IA16, assumed ordered by the stage 2 key
 * @param numcolours Number of entries in palette
 * @param intensity  Target intensity, 0..255
 * @param alpha      Target alpha, 0..255
 * @return Index of the closest entry, or 0 if numcolours is not positive
 */
s32 texFindClosestColourIndexIA(u16 *palette, s32 numcolours, s32 intensity, s32 alpha)
{
    s32 scanstart;
    s32 high;
    s32 i;
    s32 scanidx;
    s32 bestindex;
    s32 bestvalue;
    s32 low;
    s32 targetcolour;
    s32 targetmagnitude;
    s32 colour;
    s32 entryintensity;
    s32 entryalpha;
    s32 magnitude;
    s32 diffi;
    s32 diffa;
    s32 distance;
 
    // Stage 1: scan the whole palette for a matching intensity and return its index if one is found.
    targetcolour = (intensity << 8) | alpha;
 
    for (i = 0; i < numcolours; i++)
    {
        if ((u16)targetcolour == palette[i])
        {
            return i;
        }
    }
 
    // Stage 2: binary search by squared magnitude.
    low = 0;
    high = numcolours - 1;
    targetmagnitude = (intensity * intensity) + (alpha * alpha);
 
    while (high - low >= 2)
    {
        s32 mid;
 
        mid = (high + low) >> 1;
        colour = palette[mid];
 
        entryintensity = (colour >> 8) & 0xFF;
        entryalpha = colour & 0xFF;
        magnitude = (entryintensity * entryintensity) + (entryalpha * entryalpha);
 
        if (magnitude < targetmagnitude)
        {
            low = mid;
            continue;
        }
 
        if (targetmagnitude < magnitude)
        {
            high = mid;
        }
        else
        {
            // Equal magnitude. Collapse the window to end the search here.
            low = mid;
            high = mid;
        }
    }
 
    // Stage 3: widen the window by four entries either side and clamp it.
    scanstart = high - 4;
    scanidx = scanstart;
    high += 4;

    if (scanidx < 0)
    {
        scanstart = 0;
    }
 
    if (high >= numcolours)
    {
        high = numcolours - 1;
    }

    /* 999999 is an unreachable sentinel: the worst possible distance is
     * 2 * 255 * 255 = 130050. */
    bestindex = 0;
    bestvalue = 999999;

    scanidx = scanstart;
 
    if (scanidx <= high)
    {
        s32 scanend;

        for (;;)
        {
            colour = palette[scanidx];
 
            diffi = ((colour >> 8) & 0xff) - intensity;
            diffa = (colour & 0xff) - alpha;
            distance = (diffi * diffi) + (diffa * diffa);
            scanend = high + 1;
 
            if (distance < bestvalue)
            {
                bestindex = scanidx;
                bestvalue = distance;
            }

            scanidx++;

            if (scanend == scanidx)
            {
                break;
            }
        }
    }
 
    return bestindex;
}


/**
 * Shrink a non-paletted texture to half its size by averaging each each 2x2
 * group of pixels.
 *
 * Return the number of bytes written.
 *
 * If the source width is an odd number, the destination's final column is
 * calculated by sampling the final source column twice. Likewise for the height.
 */
s32 texShrinkNonPaletted(u8 *src, u8 *dst, s32 srcwidth, s32 srcheight, s32 format)
{
    s32 i;
    s32 j;
    s32 alignedsrcwidth;
    s32 aligneddstwidth;
    u32 *dst32 = (u32 *) dst;
    u16 *dst16 = (u16 *) dst;
    u8 *dst8 = dst;
    u32 *src32 = (u32 *) src;
    u16 *src16 = (u16 *) src;
    u8 *src8 = src;
    s32 dstheight = (srcheight + 1) >> 1;
    s32 r;
    s32 g;
    s32 b;
    s32 a;
    s32 c;
    u32 tl32;
    u32 tr32;
    u32 bl32;
    u32 br32;
    u16 tl16;
    u16 tr16;
    u16 bl16;
    u16 br16;
    u8 tl8;
    u8 tr8;
    u8 bl8;
    u8 br8;
    s32 nextrow;
    s32 nextcol;

    switch (format)
    {
        case TEXFORMAT_RGBA32:
        case TEXFORMAT_RGB24:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 3) & 0xffc;
            alignedsrcwidth = (srcwidth + 3) & 0xffc;
            break;
        case TEXFORMAT_RGBA16:
        case TEXFORMAT_RGB15:
        case TEXFORMAT_IA16:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 3) & 0xffc;
            alignedsrcwidth = (srcwidth + 3) & 0xffc;
            break;
        case TEXFORMAT_IA8:
        case TEXFORMAT_I8:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 7) & 0xff8;
            alignedsrcwidth = (srcwidth + 7) & 0xff8;
            break;
        case TEXFORMAT_IA4:
        case TEXFORMAT_I4:
            aligneddstwidth = (((srcwidth + 1) >> 1) + 15) & 0xff0;
            alignedsrcwidth = (srcwidth + 15) & 0xff0;
            break;
    }

    switch (format)
    {
        case TEXFORMAT_RGBA32:
        case TEXFORMAT_RGB24:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    tl32 = src32[j];
                    tr32 = src32[nextcol];
                    bl32 = src32[nextrow + j];
                    br32 = src32[nextrow + nextcol];

                    r = ((((tl32 >> 24) & 0xff) + ((tr32 >> 24) & 0xff) + ((bl32 >> 24) & 0xff) + ((br32 >> 24) & 0xff)) >> 2) & 0xff;
                    g = ((((tl32 >> 16) & 0xff) + ((tr32 >> 16) & 0xff) + ((bl32 >> 16) & 0xff) + ((br32 >> 16) & 0xff)) >> 2) & 0xff;
                    b = ((((tl32 >>  8) & 0xff) + ((tr32 >>  8) & 0xff) + ((bl32 >>  8) & 0xff) + ((br32 >>  8) & 0xff)) >> 2) & 0xff;
                    a = ((((tl32 >>  0) & 0xff) + ((tr32 >>  0) & 0xff) + ((bl32 >>  0) & 0xff) + ((br32 >>  0) & 0xff) + 1) >> 2) & 0xff;

                    dst32[j >> 1] = r << 24 | g << 16 | b << 8 | a;
                }

                dst32 += aligneddstwidth;
                src32 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth * 4;

        case TEXFORMAT_RGBA16:
        case TEXFORMAT_RGB15:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    tl16 = src16[j];
                    tr16 = src16[nextcol];
                    bl16 = src16[nextrow + j];
                    br16 = src16[nextrow + nextcol];

                    r = ((((tl16 >> 11) & 0x1f) + ((tr16 >> 11) & 0x1f) + ((bl16 >> 11) & 0x1f) + ((br16 >> 11) & 0x1f)) >> 2) & 0x1f;
                    g = ((((tl16 >>  6) & 0x1f) + ((tr16 >>  6) & 0x1f) + ((bl16 >>  6) & 0x1f) + ((br16 >>  6) & 0x1f)) >> 2) & 0x1f;
                    b = ((((tl16 >>  1) & 0x1f) + ((tr16 >>  1) & 0x1f) + ((bl16 >>  1) & 0x1f) + ((br16 >>  1) & 0x1f)) >> 2) & 0x1f;
                    a = ((((tl16 >>  0) & 0x01) + ((tr16 >>  0) & 0x01) + ((bl16 >>  0) & 0x01) + ((br16 >>  0) & 0x01) + 2) >> 2) & 0x01;

                    dst16[j >> 1] = r << 11 | g << 6 | b << 1 | a;
                }

                dst16 += aligneddstwidth;
                src16 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth * 2;

        case TEXFORMAT_IA16:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    tl16 = src16[j];
                    tr16 = src16[nextcol];
                    bl16 = src16[nextrow + j];
                    br16 = src16[nextrow + nextcol];

                    c = (((tl16 >> 8) & 0xff) + ((tr16 >> 8) & 0xff) + ((bl16 >> 8) & 0xff) + ((br16 >> 8) & 0xff)) >> 2;
                    a = ((tl16 & 0xff) + (tr16 & 0xff) + (bl16 & 0xff) + (br16 & 0xff) + 1) >> 2;

                    dst16[j >> 1] = ((u8)c << 8) | (a & 0xFF);
                }

                dst16 += aligneddstwidth;
                src16 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth * 2;

        case TEXFORMAT_IA8:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    tl8 = src8[j];
                    tr8 = src8[nextcol];
                    bl8 = src8[nextrow + j];
                    br8 = src8[nextrow + nextcol];

                    c = ((((tl8 >> 4) & 0xf) + ((tr8 >> 4) & 0xf) + ((bl8 >> 4) & 0xf) + ((br8 >> 4) & 0xf)) << 2) & 0xF0;
                    a = (((tl8 & 0xf) + (tr8 & 0xf) + (bl8 & 0xf) + (br8 & 0xf) + 1) >> 2) & 0xF;

                    dst8[j >> 1] = c | a;
                }

                dst8 += aligneddstwidth;
                src8 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth;

        case TEXFORMAT_I8:
            for (i = 0; i < srcheight; i += 2)
            {
                nextrow = i + 1 < srcheight ? alignedsrcwidth : 0;

                for (j = 0; j < alignedsrcwidth; j += 2)
                {
                    nextcol = j + 1 < srcwidth ? j + 1 : j;

                    tl8 = src8[j];
                    tr8 = src8[nextcol];
                    bl8 = src8[nextrow + j];
                    br8 = src8[nextrow + nextcol];

                    c = (u16)((tl8 + tr8 + bl8 + br8 + 1) >> 2);

                    dst8[j >> 1] = c;
                }

                dst8 += aligneddstwidth;
                src8 += alignedsrcwidth * 2;
            }

            return dstheight * aligneddstwidth;

        case TEXFORMAT_IA4:
            for (i = 0; i < srcheight; i += 2)
            {
                nextcol = i + 1;

                for (j = 0; j < alignedsrcwidth; j += 4)
                {
                    tl8 = src8[j >> 1];
                    tr8 = src8[(nextcol < srcheight ? (alignedsrcwidth >> 1) : 0) + (j >> 1)];
                    bl8 = src8[(j >> 1) + 1];
                    br8 = src8[(nextcol < srcheight ? (alignedsrcwidth >> 1) : 0) + (j >> 1) + 1];

                    c = (((((tl8 >> 5) & 7) + ((tl8 >> 1) & 7) + ((tr8 >> 5) & 7) + ((tr8 >> 1) & 7)) << 3) & 0xe0)
                        | (((((bl8 >> 5) & 7) + ((bl8 >> 1) & 7) + ((br8 >> 5) & 7) + ((br8 >> 1) & 7)) >> 1) & 0xe);

                    a = (((((tl8 >> 4) & 1) + (tl8 & 1) + ((tr8 >> 4) & 1) + (tr8 & 1) + 1) << 2) & 0x10)
                        | (((((bl8 >> 4) & 1) + (bl8 & 1) + ((br8 >> 4) & 1) + (br8 & 1) + 1) >> 2) & 1);

                    dst8[j >> 2] = c | a;
                }

                dst8 += aligneddstwidth >> 1;
                src8 += alignedsrcwidth;
            }

            return (aligneddstwidth >> 1) * dstheight;

        case TEXFORMAT_I4:
            for (i = 0; i < srcheight; i += 2)
            {
                for (j = 0; j < alignedsrcwidth; j += 4)
                {
                    tl8 = src8[j >> 1];
                    tr8 = src8[(i + 1 < srcheight ? (alignedsrcwidth >> 1) : 0) + (j >> 1)];
                    bl8 = src8[(j >> 1) + 1];
                    br8 = src8[(i + 1 < srcheight ? (alignedsrcwidth >> 1) : 0) + (j >> 1) + 1];

                    c = ((((tl8 >> 4) & 0xf) + (tl8 & 0xf) + ((tr8 >> 4) & 0xf) + (tr8 & 0xf)) << 2) & 0xf0;
                    a = ((((bl8 >> 4) & 0xf) + (bl8 & 0xf) + ((br8 >> 4) & 0xf) + (br8 & 0xf)) >> 2) & 0xf;

                    dst8[j >> 2] = c | a;
                }

                dst8 += aligneddstwidth >> 1;
                src8 += alignedsrcwidth;
            }

            return (aligneddstwidth >> 1) * dstheight;
    }

    return 0;
}


/**
 * For every second row, swap the bytes within that row.
 *
 * For textures with 32-bit colour values (in GBI format), swap every pair
 * within each word. For all other textures, swap every byte within each pair.
 */
void texSwapAltRowBytes(u8 *dst, s32 width, s32 height, s32 format)
{
	s32 x;
	s32 y;
	s32 alignedwidth;
	u32 *row = (u32 *)dst;
	s32 tmp;

	switch (format) {
	case TEXFORMAT_RGBA32:
	case TEXFORMAT_RGB24:
		alignedwidth = (width + 3) & 0xffc;
		break;
	case TEXFORMAT_RGBA16:
	case TEXFORMAT_RGB15:
	case TEXFORMAT_IA16:
		alignedwidth = ((width + 3) & 0xffc) >> 1;
		break;
	case TEXFORMAT_IA8:
	case TEXFORMAT_I8:
	case TEXFORMAT_RGBA16_CI8:
	case TEXFORMAT_IA16_CI8:
		alignedwidth = ((width + 7) & 0xff8) >> 2;
		break;
	case TEXFORMAT_IA4:
	case TEXFORMAT_I4:
	case TEXFORMAT_RGBA16_CI4:
	case TEXFORMAT_IA16_CI4:
		alignedwidth = ((width + 0xf) & 0xff0) >> 3;
		break;
	}

	row += alignedwidth;

	if (format == TEXFORMAT_RGBA32 || format == TEXFORMAT_RGB24) {
		for (y = 1; y < height; y += 2) {
			for (x = 0; x < alignedwidth; x += 4) {
				tmp = row[x + 0];
				row[x + 0] = row[x + 2];
				row[x + 2] = tmp;

				tmp = row[x + 1];
				row[x + 1] = row[x + 3];
				row[x + 3] = tmp;
			}

			row += alignedwidth * 2;
		}
	} else {
		for (y = 1; y < height; y += 2) {
			for (x = 0; x < alignedwidth; x += 2) {
				tmp = row[x + 0];
				row[x + 0] = row[x + 1];
				row[x + 1] = tmp;
			}

			row += alignedwidth * 2;
		}
	}
}


void texInitPool(struct texpool *arg0, u8 *arg1, s32 arg2)
{
    arg0->start = arg1;
	arg0->end = (struct tex *)(arg1 + arg2);
    arg0->leftpos = arg1;
    arg0->rightpos = (struct tex *)(arg1 + arg2);
}


struct tex *texFindInPool(s32 texturenum, struct texpool *arg1)
{
    struct tex *end;
    struct tex *cur;
    s32 i;

    if (arg1 == NULL)
    {
        arg1 = &ptr_texture_alloc_start;
    }

    end = arg1->end;
    cur = arg1->rightpos;

    while (cur < end)
    {
        if (cur->texturenum == texturenum)
        {
            return cur;
        }

        cur++;
    }

    return NULL;
}


s32 texFreeBytesInBuffer(struct texpool *arg0)
{
	return (u32)arg0->rightpos - (u32)arg0->leftpos;
}


void texLoadFromDisplayList(Gfx *gdl, struct texpool *arg1)
{
    u8 *bytes = (u8 *)gdl;

    while (bytes[0] != (u8)G_ENDDL)
    {
        // Look for GBI sequence: fd...... abcd....
        if (bytes[0] == G_SETTIMG && bytes[4] == 0xab && bytes[5] == 0xcd)
        {
            texLoad((u32 *)((s32)bytes + 4), arg1);
        }

        bytes += 8;
    }
}


extern u8 _imagesSegmentRomStart;

#define RAW_TEXTURE_BASE_HEADER_SIZE 0x70
#define RAW_TEXTURE_MAX_HEADER_SIZE 0x270
#define RAW_TEXTURE_DESCRIPTOR_OFFSET 16
#define RAW_TEXTURE_DESCRIPTOR_SIZE 12
#define RAW_TEXTURE_PALETTE_OFFSET 100
#define RAW_TEXTURE_MAX_IMAGES 7

static u16 texReadRawU16(u8 *src)
{
    return (src[0] << 8) | src[1];
}


static u32 texReadRawU32(u8 *src)
{
    return ((u32)src[0] << 24) | ((u32)src[1] << 16) | (src[2] << 8) | src[3];
}


static bool texShouldWriteLodCache(struct tex *tex)
{
    s32 i;

    for (i = 0; i < g_TexCacheCount; i++)
    {
        if (g_TexCacheItems[i].texturenum == tex->texturenum)
        {
            return FALSE;
        }
    }

    return TRUE;
}


static void texCommitLodCache(struct tex *tex)
{
    g_TexCacheItems[g_TexCacheCount].texturenum = tex->texturenum;
    g_TexCacheCount++;

    if (g_TexCacheCount >= ARRAYCOUNT(g_TexCacheItems))
    {
        g_TexCacheCount = 0;
    }
}


/**
 * Copy a build-time-decoded texture from ROM into a texture pool.
 *
 * Pixel rows and palettes are already in the format expected by the RDP. For
 * textures with implicit LODs, the lower levels are still generated here so
 * their memory use and filtering behaviour remain unchanged.
 */
static s32 texLoadRaw(u8 *header, u32 romAddress, struct texpool *pool)
{
    struct tex *tex = pool->rightpos;
    u8 *dst = pool->leftpos;
    u16 *palette = (u16 *)&header[RAW_TEXTURE_PALETTE_OFFSET];
    s32 paletteCount = texReadRawU16(&header[8]);
    s32 hasExplicitLods = header[4];
    s32 lodCount = header[5];
    s32 imageCount = header[6];
    s32 recordSize = texReadRawU32(&header[12]);
    s32 writeToCache = FALSE;
    s32 totalBytes = 0;
    s32 format = 0;
    s32 width = 0;
    s32 height = 0;
    s32 firstDataOffset = 0;
    s32 payloadBytes = 0;
    s32 payloadPosition = 0;
    s32 i;

    if (hasExplicitLods)
    {
        writeToCache = texShouldWriteLodCache(tex);
    }

    tex->maxlod = lodCount;
    tex->hasExplicitLods = hasExplicitLods;
    tex->unk0a = paletteCount ? paletteCount - 1 : 0;

    for (i = 0; i < imageCount; i++)
    {
        u8 *descriptor = &header[RAW_TEXTURE_DESCRIPTOR_OFFSET + i * RAW_TEXTURE_DESCRIPTOR_SIZE];
        s32 dataOffset = texReadRawU32(&descriptor[4]);
        s32 dataSize = texReadRawU32(&descriptor[8]);

        if (i == 0)
        {
            firstDataOffset = dataOffset;
        }

        if (descriptor[0] >= ARRAYCOUNT(g_TexFormatGbiMappings)
                || dataOffset < texReadRawU16(&header[10])
                || dataOffset != firstDataOffset + payloadBytes
                || dataSize < 0 || dataOffset + dataSize > recordSize)
        {
            return 0;
        }

        payloadBytes += dataSize;
    }

    romCopy(dst, (void *)(romAddress + firstDataOffset), payloadBytes);

    for (i = 0; i < imageCount; i++)
    {
        u8 *descriptor = &header[RAW_TEXTURE_DESCRIPTOR_OFFSET + i * RAW_TEXTURE_DESCRIPTOR_SIZE];
        s32 dataSize = texReadRawU32(&descriptor[8]);

        format = descriptor[0];
        width = descriptor[1];
        height = descriptor[2];

        if (i == 0)
        {
            tex->width = width;
            tex->height = height;
            tex->gbiformat = g_TexFormatGbiMappings[format];
            tex->depth = g_TexFormatDepths[format];
            tex->lutmodeindex = g_TexFormatLutModes[format] >> G_MDSFT_TEXTLUT;
        }
        else if (writeToCache)
        {
            g_TexCacheItems[g_TexCacheCount].widths[i - 1] = width;
            g_TexCacheItems[g_TexCacheCount].heights[i - 1] = height;
        }

        if (hasExplicitLods)
        {
            texSwapAltRowBytes(&dst[payloadPosition], width, height, format);
        }

        payloadPosition += dataSize;
    }

    totalBytes = payloadBytes;

    if (!hasExplicitLods && imageCount == 1)
    {
        if (lodCount >= 2)
        {
            u8 *source = dst;
            u8 *output = &dst[totalBytes];
            s32 currentWidth = width;
            s32 currentHeight = height;

            for (i = 1; i < lodCount; i++)
            {
                s32 imageBytes;

                if (paletteCount)
                {
                    imageBytes = texShrinkPaletted(source, output, currentWidth,
                            currentHeight, format, palette, paletteCount);

                    if (totalBytes + imageBytes > 0x800)
                    {
                        tex->maxlod = i;
                        break;
                    }
                }
                else
                {
                    imageBytes = texShrinkNonPaletted(source, output, currentWidth,
                            currentHeight, format);
                }

                texSwapAltRowBytes(source, currentWidth, currentHeight, format);
                totalBytes += imageBytes;
                currentWidth = (currentWidth + 1) >> 1;
                currentHeight = (currentHeight + 1) >> 1;
                source = output;
                output += imageBytes;
            }

            texSwapAltRowBytes(source, currentWidth, currentHeight, format);
        }
        else if (lodCount == 1)
        {
            texSwapAltRowBytes(dst, width, height, format);
        }
    }

    if (paletteCount)
    {
        s32 paletteBytes = paletteCount * sizeof(u16);

        bcopy(palette, &dst[totalBytes], paletteBytes);
        totalBytes += paletteBytes;
    }

    totalBytes = (totalBytes + 7) & ~7;

    if (writeToCache)
    {
        texCommitLodCache(tex);
    }

    return totalBytes;
}

/**
 * Load a build-time-decoded texture from ROM.
 *
 * The given pointer points to a word which determines what to load.
 * The formats of the word are:
 *
 *     abcdxxxx -> load texture number xxxx
 *     0000xxxx -> load texture number xxxx
 *     (memory address) -> the texture is already loaded, so do nothing
 *
 * After loading the texture, the value that's pointed to is
 * changed to be a pointer to... something.
 */
void texLoad(s32 *updateword, struct texpool *pool)
{
    u8 headerBuffer[RAW_TEXTURE_MAX_HEADER_SIZE + 0xf];
    u8 *header;
    struct tex *tex;
    u32 romAddress;
    s32 textureOffset;
    s32 hasValidHeader;
    s32 headerSize;
    s16 *texnumptr;
    s32 bytesout;

    if (pool == NULL)
    {
        pool = (struct texpool*) &ptr_texture_alloc_start;
    }

    g_TexNumToLoad = *updateword & 0xffff;
    tex = texFindInPool(g_TexNumToLoad, pool);

    if (tex == NULL)
    {
        header = (u8 *)(((u32)headerBuffer + 0xf) & ~0xf);
        textureOffset = *((s32 *)&g_Textures[g_TexNumToLoad]) & 0xffffff;
        romAddress = (u32)&_imagesSegmentRomStart + textureOffset;
        romCopy(header, (void *)romAddress, RAW_TEXTURE_BASE_HEADER_SIZE);
        headerSize = texReadRawU16(&header[10]);

        hasValidHeader = header[0] == 'G' && header[1] == 'U'
                && header[2] == 'T' && header[3] == 'X'
                && header[6] >= 1 && header[6] <= RAW_TEXTURE_MAX_IMAGES
                && header[5] <= RAW_TEXTURE_MAX_IMAGES
                && headerSize >= RAW_TEXTURE_BASE_HEADER_SIZE
                && headerSize <= RAW_TEXTURE_MAX_HEADER_SIZE;

        if (hasValidHeader && headerSize > RAW_TEXTURE_BASE_HEADER_SIZE)
        {
            romCopy(&header[RAW_TEXTURE_BASE_HEADER_SIZE],
                    (void *)(romAddress + RAW_TEXTURE_BASE_HEADER_SIZE),
                    headerSize - RAW_TEXTURE_BASE_HEADER_SIZE);
        }

        hasValidHeader = hasValidHeader
                && RAW_TEXTURE_PALETTE_OFFSET + texReadRawU16(&header[8]) * 2 <= headerSize;

        if (!hasValidHeader || texFreeBytesInBuffer(pool) < 0x10cc)
        {
            *updateword = osVirtualToPhysical(pool->start);
            return;
        }

        texnumptr = (s16 *)pool->leftpos;
        *texnumptr = g_TexNumToLoad;
        pool->leftpos += 8;

        pool->rightpos--;
        tex = pool->rightpos;
        tex->texturenum = g_TexNumToLoad;
        tex->data = pool->leftpos;

        bytesout = texLoadRaw(header, romAddress, pool);
        pool->leftpos += bytesout;
    }

    *updateword = osVirtualToPhysical(tex->data);
}


void texLoadFromModelFileHeader(ModelFileHeader* arg0, struct texpool* arg1)
{
    s32 i;
    ModelFileTextures* textures;

    textures = arg0->Textures;

    for (i = 0; i < arg0->numtextures; i++)
    {
        if ((s32)textures[i].TextureID < (s32)MAX_TEXTURES)
        {
            texLoad(&textures[i], arg1);
        }
    }
}


void texLoadFromTextureNum(s32 texturenum, struct texpool *arg1)
{
    u32 texturenumcopy;
    texturenumcopy = texturenum;
    texLoad(&texturenumcopy, arg1);
}
