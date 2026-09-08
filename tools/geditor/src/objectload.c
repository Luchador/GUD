/* Builds the editor's placed-model layer from setup records and project
   glTF files. Character body/head assembly is handled by characterload. */

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <src/propconstants.h>

#include "modelload.h"
#include "objectload.h"
#include "characterload.h"
#include "romexport.h"

#define OBJECT_MODEL_CACHE_COUNT 512

#define PAD_BOUND_BASE 10000

typedef struct ModelCacheEntry {
    BOOL attempted;
    BgVertex *tris;
    unsigned short *tritags;
    DWORD tricount;
    float scale;
    float min[3], max[3];
} ModelCacheEntry;

typedef struct ObjectBuilder {
    BgVertex *tris;
    unsigned short *tritags;
    DWORD *objectindices;
    DWORD tricount;
    DWORD capacity;
    BOOL failed;
} ObjectBuilder;

typedef struct ObjectBasis {
    float side[3];
    float up[3];
    float look[3];
    float pos[3];
} ObjectBasis;

/* Previously activated solid objects can support later objects in the
   same stan room. The game tests their projected collision boxes. */
typedef struct ObjectSupport {
    float points[8][2];
    int pointcount;
    float bottom, top;
    unsigned char room;
} ObjectSupport;

static BOOL ObjectNormalize(float vector[3])
{
    float length = sqrtf(vector[0] * vector[0]
                       + vector[1] * vector[1]
                       + vector[2] * vector[2]);

    if (!(length > 0.000001f))
    {
        return FALSE;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;
    return TRUE;
}

/* Recreates matrix_4x4_set_basis_and_position_target(-look, up):
   local model X/Y/Z become pad side/up/look respectively. */
static BOOL ObjectMakeBasis(const SetupPad *pad, float worldscale,
                            ObjectBasis *basis)
{
    int axis;

    for (axis = 0; axis < 3; axis++)
    {
        basis->look[axis] = pad->look[axis];
        basis->up[axis] = pad->up[axis];
        basis->pos[axis] = pad->pos[axis] * worldscale;
    }

    if (!ObjectNormalize(basis->look))
    {
        return FALSE;
    }

    basis->side[0] = basis->up[1] * basis->look[2]
                   - basis->up[2] * basis->look[1];
    basis->side[1] = basis->up[2] * basis->look[0]
                   - basis->up[0] * basis->look[2];
    basis->side[2] = basis->up[0] * basis->look[1]
                   - basis->up[1] * basis->look[0];

    if (!ObjectNormalize(basis->side))
    {
        return FALSE;
    }

    /* The game recomputes an orthogonal up vector after the cross. */
    basis->up[0] = basis->look[1] * basis->side[2]
                 - basis->look[2] * basis->side[1];
    basis->up[1] = basis->look[2] * basis->side[0]
                 - basis->look[0] * basis->side[2];
    basis->up[2] = basis->look[0] * basis->side[1]
                 - basis->look[1] * basis->side[0];

    return ObjectNormalize(basis->up);
}

/* Apply the authored placement used by objPlaceOnSideAtPad/objPlaceAtPad.
   Run after finding the bound-pad center: its anchor uses the original pad
   axes, while the model axes below may be rotated. Doors use setupDoor's
   separate transform and must not pass through here. */
static void ObjectApplyPlacementFlags(DWORD flags, const SetupBoundPad *bound,
                                      float worldscale, const float min[3],
                                      const float max[3], ObjectBasis *basis,
                                      float modelcenter[3])
{
    int axis;

    /* Placement anchors to the lower Y face of a bound pad,
       or the authored pad position when there are no bounds. */
    if (bound != NULL)
    {
        float offset = (bound->ymin - bound->ymax) * 0.5f * worldscale;

        for (axis = 0; axis < 3; axis++)
        {
            basis->pos[axis] += bound->pad.up[axis] * offset;
        }
    }

    if (flags & PROPFLAG_ONSIDE)
    {
        /* The setup meaning of this flag is rotate X by 270 degrees, then
           Y by 180 degrees: model X/Y/Z map to -side/look/up. The Y/Z
           fitting dimensions have already been swapped before this step.
           This mode takes precedence over UPSIDEDOWN, as in the game. */
        for (axis = 0; axis < 3; axis++)
        {
            float padup = basis->up[axis];

            basis->side[axis] = -basis->side[axis];
            basis->up[axis] = basis->look[axis];
            basis->look[axis] = padup;
        }
        modelcenter[2] = min[2];
    }
    else if (flags & PROPFLAG_UPSIDEDOWN)
    {
        for (axis = 0; axis < 3; axis++)
        {
            basis->side[axis] = -basis->side[axis];
            basis->up[axis] = -basis->up[axis];
        }
        modelcenter[1] = max[1];
    }
    else
    {
        modelcenter[1] = min[1];
    }
}

static void ObjectModelBounds(const BgVertex *tris, DWORD tricount,
                              float min[3], float max[3])
{
    DWORD i;

    min[0] = max[0] = tris[0].x;
    min[1] = max[1] = tris[0].y;
    min[2] = max[2] = tris[0].z;

    for (i = 1; i < tricount * 3; i++)
    {
        const float value[3] = { tris[i].x, tris[i].y, tris[i].z };
        int axis;

        for (axis = 0; axis < 3; axis++)
        {
            if (value[axis] < min[axis]) min[axis] = value[axis];
            if (value[axis] > max[axis]) max[axis] = value[axis];
        }
    }
}

static BOOL ObjectBuilderReserve(ObjectBuilder *builder, DWORD add)
{
    DWORD needed;
    DWORD capacity;
    BgVertex *tris;
    unsigned short *tags;
    DWORD *objectindices;

    if (builder->failed || add > 0xffffffffu - builder->tricount)
    {
        builder->failed = TRUE;
        return FALSE;
    }

    needed = builder->tricount + add;
    if (needed <= builder->capacity)
    {
        return TRUE;
    }

    capacity = builder->capacity != 0 ? builder->capacity : 1024;
    while (capacity < needed)
    {
        if (capacity > 0x7fffffffu)
        {
            capacity = needed;
            break;
        }
        capacity *= 2;
    }

    tris = (BgVertex *)realloc(builder->tris,
        (size_t)capacity * 3 * sizeof(*builder->tris));
    if (tris == NULL)
    {
        builder->failed = TRUE;
        return FALSE;
    }
    builder->tris = tris;

    tags = (unsigned short *)realloc(builder->tritags,
        (size_t)capacity * sizeof(*builder->tritags));
    if (tags == NULL)
    {
        builder->failed = TRUE;
        return FALSE;
    }
    builder->tritags = tags;

    objectindices = (DWORD *)realloc(builder->objectindices,
        (size_t)capacity * sizeof(*builder->objectindices));
    if (objectindices == NULL)
    {
        builder->failed = TRUE;
        return FALSE;
    }
    builder->objectindices = objectindices;
    builder->capacity = capacity;
    return TRUE;
}

static void ObjectPlaceModel(ObjectBuilder *builder,
                             const ModelCacheEntry *model,
                             const ObjectBasis *basis,
                             const float scale[3], BOOL door,
                             const float modelcenter[3],
                             DWORD objectindex)
{
    DWORD outfirst;
    DWORD tri;

    if (!ObjectBuilderReserve(builder, model->tricount))
    {
        return;
    }

    outfirst = builder->tricount;

    for (tri = 0; tri < model->tricount; tri++)
    {
        int corner;

        builder->tritags[outfirst + tri] =
            (unsigned short)(model->tritags[tri] | BG_TRI_OBJECT);
        builder->objectindices[outfirst + tri] = objectindex;

        for (corner = 0; corner < 3; corner++)
        {
            const BgVertex *source = &model->tris[tri * 3 + corner];
            BgVertex *dest = &builder->tris[(outfirst + tri) * 3 + corner];
            float local[3];
            int axis;

            local[0] = (source->x - modelcenter[0]) * scale[0];
            local[1] = (source->y - modelcenter[1]) * scale[1];
            local[2] = (source->z - modelcenter[2]) * scale[2];
            *dest = *source;

            for (axis = 0; axis < 3; axis++)
            {
                if (door)
                {
                    /* setupDoor rotates X then Z before applying the
                       pad basis: model X/Y/Z map to up/look/side. */
                    (&dest->x)[axis] = basis->pos[axis]
                        + basis->up[axis] * local[0]
                        + basis->look[axis] * local[1]
                        + basis->side[axis] * local[2];
                }
                else
                {
                    (&dest->x)[axis] = basis->pos[axis]
                        + basis->side[axis] * local[0]
                        + basis->up[axis] * local[1]
                        + basis->look[axis] * local[2];
                }
            }
        }
    }

    builder->tricount += model->tricount;
}

static ModelCacheEntry *ObjectGetModel(ModelCacheEntry *cache, int modelid,
                                       const char *projectdir, const RomFile *rom)
{
    ModelCacheEntry *entry;
    const char *why = "";

    if (modelid < 0 || modelid >= OBJECT_MODEL_CACHE_COUNT)
    {
        return NULL;
    }

    entry = &cache[modelid];
    if (!entry->attempted)
    {
        entry->attempted = TRUE;
        entry->tris = ModelLoadProjectGeometry(projectdir, modelid,
            &entry->tricount, &entry->tritags, &entry->scale, &why);
        if (entry->tris != NULL && entry->tricount > 0)
        {
            const char *name;
            DWORD offset, size;

            ObjectModelBounds(entry->tris, entry->tricount, entry->min, entry->max);
            /* Project glTFs currently retain render geometry, not gameplay
               boxes. Read the original box from the retained import ROM;
               older projects without it keep mesh bounds as a fallback. */
            if (rom->data != NULL && ModelGetPropDefinition(modelid, &name, NULL)
                && RomFindFile(rom, name, &offset, &size, &why))
            {
                float min[3], max[3];

                if (ModelReadPlacementBounds(rom->data + offset, size, min, max))
                {
                    memcpy(entry->min, min, sizeof(min));
                    memcpy(entry->max, max, sizeof(max));
                }
            }
        }
    }

    return entry->tris != NULL && entry->tricount > 0 ? entry : NULL;
}

static float ObjectCross2D(const float a[2], const float b[2], const float c[2])
{
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
}

static int ObjectComparePoints(const void *left, const void *right)
{
    const float *a = (const float *)left, *b = (const float *)right;

    if (a[0] != b[0]) { return a[0] < b[0] ? -1 : 1; }
    return a[1] < b[1] ? -1 : a[1] > b[1];
}

static void ObjectBuildSupport(ObjectSupport *support, const ObjectBasis *basis,
                                const float scale[3], const float center[3],
                                const float min[3], const float max[3],
                                unsigned char room, BOOL aircraft)
{
    float points[8][2], hull[16][2];
    int corner, count = 0, lowercount;

    support->room = room;
    for (corner = 0; corner < 8; corner++)
    {
        float local[3], world[3];
        int axis;

        for (axis = 0; axis < 3; axis++)
        {
            local[axis] = (((corner >> axis) & 1 ? max[axis] : min[axis]) - center[axis]) * scale[axis];
        }
        for (axis = 0; axis < 3; axis++)
        {
            world[axis] = basis->pos[axis] + basis->side[axis] * local[0]
                        + basis->up[axis] * local[1] + basis->look[axis] * local[2];
        }
        points[corner][0] = world[0]; points[corner][1] = world[2];
        if (corner == 0 || world[1] < support->bottom) { support->bottom = world[1]; }
        if (corner == 0 || world[1] > support->top) { support->top = world[1]; }
    }
    if (aircraft) { support->bottom -= 200.0f; }
    /* Convex hull of the eight projected corners, as in the runtime's
       collisionCalcFootprintFromBBox; retain sloped/rotated boxes too. */
    qsort(points, 8, sizeof(points[0]), ObjectComparePoints);
    for (corner = 0; corner < 8; corner++)
    {
        while (count >= 2 && ObjectCross2D(hull[count - 2], hull[count - 1], points[corner]) <= 0.0f) { count--; }
        memcpy(hull[count++], points[corner], sizeof(points[corner]));
    }
    lowercount = count + 1;
    for (corner = 6; corner >= 0; corner--)
    {
        while (count >= lowercount && ObjectCross2D(hull[count - 2], hull[count - 1], points[corner]) <= 0.0f) { count--; }
        memcpy(hull[count++], points[corner], sizeof(points[corner]));
    }
    support->pointcount = count > 3 ? count - 1 : 0;
    memcpy(support->points, hull, (size_t)support->pointcount * sizeof(hull[0]));
}

static float ObjectFindSupportHeight(const ObjectSupport *supports, DWORD count,
                                      unsigned char room, const float reference[3],
                                      float ground, float height)
{
    float point[2] = { reference[0], reference[2] };

    /* chrpropFindObjectContainingPointInRoom walks active props newest first
       and stops at the first containing polygon, even if its Y test fails. */
    while (count > 0)
    {
        const ObjectSupport *support = &supports[--count];
        int edge;

        if (support->room != room || support->pointcount < 3) { continue; }
        for (edge = 0; edge < support->pointcount; edge++)
        {
            if (ObjectCross2D(support->points[edge],
                              support->points[(edge + 1) % support->pointcount], point) < 0.0f)
            {
                break;
            }
        }
        if (edge == support->pointcount)
        {
            if (ground < support->top && support->bottom < ground + height + 4.0f)
            {
                return support->top;
            }
            break;
        }
    }
    return ground + 4.0f;
}

BOOL ObjectLoadSetupGeometry(const char *projectdir, const SetupFile *setup,
                             const StanFile *stan, float levelscale,
                             SetupObjectGeometry *out,
                             const char **reasonout)
{
    ModelCacheEntry cache[OBJECT_MODEL_CACHE_COUNT];
    ObjectBuilder builder;
    SetupObjectGeometry characters;
    float worldscale;
    RomFile rom;
    char basepath[MAX_PATH];
    int pathlength;
    const char *basewhy = "";
    ObjectSupport *supports = NULL;
    DWORD supportcount = 0;
    DWORD *padtiles = NULL;
    BOOL hasstan = stan != NULL && stan->tiles != NULL && stan->tilecount > 0;
    DWORD i;

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(cache, sizeof(cache));
    ZeroMemory(&builder, sizeof(builder));
    ZeroMemory(&characters, sizeof(characters));
    ZeroMemory(&rom, sizeof(rom));
    *reasonout = "";

    if (setup == NULL || projectdir == NULL || !(levelscale > 0.0f))
    {
        return TRUE;
    }

    if (setup->padcount > 0)
    {
        out->occupiedpads = (unsigned char *)calloc(setup->padcount, 1);
    }
    if (setup->boundpadcount > 0)
    {
        out->occupiedboundpads =
            (unsigned char *)calloc(setup->boundpadcount, 1);
    }
    if ((setup->padcount > 0 && out->occupiedpads == NULL)
        || (setup->boundpadcount > 0 && out->occupiedboundpads == NULL))
    {
        *reasonout = "out of memory tracking object pads.";
        goto fail;
    }

    worldscale = 1.0f / levelscale;

    if (setup->objectcount > 0)
    {
        supports = (ObjectSupport *)calloc(setup->objectcount, sizeof(*supports));
        if (supports == NULL)
        {
            *reasonout = "out of memory tracking object support surfaces.";
            goto fail;
        }
    }
    if (hasstan && setup->padcount + setup->boundpadcount > 0)
    {
        DWORD count = setup->padcount + setup->boundpadcount;

        padtiles = (DWORD *)malloc((size_t)count * sizeof(*padtiles));
        if (padtiles == NULL)
        {
            *reasonout = "out of memory resolving object pads.";
            goto fail;
        }
        for (i = 0; i < count; i++) { padtiles[i] = (DWORD)-2; }
    }
    pathlength = snprintf(basepath, sizeof(basepath), "%s\\%s", projectdir,
                           ROM_EXPORT_BASE_FILENAME);
    if (pathlength >= 0 && pathlength < (int)sizeof(basepath))
    {
        RomLoad(basepath, &rom, &basewhy);
    }

    for (i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *object = &setup->objects[i];
        const SetupPad *pad;
        const SetupBoundPad *bound = NULL;
        ModelCacheEntry *model;
        ObjectBasis basis;
        float min[3], max[3], center[3] = { 0.0f, 0.0f, 0.0f };
        float scale[3];
        float reference[3];
        DWORD referencetile = STAN_TILE_NONE, placedtile;
        int padindex;
        BOOL isbound;
        BOOL isdoor = object->type == PROPDEF_DOOR;

        if (object->deleted
            || (object->flags & (PROPFLAG_ASSIGNEDTOCHR
                              | PROPFLAG_INSIDEANOTHEROBJ)) != 0
            || object->pad < 0)
        {
            continue;
        }

        if (isdoor)
        {
            isbound = TRUE;
            padindex = object->pad; /* DoorRecord uses the raw bound index. */
        }
        else if (object->pad >= PAD_BOUND_BASE)
        {
            isbound = TRUE;
            padindex = object->pad - PAD_BOUND_BASE;
        }
        else
        {
            isbound = FALSE;
            padindex = object->pad;
        }

        if ((isbound && (DWORD)padindex >= setup->boundpadcount)
            || (!isbound && (DWORD)padindex >= setup->padcount))
        {
            continue;
        }

        bound = isbound ? &setup->boundpads[padindex] : NULL;
        pad = isbound ? &bound->pad : &setup->pads[padindex];
        model = ObjectGetModel(cache, object->modelid, projectdir, &rom);
        if (model == NULL || !ObjectMakeBasis(pad, worldscale, &basis))
        {
            continue;
        }

        memcpy(min, model->min, sizeof(min));
        memcpy(max, model->max, sizeof(max));
        memcpy(reference, basis.pos, sizeof(reference));
        if (hasstan)
        {
            DWORD *tile = &padtiles[isbound ? setup->padcount + padindex : (DWORD)padindex];

            if (*tile == (DWORD)-2) { *tile = StanResolvePadTile(stan, pad->stanname, reference); }
            referencetile = *tile;
            /* getposstan(radius=0) only validates the resolved tile. Invalid
               pads keep their authored marker, but create no game object. */
            if (referencetile == STAN_TILE_NONE) { continue; }
        }
        scale[0] = scale[1] = scale[2] = model->scale
            * ((float)object->extrascale / 256.0f);

        if (isdoor)
        {
            float padx = fabsf(bound->xmax - bound->xmin) * worldscale;
            float pady = fabsf(bound->ymax - bound->ymin) * worldscale;
            float padz = fabsf(bound->zmax - bound->zmin) * worldscale;
            float modelx = max[0] - min[0];
            float modely = max[1] - min[1];
            float modelz = max[2] - min[2];

            if (!(modelx > 0.000001f && modely > 0.000001f
                  && modelz > 0.000001f))
            {
                continue;
            }

            /* setupDoor maps model X/Y/Z to bound Y/Z/X and sizes the
               three axes independently to fill the authored volume. */
            scale[0] = pady / modelx;
            scale[1] = padz / modely;
            scale[2] = padx / modelz;
            center[0] = (min[0] + max[0]) * 0.5f;
            center[1] = (min[1] + max[1]) * 0.5f;
            center[2] = (min[2] + max[2]) * 0.5f;
        }
        else if (bound != NULL)
        {
            float padx = fabsf(bound->xmax - bound->xmin) * worldscale;
            float pady = fabsf(bound->ymax - bound->ymin) * worldscale;
            float padz = fabsf(bound->zmax - bound->zmin) * worldscale;
            float modeldim[3] = {
                max[0] - min[0], max[1] - min[1], max[2] - min[2]
            };
            /* A degenerate model axis keeps the base scale. Apply the
               setup's extra scale once, after choosing the fitted scale. */
            float fitted[3] = { model->scale, model->scale, model->scale };
            float extra = (float)object->extrascale / 256.0f;

            if (modeldim[0] > 0.000001f) fitted[0] = padx / modeldim[0];
            if (modeldim[1] > 0.000001f)
                fitted[1] = ((object->flags & PROPFLAG_ONSIDE) ? padz : pady)
                          / modeldim[1];
            if (modeldim[2] > 0.000001f)
                fitted[2] = ((object->flags & PROPFLAG_ONSIDE) ? pady : padz)
                          / modeldim[2];

            if (object->flags & PROPFLAG_SCALE_TO_PAD_BOUNDS)
            {
                float uniform = fitted[0];
                if (fitted[1] < uniform) uniform = fitted[1];
                if (fitted[2] < uniform) uniform = fitted[2];
                scale[0] = scale[1] = scale[2] = uniform * extra;
            }
            else
            {
                if (object->flags & PROPFLAG_SCALE_TO_X_BOUNDS) scale[0] = fitted[0] * extra;
                if (object->flags & PROPFLAG_SCALE_TO_Y_BOUNDS) scale[1] = fitted[1] * extra;
                if (object->flags & PROPFLAG_SCALE_TO_Z_BOUNDS) scale[2] = fitted[2] * extra;
            }
        }

        if (bound != NULL)
        {
            float localcenter[3] = {
                (bound->xmin + bound->xmax) * 0.5f * worldscale,
                (bound->ymin + bound->ymax) * 0.5f * worldscale,
                (bound->zmin + bound->zmax) * 0.5f * worldscale
            };
            int axis;

            for (axis = 0; axis < 3; axis++)
            {
                float side = pad->up[(axis + 1) % 3] * pad->look[(axis + 2) % 3]
                           - pad->up[(axis + 2) % 3] * pad->look[(axis + 1) % 3];

                basis.pos[axis] += side * localcenter[0]
                                 + pad->up[axis] * localcenter[1]
                                 + pad->look[axis] * localcenter[2];
            }
            if (!isdoor && !(object->flags2 & PROPFLAG2_DRONEGUN))
            {
                DWORD centerstan = referencetile;

                if (!hasstan || StanWalkTiles(stan, &centerstan,
                        reference[0], reference[2], basis.pos[0], basis.pos[2]))
                {
                    referencetile = centerstan;
                    memcpy(reference, basis.pos, sizeof(reference));
                }
            }
        }

        placedtile = referencetile;
        if (!isdoor)
        {
            ObjectApplyPlacementFlags(object->flags, bound, worldscale,
                                      min, max, &basis, center);
            if (hasstan)
            {
                float desired[3];
                int axis;

                if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_UPSIDEDOWN | PROPFLAG_INAIR)))
                {
                    float ground;

                    if (!StanGetTileHeight(stan, referencetile, reference[0], reference[2], &ground)) { continue; }
                    basis.pos[1] = ObjectFindSupportHeight(supports, supportcount,
                        stan->tiles[referencetile].room, reference, ground,
                        basis.up[1] * scale[1] * (max[1] - min[1]));
                }
                for (axis = 0; axis < 3; axis++)
                {
                    desired[axis] = basis.pos[axis] - basis.side[axis] * center[0] * scale[0]
                        - basis.up[axis] * center[1] * scale[1] - basis.look[axis] * center[2] * scale[2];
                }
                if ((object->flags2 & PROPFLAG2_DRONEGUN)
                    || !StanWalkTiles(stan, &placedtile, reference[0], reference[2], desired[0], desired[2]))
                {
                    placedtile = referencetile;
                    if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_ABSOLUTEPOSITION))
                        && !(object->flags2 & PROPFLAG2_DRONEGUN))
                    {
                        memcpy(basis.pos, reference, sizeof(reference));
                        ZeroMemory(center, sizeof(center));
                    }
                }
            }
        }

        ObjectPlaceModel(&builder, model, &basis, scale, isdoor, center, i);
        if (builder.failed)
        {
            *reasonout = "out of memory building setup object geometry.";
            goto fail;
        }

        if (hasstan && !isdoor && object->type != PROPDEF_COLLECTABLE
            && (object->flags & PROPFLAG_FORCE_COLLISIONS) && !object->nonsolid)
        {
            ObjectBuildSupport(&supports[supportcount++], &basis, scale, center,
                min, max, stan->tiles[placedtile].room, object->type == PROPDEF_AIRCRAFT);
        }

        if (isbound)
        {
            out->occupiedboundpads[padindex] = 1;
        }
        else
        {
            out->occupiedpads[padindex] = 1;
        }
        out->objectcount++;
    }

    if (!CharacterLoadSetupGeometry(projectdir, setup, stan, &rom, levelscale,
                                     &characters, reasonout)) { goto fail; }
    if (characters.tricount > 0)
    {
        if (!ObjectBuilderReserve(&builder, characters.tricount))
        {
            *reasonout = "out of memory adding characters to the scene.";
            goto fail;
        }
        memcpy(builder.tris + builder.tricount * 3, characters.tris,
            (size_t)characters.tricount * 3 * sizeof(*builder.tris));
        memcpy(builder.tritags + builder.tricount, characters.tritags,
            (size_t)characters.tricount * sizeof(*builder.tritags));
        memcpy(builder.objectindices + builder.tricount, characters.objectindices,
            (size_t)characters.tricount * sizeof(*builder.objectindices));
        builder.tricount += characters.tricount;
        for (i = 0; i < setup->padcount; i++)
        {
            out->occupiedpads[i] |= characters.occupiedpads[i];
        }
        out->objectcount += characters.objectcount;
    }
    ObjectGeometryFree(&characters);

    for (i = 0; i < OBJECT_MODEL_CACHE_COUNT; i++)
    {
        free(cache[i].tris);
        free(cache[i].tritags);
    }

    out->tris = builder.tris;
    out->tritags = builder.tritags;
    out->objectindices = builder.objectindices;
    out->tricount = builder.tricount;
    free(supports);
    free(padtiles);
    RomFree(&rom);
    return TRUE;

