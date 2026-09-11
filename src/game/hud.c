#include <ultra64.h>
#include <limits.h>
#include "bg.h"
#include "bgroomtrans.h"
#include "bondview.h"
#include "cam.h"
#include "dyn.h"
#include "gbi_extension.h"
#include "glass.h"
#include "hud.h"
#include "image_bank.h"
#include "lv.h"
#include "math_atan2f.h"
#include "matrixmath.h"
#include "objective_status.h"
#include "random.h"


#define G_CC_MODULATEIA_ENV COMBINED, 0, ENVIRONMENT, 0, COMBINED, 0, ENVIRONMENT, 0

#define GAUGE_BAR_VERTEX_PAIR_STRIDE (2 * sizeof(struct damage_display_val))


/**
 * Draws a tinted image using the texture already selected by the caller.
 * position is the screen-space center; halfSize contains its half-width and
 * half-height in pixels. textureWidth and textureHeight are measured in texels.
 * swapAxes exchanges the texture axes; flipS and flipT mirror those axes.
 * mipmapped selects the trilinear combiner, taking precedence over highlight.
 * Advances *gdlptr and restores perspective texturing after the rectangle.
 */
void hudDrawImage(Gfx **gdlptr, f32 *position, f32 *halfSize, s32 textureWidth, s32 textureHeight,
    s32 swapAxes, s32 flipS, s32 flipT, s32 red, s32 green, s32 blue, s32 alpha,
    s32 mipmapped, s32 highlight)
{
    // Empty or inverted rectangles emit no commands.
    if (halfSize[0] > 0.0f && halfSize[1] > 0.0f)
    {
        Gfx *gdl = *gdlptr;
        s32 xl;
        s32 yl;
        s32 xh;
        s32 yh;
        s32 s = 0;
        s32 t = 0;
        s32 dsdx;
        s32 dtdy;

        gDPSetEnvColor(gdl++, red, green, blue, alpha);

        if (mipmapped)
        {
            gDPSetCombineMode(gdl++, G_CC_TRILERP, G_CC_MODULATEIA_ENV);
        }
        else if (highlight)
        {
            gDPSetCombineMode(gdl++, G_CC_FADEA, G_CC_PASS2);
        }
        else
        {
            gDPSetCombineMode(gdl++, G_CC_FADEA, G_CC_FADEA);
        }

        // Disable texture perspective correction
        gDPSetTexturePersp(gdl++, G_TP_NONE);

        // Compute rectangle coordinates
        xl = (position[0] - halfSize[0]) * 4.0f;
        yl = (position[1] - halfSize[1]) * 4.0f;
        xh = (position[0] + halfSize[0]) * 4.0f;
        yh = (position[1] + halfSize[1]) * 4.0f;

        // Skip rectangles entirely above or to the left of the screen.
        if (xh >= 0 && yh >= 0)
        {
            // Clip the left edge and advance the matching texture coordinate.
            if (xl < 0)
            {
                if (swapAxes)
                {
                    t += ((-xl * textureHeight) << 5) / (xh - xl);
                }
                else
                {
                    s += ((-xl * textureWidth) << 5) / (xh - xl);
                }

                xl = 0;
            }

            // Clip the top edge and advance the matching texture coordinate.
            if (yl < 0)
            {
                if (swapAxes)
                {
                    s += ((-yl * textureWidth) << 5) / (yh - yl);
                }
                else
                {
                    t += ((-yl * textureHeight) << 5) / (yh - yl);
                }

                yl = 0;
            }

            // Swapping axes maps S vertically and T horizontally.
            if (swapAxes)
            {
                dsdx = textureWidth / (2.0f * halfSize[1]) * 1024.0f;
                dtdy = textureHeight / (2.0f * halfSize[0]) * 1024.0f;
            }
            else
            {
                dsdx = textureWidth / (2.0f * halfSize[0]) * 1024.0f;
                dtdy = textureHeight / (2.0f * halfSize[1]) * 1024.0f;
            }

            // Mirror along the texture S axis.
            if (flipS)
            {
                dsdx = 0x10000 - dsdx;
                s = ((textureWidth - 1) << 5) - s;
            }

            // Mirror along the texture T axis.
            if (flipT)
            {
                dtdy = 0x10000 - dtdy;
                t = ((textureHeight - 1) << 5) - t;
            }

            // Draw the textured rectangle with optional flipping
            if (swapAxes)
            {
                gSPTextureRectangleFlip(gdl++, xl, yl, xh, yh, G_TX_RENDERTILE, s, t, dsdx, dtdy);
            }
            else
            {
                gSPTextureRectangle(gdl++, xl, yl, xh, yh, G_TX_RENDERTILE, s, t, dsdx, dtdy);
            }
        }

        // Re-enable texture perspective correction
        gDPSetTexturePersp(gdl++, G_TP_PERSP);

        *gdlptr = gdl;
    }
}

