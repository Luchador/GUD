#include <string.h>
#include <src/propconstants.h>
#include "objectshade.h"

void ObjectShadeFromTile(const StanFile *stan, DWORD tile, BOOL character,
                         DWORD objectflags, ObjectShade *out)
{
    unsigned int rgb[3], brightness, minimum;
    memset(out, 0, sizeof(*out));
    if (!stan || !stan->tiles || tile >= stan->tilecount
        || (!character && (objectflags & PROPFLAG_ILLUMINATED))) { return; }

    /* StanTile already expands each native four-bit component to eight bits.
     * Keep these integer operations in step with propobj.c:objSetColorFromTile. */
    rgb[0] = stan->tiles[tile].red;
    rgb[1] = stan->tiles[tile].green;
    rgb[2] = stan->tiles[tile].blue;
    brightness = (rgb[0]*5 + rgb[1]*10 + rgb[2]) >> 4;
    out->rgba[3] = ((255 - brightness)*3) >> 2;
    minimum = rgb[0] < rgb[1] ? rgb[0] : rgb[1];
    if (rgb[2] < minimum) { minimum = rgb[2]; }
    for (int channel = 0; channel < 3; channel++)
    {
        unsigned int tint = (rgb[channel] - minimum) >> 1;
        if (!character)
        {
            /* objSetShading halves prop tint again. objRenderProp applies
             * this 255/256 factor for an undamaged object (zero shots taken). */
            tint = ((tint >> 1)*255) >> 8;
        }
        out->rgba[channel] = tint;
    }
}

void ObjectShadeVertices(BgVertex *vertices, DWORD count, const ObjectShade *shade)
{
    unsigned int strength = shade->rgba[3];
    if (!strength) { return; }
    for (DWORD i = 0; i < count; i++)
    {
        BgVertex *v = &vertices[i];
        v->r = (v->r*(255-strength) + shade->rgba[0]*strength + 127)/255;
        v->g = (v->g*(255-strength) + shade->rgba[1]*strength + 127)/255;
        v->b = (v->b*(255-strength) + shade->rgba[2]*strength + 127)/255;
    }
}