fail:
    ObjectGeometryFree(&characters);
    free(supports);
    free(padtiles);
    RomFree(&rom);
    for (i = 0; i < OBJECT_MODEL_CACHE_COUNT; i++)
    {
        free(cache[i].tris);
        free(cache[i].tritags);
    }
    free(builder.tris);
    free(builder.tritags);
    free(builder.objectindices);
    ObjectGeometryFree(out);
    return FALSE;
}

void ObjectGeometryFree(SetupObjectGeometry *geometry)
{
    free(geometry->occupiedboundpads);
    free(geometry->occupiedpads);
    free(geometry->tritags);
    free(geometry->objectindices);
    free(geometry->tris);
    ZeroMemory(geometry, sizeof(*geometry));
}

static const BgVertex *ObjectFirstVertex(const SetupObjectGeometry *geometry, DWORD index)
{
    DWORD i;
    for (i = 0; i < geometry->tricount; i++)
    {
        if (geometry->objectindices[i] == index) { return &geometry->tris[i * 3]; }
    }
    return NULL;
}

/* The caller holds an EditHistory transaction. Rebuild with explicit pad
 * placement, then compensate for the old floor/support offset. Comparing
 * actual transformed vertices also covers bound pads and rotated props. */
BOOL ObjectTranslateSetupObject(const char *projectdir, SetupFile *setup,
    const StanFile *stan, float levelscale, const SetupObjectGeometry *before,
    DWORD index, const double offset[3], SetupObjectGeometry *out,
    const char **reasonout)
{
    const BgVertex *old = ObjectFirstVertex(before, index), *placed;
    SetupObjectGeometry provisional;
    double correction[3];
    DWORD a = 0, b = 0;
    ZeroMemory(out, sizeof(*out));
    ZeroMemory(&provisional, sizeof(provisional));
    if (old == NULL) { *reasonout = "The object has no rendered geometry."; return FALSE; }
    if (!SetupFileTranslateObject(setup,index,levelscale,offset,reasonout)
        || !ObjectLoadSetupGeometry(projectdir,setup,stan,levelscale,&provisional,reasonout)) { return FALSE; }
    placed = ObjectFirstVertex(&provisional, index);
    if (placed == NULL)
    {
        ObjectGeometryFree(&provisional);
        *reasonout = "The object cannot be placed at this location.";
        return FALSE;
    }
    correction[0] = (double)old->x + offset[0] - placed->x;
    correction[1] = (double)old->y + offset[1] - placed->y;
    correction[2] = (double)old->z + offset[2] - placed->z;
    ObjectGeometryFree(&provisional);
    if (!SetupFileTranslateObject(setup,index,levelscale,correction,reasonout)
        || !ObjectLoadSetupGeometry(projectdir,setup,stan,levelscale,out,reasonout)) { return FALSE; }
    /* Placement must be a translation, never an accidental change of scale
       or orientation. Reject locations that cannot reproduce the preview. */
    for (;;)
    {
        int corner;
        while (a < before->tricount && before->objectindices[a] != index) { a++; }
        while (b < out->tricount && out->objectindices[b] != index) { b++; }
        if (a == before->tricount || b == out->tricount) { break; }
        for (corner=0; corner<3; corner++)
        {
            const BgVertex *v=&before->tris[a*3+corner], *w=&out->tris[b*3+corner];
            if (fabs((double)w->x-v->x-offset[0]) > 0.02
                || fabs((double)w->y-v->y-offset[1]) > 0.02
                || fabs((double)w->z-v->z-offset[2]) > 0.02) { goto invalid; }
        }
        a++; b++;
    }
    if (a == before->tricount && b == out->tricount) { return TRUE; }
invalid:
    ObjectGeometryFree(out);
    *reasonout = "The setup placement rules cannot reproduce this translation.";
    return FALSE;
}
