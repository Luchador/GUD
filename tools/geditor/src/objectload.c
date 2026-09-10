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

BOOL ObjectResolvePlaceableModel(const char *name, BOOL *character, int *modelid)
{
    int i;
    if (name == NULL || character == NULL || modelid == NULL)
    {
        return FALSE;
    }
    if (name[0] == 'P')
    {
        const char *filename;
        for (i = 0; ModelGetPropDefinition(i, &filename, NULL); i++)
        {
            if (strcmp(name, filename) == 0)
            {
                *character = FALSE;
                *modelid = i;
                return TRUE;
            }
        }
    }
    else if (name[0] == 'C' && strncmp(name, "Chead", 5) != 0 &&
             strcmp(name, "Csuit_lf_handZ") != 0)
    {
        CharacterModelDefinition definition;
        for (i = 0; CharacterGetModelDefinition(i, &definition); i++)
        {
            if (strcmp(name, definition.filename) == 0)
            {
                *character = TRUE;
                *modelid = i;
                return TRUE;
            }
        }
    }
    return FALSE;
}

typedef struct ModelCacheEntry {
    BOOL attempted;
    BgVertex *tris;
    unsigned short *tritags;
    BgRenderFlags *renderflags;
    DWORD tricount;
    float scale;
    float min[3], max[3];
} ModelCacheEntry;

