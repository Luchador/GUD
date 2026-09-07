/* Builds the editor's placed-object layer from setup records and the
   model glTF files stored in a project. Character records are deliberately
   excluded: their skeleton assembly is a separate concern. */

#include <windows.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "modelload.h"
#include "objectload.h"

#define OBJECT_MODEL_CACHE_COUNT 512

#define PROPDEF_DOOR 1
#define PAD_BOUND_BASE 10000

#define PROPFLAG_SCALE_UNIFORM    0x00000010u
#define PROPFLAG_SCALE_X          0x00000020u
#define PROPFLAG_SCALE_Y          0x00000040u
#define PROPFLAG_SCALE_Z          0x00000080u
#define PROPFLAG_ONSCREEN         0x00000002u
#define PROPFLAG_ASSIGNED_TO_CHR  0x00004000u
#define PROPFLAG_INSIDE_OBJECT    0x00008000u

typedef struct ModelCacheEntry {
    BOOL attempted;
    BgVertex *tris;
    unsigned short *tritags;
    DWORD tricount;
    float scale;
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
                                       const char *projectdir)
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
    }

    return entry->tris != NULL ? entry : NULL;
}

BOOL ObjectLoadSetupGeometry(const char *projectdir, const SetupFile *setup,
                             float levelscale, SetupObjectGeometry *out,
                             const char **reasonout)
{
    ModelCacheEntry cache[OBJECT_MODEL_CACHE_COUNT];
    ObjectBuilder builder;
    float worldscale;
    DWORD i;

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(cache, sizeof(cache));
    ZeroMemory(&builder, sizeof(builder));
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

    for (i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *object = &setup->objects[i];
        const SetupPad *pad;
        const SetupBoundPad *bound = NULL;
        ModelCacheEntry *model;
        ObjectBasis basis;
        float min[3], max[3], center[3] = { 0.0f, 0.0f, 0.0f };
        float scale[3];
        int padindex;
        BOOL isbound;
        BOOL isdoor = object->type == PROPDEF_DOOR;

        if (object->deleted
            || (object->flags & (PROPFLAG_ASSIGNED_TO_CHR
                              | PROPFLAG_INSIDE_OBJECT)) != 0
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
        model = ObjectGetModel(cache, object->modelid, projectdir);
        if (model == NULL || !ObjectMakeBasis(pad, worldscale, &basis))
        {
            continue;
        }

        ObjectModelBounds(model->tris, model->tricount, min, max);
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
            float fitted[3] = { scale[0], scale[1], scale[2] };
            float extra = (float)object->extrascale / 256.0f;

            if (modeldim[0] > 0.000001f) fitted[0] = padx / modeldim[0];
            if (modeldim[1] > 0.000001f)
                fitted[1] = ((object->flags & PROPFLAG_ONSCREEN) ? padz : pady)
                          / modeldim[1];
            if (modeldim[2] > 0.000001f)
                fitted[2] = ((object->flags & PROPFLAG_ONSCREEN) ? pady : padz)
                          / modeldim[2];

            if (object->flags & PROPFLAG_SCALE_UNIFORM)
            {
                float uniform = fitted[0];
                if (fitted[1] < uniform) uniform = fitted[1];
                if (fitted[2] < uniform) uniform = fitted[2];
                scale[0] = scale[1] = scale[2] = uniform * extra;
            }
            else
            {
                if (object->flags & PROPFLAG_SCALE_X) scale[0] = fitted[0] * extra;
                if (object->flags & PROPFLAG_SCALE_Y) scale[1] = fitted[1] * extra;
                if (object->flags & PROPFLAG_SCALE_Z) scale[2] = fitted[2] * extra;
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
                basis.pos[axis] += basis.side[axis] * localcenter[0]
                                 + basis.up[axis] * localcenter[1]
                                 + basis.look[axis] * localcenter[2];
            }
        }

        ObjectPlaceModel(&builder, model, &basis, scale, isdoor, center, i);
        if (builder.failed)
        {
            *reasonout = "out of memory building setup object geometry.";
            goto fail;
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

    for (i = 0; i < OBJECT_MODEL_CACHE_COUNT; i++)
    {
        free(cache[i].tris);
        free(cache[i].tritags);
    }

    out->tris = builder.tris;
    out->tritags = builder.tritags;
    out->objectindices = builder.objectindices;
    out->tricount = builder.tricount;
    return TRUE;

fail:
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