/*
  Render Health Bars
  AI Comment: This function populates a radial array of HUD elements (HealthSegments) with position and color data
  based on a damage value (HealthValue) and a display mode (isArmour). It loops through 23 segments, calculating their
  screen-space coordinates and visual properties using trigonometric functions.
  @healthSegments : A pointer to an Array of 46 vertices.
  @isArmour : Armour/Health if positive/negative
  @numsegments : Not Used
  @HealthValue : amount of health/armour 0-10
*/
void hudMakeDamageSegments(struct damage_display_val *HealthSegments, s32 numSegments, s32 isArmour, f32 HealthValue)
{
	s32 unused;
    s32 i;
    s32 pairIndex;
    f32 angleRadians;

    HealthValue *= 8;


    //for 145.2 to 35.2 degrees, calculate health/armour
    for (i=0; i<23; i++)
    {
        //This line calculates an angle in radians, starting from 142° (cast truncated) and decreasing by 5° per iteration.
        angleRadians = ((f32) (s32)(142.5 - (i*5))* M_PI_F * 2) / 360;

        for (pairIndex = 0; pairIndex < 2; pairIndex++)
        {
            s16 radialOffsetX = (((sinf(angleRadians) * 4 * 130 * (6 - pairIndex)) / 5) * isArmour);
            s16 radialOffsetZ = (((cosf(angleRadians) * 4) * 130 * (6 - pairIndex)) / 5);

            HealthSegments->pos.x    = (radialOffsetX + 1);
            HealthSegments->pos.y    = 0;
            HealthSegments->pos.z    = -radialOffsetZ;
            HealthSegments->normal.x = 0;
            HealthSegments->normal.y = 0;
            HealthSegments->normal.z = 0;
            HealthSegments->colour.r = 255;
            HealthSegments->colour.g = 255;
            HealthSegments->colour.b = 255;

            if (isArmour >= TRUE) //armour shade
            {
                HealthSegments->colour.r = (int)(96 - (cosf(angleRadians) * 96));
                HealthSegments->colour.g = (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = 255;
            }
            else if (isArmour < FALSE) //health shade
            {
                HealthSegments->colour.g =  (int)(127 - (cosf(angleRadians) * 127));
                HealthSegments->colour.b = (int)(32 - (cosf(angleRadians) * 32));
            }

            // segments 0-9 are single, 10-22 are doubled with single gaps
            // IF i < damage fill, if fractional, shade else no fill.
            if (i < 10) //145 to 95
            {
                //full shade
                if (i <= (((int)HealthValue * 2) - 1))
                {
                    HealthSegments->colour.a = 255;
                }
                // Fraction fill
                else if ((i < (int)(HealthValue * 2.0f)) && (i > (((int)HealthValue * 2) - 1))) //yes, it looks like one is float, the other is cast
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else //no fill
                {
                    HealthSegments->colour.a = 48;
                }
            }
            else if (i >= 10) //95 to 35
            {
                if ( i <= (9 + ((HealthValue - 5.0f) * 4)))
                {
                    HealthSegments->colour.a = 255;
                }
                else if (i <= (((int)(((HealthValue - 5.0f) * 4) + 0.5f) + 9)) && (i > (((int)(HealthValue - 5.0f) * 2) + 8)))
                {
                    HealthSegments->colour.a = (int)((HealthValue - (int)HealthValue) * 207) + 48;
                }
                else
                {
                    HealthSegments->colour.a = 48;
                }
            }
            HealthSegments++;
        }
    }
}


/**
 * Creates the display list for HUD and watch health and armor bars.
 */
Gfx *hudBuildGaugeBar(Gfx *gdl, uintptr_t vtxaddr, s32 numvertices)
{
    s8 i;

    for (i = 0; i <= (numvertices / 2 - 2); i++) 
    {
        gSPVertex(gdl++, vtxaddr, 4, 0);

        if (i >= 9) 
        {
            if ((i + 3) % 4) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        } 
        else if (i < 9) 
        {
            if ((i & 1) == 0) 
            {
                gSP2Triangles(gdl++, 0, 1, 2, 0, 1, 2, 3, 0);
            }
        }

        vtxaddr += GAUGE_BAR_VERTEX_PAIR_STRIDE;
    }

    gSPEndDisplayList(gdl++);

    return gdl;
}
