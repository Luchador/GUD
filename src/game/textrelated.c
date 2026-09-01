#include <ultra64.h>
#include <memp.h>
#include "textrelated.h"
#include "bondtypes.h"


#define SPACE_WIDTH 5

#define M_COLOR_R(x) (u8)(x >> 0x18)
#define M_COLOR_G(x) (u8)(x >> 0x10)
#define M_COLOR_B(x) (u8)(x >> 0x08)
#define M_COLOR_A(x) (u8)(x >> 0x00)

s32 text_spacing = 0;
s32 text_orientation = 0;
s32 text_wordwrap = 0;
s32 overlap_correction = -1;
s32 text_bilevel_filter = FALSE;
s32 text_x = 0;
s32 text_y = 0;
s32 text_s = 0;
s32 text_t = 0;

struct font * ptrFontBankGothic = NULL;
struct fontchar * ptrFontBankGothicChars = NULL;
struct font * ptrFontZurichBold = NULL;
struct fontchar * ptrFontZurichBoldChars = NULL;

extern u8 _fontbankgothicSegmentEnd;
extern u8 _fontbankgothicSegmentRomStart;
extern u8 _fontzurichboldSegmentEnd;
extern u8 _fontzurichboldSegmentRomStart;
extern u8 _fontzurichboldSegmentStart;
extern u8 _fontbankgothicSegmentStart;

// forward declarations

Gfx *textRenderGlyph(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight, s32 yOffset);
Gfx *textRenderGlyphOutlined(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, u32 textColour, u32 outlineColour, s32 clipWidth, s32 clipHeight, s32 yOffset);
Gfx *textDrawGlyphQuad(Gfx *gdl, s32 x, s32 y, struct fontchar *curchar, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight);

// end forward declarations

void setTextSpacingInverted(s32 spacing)
{
    text_spacing = -spacing;
}


void setTextOrientation(s32 orientation)
{
    text_orientation = orientation;
}


void setTextWordWrap(s32 flag)
{
    text_wordwrap = flag;
}


void setTextOverlapCorrection(s32 flag)
{
    overlap_correction = flag;
}


void load_font_tables(void)
{
    u32 len;
	s32 i;
    
    text_spacing = 0;
    text_orientation = 0;
    text_wordwrap = 0;
    overlap_correction = -1;
    text_bilevel_filter = 0;
    text_x = 0;
    text_y = 0;
    text_s = 0;
    text_t = 0;

    len = (romptr_t)&_fontbankgothicSegmentEnd - (romptr_t)&_fontbankgothicSegmentStart;
	ptrFontBankGothic = (struct font *)mempAllocBytesInBank(len, MEMPOOL_STAGE);
	ptrFontBankGothicChars = ptrFontBankGothic->chars;

	romCopy(ptrFontBankGothic, (void *) &_fontbankgothicSegmentRomStart, len);

    // Convert pointers
	for (i = 0; i < 94; i++)
    {
		ptrFontBankGothicChars[i].pixeldata += (uintptr_t)ptrFontBankGothic;
	}

    len = (romptr_t)&_fontzurichboldSegmentEnd - (romptr_t)&_fontzurichboldSegmentStart;
	ptrFontZurichBold = (struct font *)mempAllocBytesInBank(len, MEMPOOL_STAGE);
	ptrFontZurichBoldChars = ptrFontZurichBold->chars;

	romCopy(ptrFontZurichBold, (void *) &_fontzurichboldSegmentRomStart, len);

    // Convert pointers
	for (i = 0; i < 94; i++)
    {
		ptrFontZurichBoldChars[i].pixeldata += (uintptr_t)ptrFontZurichBold;
	}
}