typedef struct ObjectBuilder {
    BgVertex *tris;
    unsigned short *tritags;
    BgRenderFlags *renderflags;
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
    BgRenderFlags *renderflags;
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
    renderflags = (BgRenderFlags *)realloc(
        builder->renderflags, (size_t)capacity * sizeof(*renderflags));
    if (renderflags == NULL) { builder->failed = TRUE; return FALSE; }
    builder->renderflags = renderflags;

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
        builder->renderflags[outfirst + tri] = model->renderflags[tri];
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
            if (model->renderflags[tri] & BG_RENDER_ENVIRONMENT)
            {
                float normal[3];
                const float *xaxis = door ? basis->up : basis->side;
                const float *yaxis = door ? basis->look : basis->up;
                const float *zaxis = door ? basis->side : basis->look;
                /* Normals use the inverse transpose, including fitted doors'
                   non-uniform scale, and receive no position offset. */
                for (axis = 0; axis < 3; axis++)
                {
                    normal[axis] = scale[axis] != 0
                        ? source->environment.normal[axis] / scale[axis] : 0;
                }
                for (axis = 0; axis < 3; axis++)
                {
                    dest->environment.normal[axis] = xaxis[axis] * normal[0]
                        + yaxis[axis] * normal[1] + zaxis[axis] * normal[2];
                }
            }

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
            &entry->tricount, &entry->tritags, &entry->renderflags, &entry->scale, &why);
        if (entry->tris != NULL && entry->tricount > 0)
        {
            const char *name;
            DWORD offset, size;

            ObjectModelBounds(entry->tris, entry->tricount, entry->min, entry->max);
            /* Gameplay boxes come from the required base ROM. Models with
               no native box use their render-geometry bounds. */
            if (ModelGetPropDefinition(modelid, &name, NULL)
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

/* Shared with scaling so the new bound pad starts at the visible size,
   including uniform fitting, per-axis flags and the setup extra scale. */
static BOOL ObjectPlacementScale(const ModelCacheEntry *model, const SetupObject *object,
                                 const SetupBoundPad *bound, float worldscale, float scale[3])
{
    const float *min = model->min, *max = model->max;
    BOOL isdoor = object->type == PROPDEF_DOOR;
    scale[0] = scale[1] = scale[2] = model->scale * ((float)object->extrascale / 256.0f);

    if (isdoor)
    {
        float padx = fabsf(bound->xmax - bound->xmin) * worldscale;
        float pady = fabsf(bound->ymax - bound->ymin) * worldscale;
        float padz = fabsf(bound->zmax - bound->zmin) * worldscale;
        float modelx = max[0] - min[0];
        float modely = max[1] - min[1];
        float modelz = max[2] - min[2];

        if (!(modelx > 0.000001f && modely > 0.000001f && modelz > 0.000001f))
        {
            return FALSE;
        }

        /* setupDoor maps model X/Y/Z to bound Y/Z/X and sizes the
           three axes independently to fill the authored volume. */
        scale[0] = pady / modelx;
        scale[1] = padz / modely;
        scale[2] = padx / modelz;
    }
    else if (bound != NULL)
    {
        float padx = fabsf(bound->xmax - bound->xmin) * worldscale;
        float pady = fabsf(bound->ymax - bound->ymin) * worldscale;
        float padz = fabsf(bound->zmax - bound->zmin) * worldscale;
        float modeldim[3] = {max[0] - min[0], max[1] - min[1], max[2] - min[2]};
        /* A degenerate model axis keeps the base scale. Apply the
           setup's extra scale once, after choosing the fitted scale. */
        float fitted[3] = {model->scale, model->scale, model->scale};
        float extra = (float)object->extrascale / 256.0f;

        if (modeldim[0] > 0.000001f)
            fitted[0] = padx / modeldim[0];
        if (modeldim[1] > 0.000001f)
            fitted[1] = ((object->flags & PROPFLAG_ONSIDE) ? padz : pady) / modeldim[1];
        if (modeldim[2] > 0.000001f)
            fitted[2] = ((object->flags & PROPFLAG_ONSIDE) ? pady : padz) / modeldim[2];

        if (object->flags & PROPFLAG_SCALE_TO_PAD_BOUNDS)
        {
            float uniform = fitted[0];
            if (fitted[1] < uniform)
                uniform = fitted[1];
            if (fitted[2] < uniform)
                uniform = fitted[2];
            scale[0] = scale[1] = scale[2] = uniform * extra;
        }
        else
        {
            if (object->flags & PROPFLAG_SCALE_TO_X_BOUNDS)
                scale[0] = fitted[0] * extra;
            if (object->flags & PROPFLAG_SCALE_TO_Y_BOUNDS)
                scale[1] = fitted[1] * extra;
            if (object->flags & PROPFLAG_SCALE_TO_Z_BOUNDS)
                scale[2] = fitted[2] * extra;
        }
    }

    return TRUE;
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
    if (pathlength < 0 || pathlength >= (int)sizeof(basepath))
    { *reasonout = "The project base-ROM path is too long."; goto fail; }
    if (!RomLoad(basepath, &rom, &basewhy))
    { *reasonout = basewhy; goto fail; }

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
        if (!ObjectPlacementScale(model, object, bound, worldscale, scale)) { continue; }
        if (isdoor)
        {
            center[0] = (min[0] + max[0]) * .5f;
            center[1] = (min[1] + max[1]) * .5f;
            center[2] = (min[2] + max[2]) * .5f;
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
        memcpy(builder.renderflags + builder.tricount, characters.renderflags,
               characters.tricount * sizeof(*builder.renderflags));
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
        free(cache[i].renderflags);
    }

    out->tris = builder.tris;
    out->tritags = builder.tritags;
    out->renderflags = builder.renderflags;
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
        free(cache[i].renderflags);
    }
    free(builder.tris);
    free(builder.tritags);
    free(builder.renderflags);
    free(builder.objectindices);
    ObjectGeometryFree(out);
    return FALSE;
}

void ObjectGeometryFree(SetupObjectGeometry *geometry)
{
    free(geometry->occupiedboundpads);
    free(geometry->occupiedpads);
    free(geometry->tritags);
    free(geometry->renderflags);
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

/* The caller holds an EditHistory transaction. Props use explicit placement
 * with their old floor/support offset compensated. Characters keep the game's
 * grounding rules, using their visible feet as the starting pad position. */
BOOL ObjectTranslateSetupModel(const char *projectdir, SetupFile *setup,
    const StanFile *stan, float levelscale, const SetupObjectGeometry *before,
    DWORD index, const double offset[3], SetupObjectGeometry *out,
    const char **reasonout)
{
    const BgVertex *old = ObjectFirstVertex(before, index), *placed;
    SetupObjectGeometry provisional;
    double correction[3], padmove[3], expected[3];
    BOOL character = (index & SETUP_CHARACTER_SELECTION_BIT) != 0;
    DWORD a = 0, b = 0;
    int axis;
    ZeroMemory(out, sizeof(*out));
    ZeroMemory(&provisional, sizeof(provisional));
    if (old == NULL) { *reasonout = "The model has no rendered geometry."; return FALSE; }
    for (axis = 0; axis < 3; axis++) { padmove[axis] = expected[axis] = offset[axis]; }
    if (character)
    {
        DWORD chrindex = index & ~SETUP_CHARACTER_SELECTION_BIT;
        const SetupPad *pad;
        float feet[3];

        if (setup == NULL || setup->characters == NULL || setup->pads == NULL
            || chrindex >= setup->charactercount
            || setup->characters[chrindex].pad >= setup->padcount)
        {
            *reasonout = "The character has no editable placement pad.";
            return FALSE;
        }
        pad = &setup->pads[setup->characters[chrindex].pad];
        if (!CharacterGetPadPosition(pad, stan, levelscale, feet))
        {
            *reasonout = "The character's current stan floor could not be resolved.";
            return FALSE;
        }
        /* Authored pads can sit well above their character. Move from the
         * displayed feet so the requested height chooses the intended floor. */
        for (axis = 0; axis < 3; axis++)
        {
            padmove[axis] += (double)feet[axis] - pad->pos[axis] / levelscale;
        }
    }
    if (!SetupFileTranslateModel(setup,index,levelscale,padmove,reasonout)
        || !ObjectLoadSetupGeometry(projectdir,setup,stan,levelscale,&provisional,reasonout)) { return FALSE; }
    placed = ObjectFirstVertex(&provisional, index);
    if (placed == NULL)
    {
        ObjectGeometryFree(&provisional);
        *reasonout = character ? "The character cannot be placed on a stan floor at this location."
                               : "The object cannot be placed at this location.";
        return FALSE;
    }
    if (character)
    {
        expected[1] = (double)placed->y - old->y;
        *out = provisional;
    }
    else
    {
        correction[0] = (double)old->x + offset[0] - placed->x;
        correction[1] = (double)old->y + offset[1] - placed->y;
        correction[2] = (double)old->z + offset[2] - placed->z;
        ObjectGeometryFree(&provisional);
        if (!SetupFileTranslateModel(setup,index,levelscale,correction,reasonout)
            || !ObjectLoadSetupGeometry(projectdir,setup,stan,levelscale,out,reasonout)) { return FALSE; }
    }
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
            if (fabs((double)w->x-v->x-expected[0]) > 0.02
                || fabs((double)w->y-v->y-expected[1]) > 0.02
                || fabs((double)w->z-v->z-expected[2]) > 0.02) { goto invalid; }
        }
        a++; b++;
    }
    if (a == before->tricount && b == out->tricount) { return TRUE; }
invalid:
    ObjectGeometryFree(out);
    *reasonout = "The setup placement rules cannot reproduce this translation.";
    return FALSE;
}

