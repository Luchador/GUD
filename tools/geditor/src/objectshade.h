#ifndef GEDITOR_OBJECTSHADE_H
#define GEDITOR_OBJECTSHADE_H

#include "bgload.h"
#include "stanload.h"

/* Preview-only counterpart of the game's fog-colour tile shading. Alpha here
 * is the blend strength, never object opacity. Zero is neutral shading. */
typedef struct ObjectShade { unsigned char rgba[4]; } ObjectShade;

void ObjectShadeFromTile(const StanFile *stan, DWORD tile, BOOL character,
                         DWORD objectflags, ObjectShade *out);
/* Blend into placed vertex copies, never into cached or exported model data.
 * The game applies this blend after texture modulation in G_RM_FOG_PRIM_A;
 * the editor approximates it on vertex RGB before texture modulation. */
void ObjectShadeVertices(BgVertex *vertices, DWORD count, const ObjectShade *shade);

#endif