Gfx *gfxSetup2DTextureMode(Gfx *gdl)
{
	gDPPipeSync(gdl++);
	gDPSetCycleType(gdl++, G_CYC_1CYCLE);
	gDPSetColorDither(gdl++, G_CD_DISABLE);
	gDPSetRenderMode(gdl++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
	gDPSetCombineLERP(gdl++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
	gDPSetTexturePersp(gdl++, G_TP_NONE);
	gDPSetAlphaCompare(gdl++, G_AC_NONE);
	gDPSetTextureLOD(gdl++, G_TL_TILE);
	gDPSetTextureConvert(gdl++, G_TC_FILT);
	gDPSetTextureLUT(gdl++, G_TT_NONE);

	if (text_bilevel_filter)
    {
		gDPSetTextureFilter(gdl++, G_TF_AVERAGE);
	} 
    else
    {
		gDPSetTextureFilter(gdl++, G_TF_BILERP);
	}

	return gdl;
}


Gfx *gfxRestore3DRenderMode(Gfx *gdl)
{
	gDPPipeSync(gdl++);
	gDPSetColorDither(gdl++, G_CD_BAYER);
	gDPSetTexturePersp(gdl++, G_TP_PERSP);
	gDPSetTextureLOD(gdl++, G_TL_LOD);

	return gdl;
}


Gfx* draw_blackbox_to_screen(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx, s32 *lry)
{
    gDPSetRenderMode(glist++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(glist++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(glist++, 0, 0, 0x00, 0x00, 0x00, 0x00);
    gDPFillRectangle(glist++, *ulx, *uly, *lrx, *lry);
    gDPSetRenderMode(glist++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineLERP(glist++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);
    
    return glist;
}


Gfx *gfxDrawTranslucentRect(Gfx *gdl, s32 ulx, s32 uly, s32 lrx, s32 lry, u32 color)
{
    u32 r = (u8)(color >> 0x18);
    u32 g = (u8)(color >> 0x10);
    u32 b = (u8)(color >> 0x8);
    u32 a = (u8)(color >> 0);

    gDPSetRenderMode(gdl++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gdl++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gdl++, 0, 0, r, g, b, a);
    gDPFillRectangle(gdl++, ulx, uly, lrx, lry);
    gDPSetCombineLERP(gdl++, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0, 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0);

    return gdl;
}


/**
 * Render a single character with no outline.
 */
Gfx *textRenderGlyph(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight, s32 yOffset)
{
	s32 kerningOffset;
    s32 drawY;

	drawY = *y + yOffset;
	kerningOffset = font->kerning[prevchar->kerningindex * 13 + curchar->kerningindex] + text_spacing;
    *x -= (kerningOffset - 1);

    if (text_orientation || (*x > 0 && *x <= viGetX() && drawY + curchar->baseline <= viGetY()))
    {
        if (clipX + clipWidth >= *x
				&& clipY + clipHeight >= curchar->baseline + drawY
				&& *x >= clipX
				&& curchar->baseline + drawY + curchar->height >= clipY)
        {
            gDPSetTextureLUT(gdl++, G_TT_NONE);
            gDPLoadTextureBlock(gdl++, curchar->pixeldata, G_IM_FMT_I, G_IM_SIZ_8b, ((curchar->width + 7) & 0xF8), curchar->height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, 0, 0);

            if ((*x + curchar->width) <= clipX + clipWidth)
            {
				if (clipY <= curchar->baseline + drawY)
                {
					if (curchar->baseline + drawY + curchar->height <= clipY + clipHeight)
                    {
                        if (text_orientation)
                        {
                            gSPTextureRectangleFlip(gdl++,
                                /* xl */ (((drawY - curchar->baseline) - curchar->height) * 4) + text_y,
                                /* yl */ ((*x * 4) + text_x),
                                /* xh */ ((drawY - curchar->baseline) * 4) + text_y,
                                /* yh */ ((*x + curchar->width) * 4) + text_x ,
                                /* tile */  G_TX_RENDERTILE,
                                /* s */ text_t,
                                /* t */ ((curchar->height - 1) << 5) + text_s,
                                /* dsdx */ 0x400,
                                /* dsdy */ 0xfc00);
                        }
                        else
                        {
                            gSPTextureRectangle(gdl++,
                                /* xl */ ((*x * 4) + text_x),
                                /* yl */ ((drawY + curchar->baseline) * 4) + text_y,
                                /* xh */ ((*x + curchar->width) * 4) + text_x ,
                                /* yh */ ((curchar->baseline + drawY + curchar->height) * 4) + text_y,
                                /* tile */  G_TX_RENDERTILE,
                                /* s */ text_s,
                                /* t */ text_t,
                                /* dsdx */ 0x400,
                                /* dsdy */ 0x400);
                        }

                    }
                    else if (curchar->baseline + drawY <= clipY + clipHeight)
                    {
                        gSPTextureRectangle(gdl++,
                            /* xl */ ((*x * 4) + text_x),
                            /* yl */ ((drawY + curchar->baseline) * 4) + text_y,
                            /* xh */ ((*x + curchar->width) * 4) + text_x ,
                            /* yh */ clipY + clipHeight + text_y,
                            /* tile */  G_TX_RENDERTILE,
                            /* s */ text_s,
                            /* t */ text_t,
                            /* dsdx */ 0x400,
                            /* dsdy */ 0x400);
                    }
                }
                else
                {
                    if (curchar->baseline + drawY + curchar->height >= clipY)
                    {
                        gSPTextureRectangle(gdl++,
                            /* xl */ ((*x * 4) + text_x),
                            /* yl */ (clipY * 4) + text_y,
                            /* xh */ ((*x + curchar->width) * 4) + text_x ,
                            /* yh */ ((curchar->baseline + drawY + curchar->height) * 4) + text_y,
                            /* tile */  G_TX_RENDERTILE,
                            /* s */ text_s,
                            /* t */ (((clipY - drawY) - curchar->baseline) << 5) + text_t,
                            /* dsdx */ 0x400,
                            /* dsdy */ 0x400);
                    }
                }
            }
        }
    }
    
    *x += curchar->width;
    return gdl;
}


/**
 * Render plain text with no outline e.g. briefings and watch menu text.
 */
Gfx *textRender(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, s32 width, s32 height, u32 yOffset, s32 lineheight)
{
    s32 savedx;
	s32 savedy;
    s32 prevchar;

    savedx = *x;
	savedy = *y;
    prevchar = 'H';

	if (lineheight == 0) 
    {
		lineheight = chars['['].height + chars['['].baseline;
	}

    gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(colour), M_COLOR_G(colour), M_COLOR_B(colour), M_COLOR_A(colour));
    
    while (*text != '\0')
    {
		if (*text == ' ')
        {
			*x += 5;
			prevchar = 'H';
			text++;
		} 
        else if (*text == '\n')
        {
			prevchar = 'H';

            if ((overlap_correction >= 0) && (savedx == *x))
            {
                *y += overlap_correction;
            }
            else
            {
                *y += lineheight;
            }
            
			text++;
			*x = savedx;
		} 
        else
        {
			gdl = textRenderGlyph(gdl, x, y, &chars[*text - 33], &chars[prevchar - 33], font, savedx, savedy, width, height, yOffset);
			prevchar = *text;
			text++;
		}
	}

    return gdl;
}


/**
 * Draw individual characters with an outline.
 */
Gfx *textRenderGlyphOutlined(Gfx *gdl, s32 *x, s32 *y, struct fontchar *curchar, struct fontchar *prevchar, struct font *font, s32 clipX, s32 clipY, u32 textColour, u32 outlineColour, s32 clipWidth, s32 clipHeight, s32 yOffset)
{
    s32 offsetX;
    s32 offsetY;
    s32 kerningOffset;
    s32 drawY;
    s32 tmpx;
    s32 tmpy;
    u32 rgba;

    drawY = *y + yOffset;

    kerningOffset = font->kerning[prevchar->kerningindex * 13 + curchar->kerningindex] + text_spacing;
    *x -= (kerningOffset - 1);

    if (*x > 0
            && *x <= viGetX()
            && drawY + curchar->baseline <= viGetY()
            && *x <= clipX + clipWidth
            && drawY + curchar->baseline <= clipY + clipHeight
            && *x >= clipX
            && drawY + curchar->baseline + curchar->height >= clipY) {
        gDPSetTextureLUT(gdl++, G_TT_NONE);
        gDPLoadTextureBlock(gdl++, curchar->pixeldata, G_IM_FMT_I, G_IM_SIZ_8b, ((curchar->width + 7) & 0xF8), curchar->height, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, 0, 0);

        /**
         * Render outlines.
         * This is done by rendering the character 8 times in diffent offsets around the origin:
         * left/right/up/down and the four diagonals.
         */
        rgba = outlineColour;
        gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(rgba), M_COLOR_G(rgba), M_COLOR_B(rgba), M_COLOR_A(rgba));

        for (offsetX = -1; offsetX < 2; offsetX++)
        {
            for (offsetY = -1; offsetY < 2; offsetY++)
            {
                tmpx = *x + offsetX;
                tmpy = drawY + offsetY;

                // Skip the center since that will be covered by the main character anyway.
                if (offsetX != 0 || offsetY != 0)
                { 
                    gdl = textDrawGlyphQuad(gdl, tmpx, tmpy, curchar, clipX, clipY, clipWidth, clipHeight);
                }
            }
        }

        // Render main character.
        tmpx = *x;
        tmpy = drawY;
        rgba = textColour;

        gDPSetPrimColor(gdl++, 0, 0, M_COLOR_R(rgba), M_COLOR_G(rgba), M_COLOR_B(rgba), M_COLOR_A(rgba));
        gdl = textDrawGlyphQuad(gdl, tmpx, tmpy, curchar, clipX, clipY, clipWidth, clipHeight);
    }

    *x += curchar->width;

    return gdl;
}


/**
 * Draw a quad for the current glyph, clipping it inside the region defined by clipX, clipY, clipWidth, and clipHeight.
 */
Gfx *textDrawGlyphQuad(Gfx *gdl, s32 x, s32 y, struct fontchar *curchar, s32 clipX, s32 clipY, s32 clipWidth, s32 clipHeight)
{
    // Check if the glyph's right edge is inside or touching the clipping rect's right edge.
    if (clipX + clipWidth >= curchar->width + x)
    {
        // Check if the glyph's top is below the clip top.
        if (curchar->baseline + y >= clipY)
        {
            // Check if the glyph's bottom is above the clip bottom.
            if (clipY + clipHeight >= curchar->baseline + y + curchar->height)
            {
                // Check if vertical text. Not sure if ever used for outlined text?
                if (text_orientation)
                {
                    gSPTextureRectangleFlip(gdl++, 
                        /* xl */ ((y - curchar->baseline) - curchar->height) << 2, 
                        /* yl */ x << 2, 
                        /* xh */ (y - curchar->baseline) << 2, 
                        /* yh */ (x + curchar->width) << 2, 
                        /* tile */ G_TX_RENDERTILE, 
                        /* s */ 0, 
                        /* t */ (curchar->height - 1) << 5, 
                        /* dsdx */ 0x400, 
                        /* dsdy */ 0xFC00);
                }
                else
                {
                    gSPTextureRectangle(gdl++, 
                        /* xl */ x << 2, 
                        /* yl */ (y + curchar->baseline) << 2, 
                        /* xh */ (x + curchar->width << 2), 
                        /* yh */ (curchar->baseline + y + curchar->height) << 2, 
                        /* tile */ G_TX_RENDERTILE, 
                        /* s */ 0, 
                        /* t */ 0, 
                        /* dsdx */ 0x400, 
                        /* dsdy */ 0x400);
                }
            }
            // Glyph bottom is below the clipping rectangle.
            else if (clipY + clipHeight >= curchar->baseline + y)
            {
                // Bottom coordinates clipped to savedy + height.
                gSPTextureRectangle(gdl++, 
                    /* xl */ x << 2, 
                    /* yl */ (y + curchar->baseline) << 2, 
                    /* xh */ (x + curchar->width) << 2, 
                    /* yh */ (clipY + clipHeight << 2), 
                    /* tile */ G_TX_RENDERTILE, 
                    /* s */ 0, 
                    /* t */ 0, 
                    /* dsdx */ 0x400, 
                    /* dsdy */ 0x400); 
            }
        }
        // Glyph top is above the clipping rectangle.
        else if (curchar->baseline + y + curchar->height >= clipY) 
        {
            gSPTextureRectangle(gdl++, // Top clipped to fit.
                /* xl */ x << 2, 
                /* yl */ clipY<< 2, 
                /* xh */ (x + curchar->width) << 2, 
                /* yh */ (curchar->baseline + y + curchar->height) << 2, 
                /* tile */ G_TX_RENDERTILE, 
                /* s */ 0, 
                /* t */ ((clipY - curchar->baseline) - y) << 5, 
                /* dsdx */ 0x400, 
                /* dsdy */ 0x400);
        }
    }

    return gdl;
}


/**
 * Draw strings of text with an outline.
 * Used for ammo counter, bottom left HUD messages, countdown timers.
 */
Gfx *textRenderOutlined(Gfx *gdl, s32 *x, s32 *y, char *text, struct fontchar *chars, struct font *font, u32 colour, u32 colour2, s32 width, s32 height, s32 yOffset, s32 lineheight)
{
	s32 savedy;
    s32 savedx;
    s32 prevchar;

    savedx = *x;
	savedy = *y;
    prevchar = 'H';

	if (lineheight == 0)
    {
		lineheight = chars['['].height + chars['['].baseline;
	}
    
    while (*text != '\0')
    {
		if (*text == ' ')
        {
			*x += SPACE_WIDTH;
			prevchar = 'H';
			text++;
		} 
        else if (*text == '\n')
        {
			prevchar = 'H';
			*x = savedx;
            *y += lineheight;
			text++;
		}
        else
        {
            // Render individual characters with outline.
            gdl = textRenderGlyphOutlined(gdl, x, y, &chars[*text - 0x21], &chars[prevchar - 0x21], font, savedx, savedy, colour, colour2, width, height, yOffset);

			prevchar = *text;
			text++;
		}
	}

    return gdl;
}


void textMeasure(s32 *textheight, s32 *textwidth, char *text, struct fontchar *font1, struct font *font2, s32 lineheight)
{
    char prevchar;
	s32 longest;
	s32 tmp;

	prevchar = 'H';
    tmp = 0;
	longest = 0;
	*textheight = 0;
	*textwidth = 0;

    if (lineheight == 0)
    {
		lineheight = font1['['].baseline + font1['['].height;
    }

    while (*text != '\0')
    {
        if (*text == ' ')
        {
            // Space
            if (text[1] != '\n')
            {
                *textwidth += SPACE_WIDTH;
            }

            prevchar = 'H';
            text++;
        }
        else if (*text == '\n')
        {
            // Line break
            if (*textwidth > longest)
            {
                longest = *textwidth;
            }

            *textwidth = 0;
            *textheight += lineheight;
            text++;
        }
        else
        {
            tmp = font2->kerning[font1[prevchar - 0x21].kerningindex * 13 + font1[*text - 0x21].kerningindex] + text_spacing - 1;
            *textwidth = font1[*text - 0x21].width + *textwidth - tmp;

            prevchar = *text;
            text++;
        }
    };
    
    if (*textwidth < longest)
    {
        *textwidth = longest;
    }
}


void textWrap(s32 wrapwidth, char *src, char *dst, struct fontchar *chars, struct font *font)
{
	s32 curlinewidth = 0;
	bool itfits;
	s32 wordlen;
	s32 wordwidth;
	s32 wordheight = 0;
	s32 more = 1;
	s32 i;
	char curword[32];

    while (more == 1)
    {
		// Load the next word
		wordwidth = 0;
		wordlen = 0;

        // Read one word into the curword buffer.
        while (*src > ' ')
        {
            curword[wordlen] = *src;
			src++;
			wordlen++;

        }
        
        // Null terminate the word
        curword[wordlen] = '\0';

        textMeasure(&wordheight, &wordwidth, curword, chars, font, 0);

        // Track the current line's pixel length
        curlinewidth += wordwidth;

        if (curlinewidth <= wrapwidth)
        {
            itfits = 1;
        } 
        else
        {
            itfits = 0;
        }

        if (*src == '\n')
        {
            // Write a new line and indent
			if (!itfits)
            {
				*dst = '\n';
				dst++;

				for (i = 0; i < text_wordwrap; i++)
                {
					*dst = ' ';
					dst++;
				}
			}

			curlinewidth = 0;

			// Write the current word
			for (i = 0; i < wordlen; i++)
            {
				*dst = curword[i];
				dst++;
			}

			// Write the original new line that was in src
			*dst = '\n';
			dst++;
        }
        else if (*src == ' ')
        {
            if (!itfits)
            {
				// Write a new line and indent
				*dst = '\n';
				dst++;

				for (i = 0; i < text_wordwrap; i++)
                {
					*dst = ' ';
					dst++;
				}

				curlinewidth = text_wordwrap * SPACE_WIDTH + wordwidth;
			}

			curlinewidth += SPACE_WIDTH;

			// Write the current word
			for (i = 0; i < wordlen; i++)
            {
				*dst = curword[i];
				dst++;
			}

			// Write the trailing space
			*dst = ' ';
			dst++;
        }
        else if (*src == '\0')
        {
            more = 0;

			if (!itfits)
            {
				// Write a new line and indent
				*dst = '\n';
				dst++;

				for (i = 0; i < text_wordwrap; i++)
                {
					*dst = ' ';
					dst++;
				}
			}

			// Write the current word
			for (i = 0; i < wordlen; i++)
            {
				*dst = curword[i];
				dst++;
			}

			// Write the null terminator
			*dst = '\0';
        }

        src++;
    }
}