BOOL ObjectRotateSetupModel(const char *projectdir, SetupFile *setup, const StanFile *stan,
                            float levelscale, const SetupObjectGeometry *before, DWORD index,
                            const Rotation *rotation, const double pivot[3],
                            SetupObjectGeometry *out, const char **reasonout)
{
    const BgVertex *old = ObjectFirstVertex(before, index), *placed;
    SetupObjectGeometry provisional = {0};
    SetupPadRef ref;
    BOOL changed;
    double padmove[3] = {0}, source[3], expected[3], correction[3], groundoffset = 0;
    DWORD a = 0, b = 0;
    int axis;
    ZeroMemory(out, sizeof(*out));
    if (!old || !pivot || !RotationValid(rotation))
    {
        *reasonout = "Invalid model rotation.";
        return FALSE;
    }
    /* Guard records only represent yaw. Reject a pitch/roll even if called
       outside the UI, rather than silently discarding it on ROM export. */
    if ((index & SETUP_CHARACTER_SELECTION_BIT) &&
        (fabs(rotation->m[1][1] - 1) > 1e-6 || fabs(rotation->m[0][1]) > 1e-6 ||
         fabs(rotation->m[2][1]) > 1e-6))
    {
        *reasonout = "Characters support Y-axis rotation only.";
        return FALSE;
    }
    source[0] = old->x;
    source[1] = old->y;
    source[2] = old->z;
    RotationPoint(rotation, pivot, source, expected);
    if (index & SETUP_CHARACTER_SELECTION_BIT)
    {
        const SetupPad *pad;
        float feet[3];
        if (!SetupFileGetModelPad(setup, index, &ref))
        {
            return FALSE;
        }
        pad = &setup->pads[ref.index];
        if (!CharacterGetPadPosition(pad, stan, levelscale, feet))
        {
            *reasonout = "The character's current stan floor could not be resolved.";
            return FALSE;
        }
        for (axis = 0; axis < 3; axis++)
        {
            padmove[axis] = (double)feet[axis] - pad->pos[axis] / levelscale;
        }
    }
    if (!SetupFileTranslateModel(setup, index, levelscale, padmove, reasonout) ||
        !SetupFileGetModelPad(setup, index, &ref) ||
        !SetupFileRotatePad(setup, &ref, rotation, &changed, reasonout) ||
        !ObjectLoadSetupGeometry(projectdir, setup, stan, levelscale, &provisional, reasonout))
    {
        goto fail;
    }
    placed = ObjectFirstVertex(&provisional, index);
    if (!placed)
    {
        *reasonout = "The rotated model cannot be placed here.";
        goto fail;
    }
    correction[0] = expected[0] - placed->x;
    correction[1] = expected[1] - placed->y;
    correction[2] = expected[2] - placed->z;
    ObjectGeometryFree(&provisional);
    if (!SetupFileTranslateModel(setup, index, levelscale, correction, reasonout) ||
        !ObjectLoadSetupGeometry(projectdir, setup, stan, levelscale, out, reasonout))
    {
        goto fail;
    }
    if (index & SETUP_CHARACTER_SELECTION_BIT)
    {
        placed = ObjectFirstVertex(out, index);
        if (!placed)
        {
            *reasonout = "The character cannot be placed on a stan floor here.";
            goto fail;
        }
        /* Yaw keeps characters upright; a shifted foot position follows the
           destination slope, as it does for the translation tool. */
        groundoffset = placed->y - expected[1];
    }
    /* Verify the game placement rules reproduce the rigid rotation. */
    for (;;)
    {
        int corner;
        while (a < before->tricount && before->objectindices[a] != index)
        {
            a++;
        }
        while (b < out->tricount && out->objectindices[b] != index)
        {
            b++;
        }
        if (a == before->tricount || b == out->tricount)
        {
            break;
        }
        for (corner = 0; corner < 3; corner++)
        {
            const BgVertex *v = &before->tris[a * 3 + corner], *w = &out->tris[b * 3 + corner];
            double actual[3] = {w->x, w->y, w->z};
            source[0] = v->x;
            source[1] = v->y;
            source[2] = v->z;
            RotationPoint(rotation, pivot, source, expected);
            for (axis = 0; axis < 3; axis++)
            {
                if (fabs(expected[axis] + (axis == 1 ? groundoffset : 0) - actual[axis]) > .03)
                {
                    goto invalid;
                }
            }
        }
        a++;
        b++;
    }
    if (a == before->tricount && b == out->tricount)
    {
        return TRUE;
    }
invalid:
    *reasonout = "The setup placement rules cannot reproduce this rotation at this location.";
fail:
    ObjectGeometryFree(&provisional);
    ObjectGeometryFree(out);
    return FALSE;
}

/* The panel reports the visible model basis, including setup placement flags
   and the door's axis permutation, rather than just the underlying pad. */
BOOL ObjectGetSetupModelRotation(const SetupFile *setup, DWORD selection, Rotation *out)
{
    SetupPadRef ref;
    const SetupPad *pad;
    ObjectBasis basis;
    int axis;
    if (!SetupFileGetModelPad(setup, selection, &ref))
    {
        return FALSE;
    }
    pad = ref.bound ? &setup->boundpads[ref.index].pad : &setup->pads[ref.index];
    if (selection & SETUP_CHARACTER_SELECTION_BIT)
    {
        double heading = atan2(pad->look[0], pad->look[2]) * 180.0 / 3.14159265358979323846;
        RotationAxis(out, 1, heading);
        return RotationValid(out);
    }
    if (!ObjectMakeBasis(pad, 1, &basis))
    {
        return FALSE;
    }
    if (setup->objects[selection].type == PROPDEF_DOOR)
    {
        for (axis = 0; axis < 3; axis++)
        {
            out->m[axis][0] = basis.up[axis];
            out->m[axis][1] = basis.look[axis];
            out->m[axis][2] = basis.side[axis];
        }
    }
    else
    {
        float zero[3] = {0}, center[3] = {0};
        ObjectApplyPlacementFlags(setup->objects[selection].flags, NULL, 1, zero, zero, &basis,
                                  center);
        for (axis = 0; axis < 3; axis++)
        {
            out->m[axis][0] = basis.side[axis];
            out->m[axis][1] = basis.up[axis];
            out->m[axis][2] = basis.look[axis];
        }
    }
    return RotationValid(out);
}

BOOL ObjectScaleSetupModel(const char *projectdir, SetupFile *setup, const StanFile *stan,
                           float levelscale, const SetupObjectGeometry *before, DWORD index,
                           const Scaling *scale, SetupObjectGeometry *out, const char **reasonout)
{
    ModelCacheEntry *cache = NULL, *model;
    SetupObjectGeometry provisional = {0};
    SetupPadRef ref;
    RomFile rom = {0};
    char basepath[MAX_PATH];
    const BgVertex *oldvertex, *newvertex;
    const SetupObject *object;
    float modelscale[3];
    double dimensions[3], bounds[6], source[3], expected[3], correction[3];
    DWORD a = 0, b = 0;
    int axis, corner;
    BOOL ok = FALSE;
    ZeroMemory(out, sizeof(*out));
    *reasonout = "Only props with editable placement pads can be scaled.";
    if (!setup || !before || !ScalingValid(scale) || !(levelscale > 0) ||
        (index & SETUP_CHARACTER_SELECTION_BIT) || !SetupFileGetModelPad(setup, index, &ref) ||
        !(oldvertex = ObjectFirstVertex(before, index)))
    {
        return FALSE;
    }
    cache = calloc(OBJECT_MODEL_CACHE_COUNT, sizeof(*cache));
    if (!cache)
    {
        *reasonout = "Out of memory scaling the prop.";
        goto done;
    }
    {
        int length = snprintf(basepath, sizeof(basepath), "%s\\%s", projectdir, ROM_EXPORT_BASE_FILENAME);
        if (length < 0 || length >= (int)sizeof(basepath))
        { *reasonout = "The project base-ROM path is too long."; goto done; }
        if (!RomLoad(basepath, &rom, reasonout)) { goto done; }
    }
    object = &setup->objects[index];
    model = ObjectGetModel(cache, object->modelid, projectdir, &rom);
    if (!model ||
        !ObjectPlacementScale(model, object, ref.bound ? &setup->boundpads[ref.index] : NULL,
                              1.0f / levelscale, modelscale))
    {
        *reasonout = "The model's placement bounds could not be loaded.";
        goto done;
    }
    for (axis = 0; axis < 3; axis++)
    {
        int padaxis = object->type == PROPDEF_DOOR                     ? (axis + 1) % 3
                      : (object->flags & PROPFLAG_ONSIDE) && axis != 0 ? 3 - axis
                                                                       : axis;
        dimensions[padaxis] = (model->max[axis] - model->min[axis]) * modelscale[axis] *
                              levelscale * scale->factor[padaxis];
    }
    for (axis = 0; axis < 3; axis++)
    {
        bounds[axis * 2] = -dimensions[axis] * .5;
        bounds[axis * 2 + 1] = dimensions[axis] * .5;
    }
    source[0] = oldvertex->x;
    source[1] = oldvertex->y;
    source[2] = oldvertex->z;
    ScalingPoint(scale, source, expected);
    if (!SetupFileSetModelBounds(setup, index, levelscale, bounds, reasonout) ||
        !ObjectLoadSetupGeometry(projectdir, setup, stan, levelscale, &provisional, reasonout))
    {
        goto done;
    }
    newvertex = ObjectFirstVertex(&provisional, index);
    if (!newvertex)
    {
        goto invalid;
    }
    correction[0] = expected[0] - newvertex->x;
    correction[1] = expected[1] - newvertex->y;
    correction[2] = expected[2] - newvertex->z;
    if (!SetupFileTranslateModel(setup, index, levelscale, correction, reasonout) ||
        !ObjectLoadSetupGeometry(projectdir, setup, stan, levelscale, out, reasonout))
    {
        goto done;
    }
    /* Check the actual rebuilt model, not just its bounding box: pad flags,
       grounding and model-axis permutations must reproduce the preview. */
    for (;;)
    {
        while (a < before->tricount && before->objectindices[a] != index)
        {
            a++;
        }
        while (b < out->tricount && out->objectindices[b] != index)
        {
            b++;
        }
        if (a == before->tricount || b == out->tricount)
        {
            break;
        }
        for (corner = 0; corner < 3; corner++)
        {
            const BgVertex *v = &before->tris[a * 3 + corner], *w = &out->tris[b * 3 + corner];
            double actual[3] = {w->x, w->y, w->z};
            source[0] = v->x;
            source[1] = v->y;
            source[2] = v->z;
            ScalingPoint(scale, source, expected);
            for (axis = 0; axis < 3; axis++)
            {
                if (fabs(expected[axis] - actual[axis]) > .03)
                {
                    goto invalid;
                }
            }
        }
        a++;
        b++;
    }
    if (a != before->tricount || b != out->tricount)
    {
        goto invalid;
    }
    ok = TRUE;
    *reasonout = "";
    goto done;
invalid:
    *reasonout = "The setup placement rules cannot reproduce this scale at this location.";
done:
    if (cache)
    {
        for (axis = 0; axis < OBJECT_MODEL_CACHE_COUNT; axis++)
        {
            free(cache[axis].tris);
            free(cache[axis].tritags);
            free(cache[axis].renderflags);
        }
        free(cache);
    }
    RomFree(&rom);
    ObjectGeometryFree(&provisional);
    if (!ok)
    {
        ObjectGeometryFree(out);
    }
    return ok;
}
