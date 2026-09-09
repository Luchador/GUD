/*
 * Model file layout (bondtypes.h is the spec):
 *   The ModelFileHeader lives in the game's static tables, not this file.
 *   Pointers are 0x05-segment addresses; low 24 bits are file offsets.
 *   Nodes are 24 bytes: u16 opcode, Data*, Parent*, Next*, Prev*,
 *   Child*. The tree walks Child-first, then Next.
 *   Mesh data lives in two opcodes:
 *     0x04 DisplayListRecord: Primary@0 Secondary@4 Vertices@0xC
 *     0x18 DisplayList_Collision: Primary@0 Secondary@4 Vertices@8
 *   Display lists are the same Fast3D dialect as bg files, with the
 *   same G_NOOP texture markers - but G_VTX addresses are absolute
 *   file offsets, not blob-relative ones.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "gltf.h"
#include "bgrender.h"
#include "modelload.h"

/* Reuse the exact model-ID order and scale values compiled into the
   game. Redefining the record macro avoids pulling any N64 structs or
   symbols into the editor build; the asset list remains the one source
   of truth when props are added or reordered. */
typedef struct PropModelDefinition {
    void *unusedheader;
    const char *filename;
    float scale;
} PropModelDefinition;

#define ItemModelFileRecord PropModelDefinition
#define PitemZ_entries g_PropModelDefinitions
#define PROPFILERECORD(NAME, SCALE) { NULL, "P" #NAME "Z", SCALE },
#include <assets/obseg/prop/propItemModelFileRecord.inc.c>
#undef PROPFILERECORD
#undef PitemZ_entries
#undef ItemModelFileRecord

#define PROP_MODEL_COUNT \
    ((int)(sizeof(g_PropModelDefinitions) \
         / sizeof(g_PropModelDefinitions[0])) - 1)

#define MDL_G_MTX   0x01
#define MDL_G_NOOP  0xC0
#define MDL_G_VTX   0x04
#define MDL_G_TRI4  0xB1
#define MDL_G_ENDDL 0xB8
#define MDL_G_TRI1  0xBF

#define MDL_MAX_NODES 512
#define MDL_MAX_MATRICES (MDL_MAX_NODES * 3)

typedef struct MdlTranslation {
    float xyz[3];
    BOOL valid;
} MdlTranslation;

typedef struct MdlPose {
    MdlTranslation matrices[MDL_MAX_MATRICES];
    BOOL hasmatrices;
} MdlPose;

typedef struct MdlBuilder {
    BgVertex       *verts;
    unsigned short *texids;
    BgRenderFlags *renderflags;
    DWORD           count;
    DWORD           capacity;
    const char     *error;
} MdlBuilder;

static DWORD md32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static short md16(const unsigned char *p)
{
    return (short)(((int)p[0] << 8) | p[1]);
}

static DWORD mdoff(DWORD segptr)
{
    return segptr & 0x00FFFFFF;
}

static void MdlPush(MdlBuilder *b, const BgVertex *v)
{
    if (b->error)
    {
        return;
    }

    if (b->count == b->capacity)
    {
        DWORD next = b->capacity ? b->capacity * 2 : 1024;
        BgVertex *g = (BgVertex *)realloc(b->verts, next * sizeof(BgVertex));
        unsigned short *gt = (unsigned short *)realloc(b->texids,
                                 (next / 3) * sizeof(unsigned short));

        BgRenderFlags *flags = (BgRenderFlags *)realloc(
            b->renderflags, (size_t)(next / 3) * sizeof(*flags));

        if (flags != NULL) { b->renderflags = flags; }
        if (g != NULL) { b->verts = g; }
        if (gt != NULL) { b->texids = gt; }
        if (g == NULL || gt == NULL || flags == NULL)
        {
            b->error = "out of memory building model geometry.";
            return;
        }

        b->capacity = next;
    }

    b->verts[b->count++] = *v;
}

/* In the unanimated pose, GROUP/OP03/GROUPSIMPLE contribute translations.
   Keep the root at the origin: objTickBuildMatrices supplies its placement
   matrix directly, and setsuboffset replaces its authored offset for animated
   models. Follow Parent through non-transform nodes as modelFindNodeMtx does. */
static BOOL MdlNodeTranslation(const unsigned char *data, DWORD size,
                               DWORD node, float out[3])
{
    int visited = 0;
    DWORD offsets[MDL_MAX_NODES];
    int count = 0;
    int i, axis;

    out[0] = out[1] = out[2] = 0.0f;
    while (node != 0)
    {
        DWORD opcode, offset;

        if (node > size - 24 || visited++ >= MDL_MAX_NODES) { return FALSE; }
        opcode = (unsigned short)md16(data + node) & 0xff;
        offset = mdoff(md32(data + node + 4));
        if ((opcode == 0x02 || opcode == 0x03 || opcode == 0x15)
            && md32(data + node + 8) != 0)
        {
            if (offset == 0 || offset > size - 12) { return FALSE; }
            offsets[count++] = offset;
        }
        node = mdoff(md32(data + node + 8));
    }
    /* Match the game's parent-before-child floating-point addition order. */
    for (i = count - 1; i >= 0; i--)
    {
        for (axis = 0; axis < 3; axis++)
        {
            union { DWORD bits; float value; } coordinate;

            coordinate.bits = md32(data + offsets[i] + axis * 4);
            out[axis] += coordinate.value;
            if (!isfinite(out[axis])) { return FALSE; }
        }
    }
    return TRUE;
}

/* Build every matrix before reading any display lists: a G_MTX can refer to
   a joint later in the tree. Matrix 1/2 (when enabled by the node's upper
   opcode bits) have the same translation as matrix 0 at zero rotation. */
static BOOL MdlAddNodeMatrices(MdlPose *pose, const unsigned char *data,
                               DWORD size, DWORD node)
{
    unsigned short flags = (unsigned short)md16(data + node);
    DWORD opcode = flags & 0xff;
    DWORD offset = mdoff(md32(data + node + 4));
    DWORD needed;
    int slots[3] = { -1, -1, -1 };
    float translation[3];
    int i;

    switch (opcode)
    {
    case 0x01: needed = 4; break;  /* HEADER: MatrixIndex at +2 */
    case 0x02:
    case 0x03: needed = 20; break; /* GROUP: MatrixIDs at +14 */
    case 0x15: needed = 14; break; /* GROUPSIMPLE: Group1 at +12 */
    default: return TRUE;
    }
    if (offset == 0 || offset > size - needed
        || !MdlNodeTranslation(data, size, node, translation))
    {
        return FALSE;
    }
    if (opcode == 0x01) { slots[0] = md16(data + offset + 2); }
    else if (opcode == 0x15) { slots[0] = md16(data + offset + 12); }
    else
    {
        slots[0] = md16(data + offset + 14);
        if (flags & 0x100) { slots[1] = md16(data + offset + 16); }
        if (flags & 0x200) { slots[2] = md16(data + offset + 18); }
    }
    for (i = 0; i < 3; i++)
    {
        if (slots[i] < 0) { continue; }
        if (slots[i] >= MDL_MAX_MATRICES) { return FALSE; }
        memcpy(pose->matrices[slots[i]].xyz, translation, sizeof(translation));
        pose->matrices[slots[i]].valid = TRUE;
        pose->hasmatrices = TRUE;
    }
    return TRUE;
}

/*
 * Same command walk as bgload. Vertex addressing differs by segment:
 * segment 0x05 is a file-absolute offset, while segment 0x04 (the
 * model-vertex segment the renderer maps to the record's Vertices
 * pointer) is RELATIVE to vtxbase - Pbook1Z's own display list is the
 * proof, loading 0x04000000 then 0x040000F0 against Vertices=0x98.
 */
static void MdlWalkGdl(MdlBuilder *b, const unsigned char *data, DWORD maxlen,
                       DWORD gdloffset, DWORD vtxbase, unsigned short layerflag,
                       const MdlPose *pose, const float origin[3],
                       BgRenderState *state, BgMaterial *material)
{
    DWORD pc;
    BgVertex cache[16];
    BgRenderFlags cacheflags[16];
    unsigned int valid = 0;
    const float *translation = origin;
    unsigned short curtex = BG_TEX_NONE;

    for (pc = gdloffset; pc + 8 <= maxlen; pc += 8)
    {
        const unsigned char *cmd = data + pc;

        BgRenderStateRead(state, md32(cmd), md32(cmd + 4));
        if (cmd[0] == BG_G_TEXTURE)
        {
            material->modeword0 = md32(cmd);
            material->modeword1 = md32(cmd + 4);
        }
        if (cmd[0] == 0xFC) /* gDPSetCombine: models have no BG LUT rewrite. */
        {
            material->combineword0 = md32(cmd);
            material->combineword1 = md32(cmd + 4);
        }

        if (cmd[0] == MDL_G_ENDDL)
        {
            return;
        }

        if (cmd[0] == MDL_G_NOOP)
        {
            curtex = (unsigned short)(md32(cmd + 4) & 0xFFF);
            material->textureword0 = md32(cmd);
            material->textureword1 = md32(cmd + 4);
            continue;
        }

        if (cmd[0] == MDL_G_MTX)
        {
            DWORD raw = md32(cmd + 4);
            DWORD index = mdoff(raw) / 64; /* Segment 3: packed N64 Mtx array. */

            if (cmd[1] != 0x02 || (raw >> 24) != 0x03
                || (mdoff(raw) % 64) != 0 || index >= MDL_MAX_MATRICES
                || !pose->matrices[index].valid)
            {
                b->error = "model display list references an unsupported matrix.";
                return;
            }
            translation = pose->matrices[index].xyz;
            continue;
        }

        if (cmd[0] == MDL_G_VTX)
        {
            DWORD raw = md32(cmd + 4);
            DWORD addr = (raw >> 24) == 0x05 ? mdoff(raw)
                                             : vtxbase + mdoff(raw);
            int count = (cmd[1] >> 4) + 1;
            int first = cmd[1] & 0xf;
            int i;

            if (first + count > 16 || addr > maxlen
                || (DWORD)count * 16 > maxlen - addr)
            {
                valid = 0;
                continue;
            }
            for (i = 0; i < count; i++)
            {
                const unsigned char *v = data + addr + i * 16;
                BgVertex *out = &cache[first + i];

                out->x = md16(v + 0) + translation[0];
                out->y = md16(v + 2) + translation[1];
                out->z = md16(v + 4) + translation[2];
                out->s = (float)md16(v + 8) / 32.0f;
                out->t = (float)md16(v + 10) / 32.0f;
                out->r = v[12]; out->g = v[13]; out->b = v[14]; out->a = v[15];
                cacheflags[first + i] = BgRenderStateFlags(state) & BG_RENDER_ENVIRONMENT_MASK;
                BgRenderPrepareEnvironment(out, cacheflags[first + i], material);
                valid |= 1u << (first + i);
            }
            continue;
        }

        if (cmd[0] == MDL_G_TRI1 || cmd[0] == MDL_G_TRI4)
        {
            int tri;
            int tricount = (cmd[0] == MDL_G_TRI1) ? 1 : 4;

            for (tri = 0; tri < tricount; tri++)
            {
                int idx[3];
                int k;

                if (cmd[0] == MDL_G_TRI1)
                {
                    idx[0] = cmd[5] / 10;
                    idx[1] = cmd[6] / 10;
                    idx[2] = cmd[7] / 10;
                }
                else if (tri == 0)
                {
                    idx[0] = cmd[7] & 0xF; idx[1] = cmd[7] >> 4; idx[2] = cmd[3] & 0xF;
                }
                else if (tri == 1)
                {
                    idx[0] = cmd[6] & 0xF; idx[1] = cmd[6] >> 4; idx[2] = cmd[3] >> 4;
                }
                else if (tri == 2)
                {
                    idx[0] = cmd[5] & 0xF; idx[1] = cmd[5] >> 4; idx[2] = cmd[2] & 0xF;
                }
                else
                {
                    idx[0] = cmd[4] & 0xF; idx[1] = cmd[4] >> 4; idx[2] = cmd[2] >> 4;
                }

                if (idx[0] == idx[1] && idx[1] == idx[2])
                {
                    continue;
                }

                if (idx[0] >= 16 || idx[1] >= 16 || idx[2] >= 16
                    || !(valid & (1u << idx[0]))
                    || !(valid & (1u << idx[1]))
                    || !(valid & (1u << idx[2])))
                {
                    continue;
                }

                /* N64 transforms vertices at G_VTX time, not G_TRI time.
                   Cached vertices can therefore belong to different joints. */
                BgRenderAlpha alpha = BgRenderGetMaterialAlpha(state, material);
                for (k = 0; k < 3; k++)
                {
                    BgVertex vertex = cache[idx[k]];
                    vertex.a = BgRenderVertexAlpha(alpha, vertex.a);
                    MdlPush(b, &vertex);
                }

                if (!b->error)
                {
                    b->texids[b->count / 3 - 1] = (unsigned short)(curtex | layerflag);
                    b->renderflags[b->count / 3 - 1] = (BgRenderStateFlags(state) & ~BG_RENDER_ENVIRONMENT_MASK)
                        | cacheflags[idx[0]]
                        | (alpha.texture ? 0 : BG_RENDER_IGNORE_TEXTURE_ALPHA)
                        | BgRenderMaterialWrap(material);
                }
            }
        }
    }
}

/* Emits the mesh data of one node, if it has any. */
static void MdlNodeMeshes(MdlBuilder *b, const unsigned char *data,
                          DWORD maxlen, DWORD opcode, DWORD dataoff,
                          const MdlPose *pose, const float origin[3])
{
    DWORD prioff = 0;
    DWORD secoff = 0;
    int modeltype = 0;
    BgRenderState state;
    BgMaterial material;

    BgMaterialInit(&material);
    BgRenderStateInit(&state, FALSE);
    /* modelApplyRenderModeType2/3/4: TRILERP, MODULATEIA2. */
    material.combineword0 = 0xFC26A004u;
    material.combineword1 = 0x1F1093FFu;

    if (dataoff == 0 || dataoff + 0x14 > maxlen)
    {
        return;
    }

    DWORD vtxbase = 0;

    if (opcode == 0x04 || opcode == 0x18)
    {
        if (opcode == 0x18 && maxlen - dataoff < 0x20) { return; }
        modeltype = opcode == 0x04 ? data[dataoff + 0x12] : md16(data + dataoff + 0x18);
        if (modeltype == 1) /* MODULATEIA, MODULATEIA. */
        {
            material.combineword0 = 0xFC121824u;
            material.combineword1 = 0xFF33FFFFu;
        }
        prioff = mdoff(md32(data + dataoff + 0));
        secoff = mdoff(md32(data + dataoff + 4));

        /* Vertices: op 0x04 keeps them at +0xC, op 0x18 at +0x8. */
        vtxbase = mdoff(md32(data + dataoff + (opcode == 0x04 ? 0xC : 0x8)));
    }

    if (prioff != 0 && prioff < maxlen)
    {
        MdlWalkGdl(b, data, maxlen, prioff, vtxbase, 0, pose, origin, &state, &material);
    }

    if (secoff != 0 && secoff < maxlen)
    {
        /* Type 3 keeps the primary combiner; type 4 reinstalls the standard
           combiner. Both secondary passes start with translucent depth state. */
        BgRenderState defaults;
        BgRenderStateInit(&defaults, TRUE);
        state.othermode = defaults.othermode;
        if (modeltype == 4)
        {
            material.combineword0 = 0xFC26A004u;
            material.combineword1 = 0x1F1093FFu;
        }
        MdlWalkGdl(b, data, maxlen, secoff, vtxbase, BG_TRI_SECONDARY, pose, origin,
                   &state, &material);
    }
}

/* Raw model files have no ModelFileHeader. Reuse the mesh extraction path's
   segment-5 root-node discovery for placement and attachment data. */
static DWORD ModelFindRootNode(const unsigned char *data, DWORD size)
{
    DWORD probe;

    for (probe = 0; probe + 24 <= size && probe < 0x200; probe += 4)
    {
        DWORD opcode = (unsigned short)md16(data + probe) & 0xff;
        DWORD dataptr = md32(data + probe + 4);

        if (opcode >= 1 && opcode <= 0x20 && (dataptr >> 24) == 0x05
            && mdoff(dataptr) < size && md32(data + probe + 8) == 0)
        {
            return probe;
        }
    }
    return 0;
}

BOOL ModelReadPlacementBounds(const unsigned char *data, DWORD size,
                               float min[3], float max[3])
{
    DWORD root, first, node;
    int depth, visited = 0;

    if (data == NULL || size < 40) { return FALSE; }
    root = ModelFindRootNode(data, size);
    if (root == 0) { return FALSE; }
    first = mdoff(md32(data + root + 20));
    /* Game lookup: root child siblings, then first child's child siblings. */
    for (depth = 0; depth < 2; depth++)
    {
        for (node = first; node != 0 && node <= size - 24 && visited++ < MDL_MAX_NODES;
             node = mdoff(md32(data + node + 12)))
        {
            if ((unsigned short)md16(data + node) == 0x0a) /* MODELNODE_OPCODE_BBOX */
            {
                DWORD offset = mdoff(md32(data + node + 4));
                int axis;

                if (offset == 0 || size < 28 || offset > size - 28) { return FALSE; }
                for (axis = 0; axis < 3; axis++)
                {
                    union { DWORD bits; float value; } lo, hi;

                    lo.bits = md32(data + offset + 4 + axis * 8);
                    hi.bits = md32(data + offset + 8 + axis * 8);
                    if (!isfinite(lo.value) || !isfinite(hi.value) || lo.value > hi.value)
                    {
                        return FALSE;
                    }
                    min[axis] = lo.value;
                    max[axis] = hi.value;
                }
                return TRUE;
            }
        }
        if (first == 0 || first > size - 24) { break; }
        first = mdoff(md32(data + first + 20));
    }
    return FALSE;
}

/* Character assets contain several distance variants of the same body part.
   Export the distance-zero branch so flattened glTFs contain one visible
   surface per part, rather than overlapping near and far meshes. */
static BOOL MdlNodeInClosestLod(const unsigned char *data, DWORD size, DWORD node)
{
    int visited = 0;

    while (node != 0)
    {
        if (node > size - 24 || visited++ >= MDL_MAX_NODES) { return FALSE; }
        if (((unsigned short)md16(data + node) & 0xff) == 0x08)
        {
            DWORD offset = mdoff(md32(data + node + 4));
            union { DWORD bits; float value; } minimum, maximum;

            if (offset == 0 || offset > size - 8) { return FALSE; }
            minimum.bits = md32(data + offset);
            maximum.bits = md32(data + offset + 4);
            if (!isfinite(minimum.value) || !isfinite(maximum.value)
                || minimum.value > 0.0f || maximum.value < 0.0f)
            {
                return FALSE;
            }
        }
        node = mdoff(md32(data + node + 8));
    }
    return TRUE;
}

BOOL ModelReadHeadAttachment(const unsigned char *data, DWORD size, float position[3])
{
    DWORD stack[MDL_MAX_NODES];
    DWORD root;
    int count = 0, visited = 0;

    if (data == NULL || size < 40) { return FALSE; }
    root = ModelFindRootNode(data, size);
    if (root == 0) { return FALSE; }
    stack[count++] = root;
    while (count > 0 && visited++ < MDL_MAX_NODES)
    {
        DWORD node = stack[--count];
        DWORD next, child;

        if (node == 0 || node > size - 24) { continue; }
        if (((unsigned short)md16(data + node) & 0xff) == 0x17)
        {
            return MdlNodeTranslation(data, size, node, position);
        }
        next = mdoff(md32(data + node + 12));
        child = mdoff(md32(data + node + 20));
        if (count + 2 <= MDL_MAX_NODES)
        {
            if (next != 0) { stack[count++] = next; }
            if (child != 0) { stack[count++] = child; }
        }
    }
    return FALSE;
}

BOOL ModelReadSwitchAttachment(const unsigned char *data, DWORD size,
                                int switchcount, int index, float position[3])
{
    DWORD pointer, node;

    if (data == NULL || size < 24 || switchcount <= 0
        || (DWORD)switchcount > size / 4 || index < 0 || index >= switchcount)
    {
        return FALSE;
    }
    pointer = md32(data + (DWORD)index * 4);
    node = mdoff(pointer);
    if ((pointer >> 24) != 5 || node == 0 || node > size - 24) { return FALSE; }
    return MdlNodeTranslation(data, size, node, position);
}

BOOL ModelReadHeldPlacement(const unsigned char *data, DWORD size,
                             float origin[3], BOOL *usesmodelscale)
{
    DWORD root, opcode, offset;
    int axis;

    if (data == NULL || size < 40) { return FALSE; }
    root = ModelFindRootNode(data, size);
    if (root == 0) { return FALSE; }
    opcode = (unsigned short)md16(data + root) & 0xff;
    *usesmodelscale = opcode == 0x01;
    origin[0] = origin[1] = origin[2] = 0.0f;
    if (*usesmodelscale) { return TRUE; }
    if (opcode != 0x02 && opcode != 0x03 && opcode != 0x15) { return FALSE; }
    offset = mdoff(md32(data + root + 4));
    if (offset == 0 || offset > size - 12) { return FALSE; }
    for (axis = 0; axis < 3; axis++)
    {
        union { DWORD bits; float value; } coordinate;

        coordinate.bits = md32(data + offset + axis * 4);
        if (!isfinite(coordinate.value)) { return FALSE; }
        origin[axis] = coordinate.value;
    }
    return TRUE;
}

static BgVertex *MdlLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            BgRenderFlags **renderflags,
                            const char **reasonout, BOOL closestlod)
{
    MdlBuilder b;
    MdlPose pose;
    DWORD nodes[MDL_MAX_NODES];
    int nodecount = 0;
    int i;
    DWORD stack[MDL_MAX_NODES];
    int sp = 0;
    int visited = 0;
    DWORD rootoff;

    *tricount = 0;
    *texids = NULL;
    *renderflags = NULL;
    *reasonout = "";

    if (data == NULL || maxlen < 40)
    {
        *reasonout = "model file too small for a node tree.";
        return NULL;
    }

    /*
     * GE model files carry no header - it lives in the game's static
     * per-model tables (load_object_fill_header is the spec). The file
     * is [switch ptrs][texture table][RootNode][...], and the counts
     * that locate RootNode are in code we don't have here. So: scan
     * the first bytes for the first thing shaped like a node - small
     * u16 opcode, a segment-5 Data pointer, and a NULL Parent, which
     * the root uniquely has.
     */
    rootoff = ModelFindRootNode(data, maxlen);

    if (rootoff == 0)
    {
        *reasonout = "no root node found in model file.";
        return NULL;
    }

    ZeroMemory(&b, sizeof(b));
    ZeroMemory(&pose, sizeof(pose));
    stack[sp++] = rootoff;

    while (sp > 0 && visited < MDL_MAX_NODES)
    {
        DWORD node = stack[--sp];

        DWORD childoff;
        DWORD nextoff;

        if (node == 0 || node + 24 > maxlen)
        {
            continue;
        }

        visited++;
        for (i = 0; i < nodecount; i++)
        {
            if (nodes[i] == node) { break; }
        }
        if (i < nodecount) { continue; }
        nodes[nodecount++] = node;
        nextoff = mdoff(md32(data + node + 12));
        childoff = mdoff(md32(data + node + 20));

        if (!MdlAddNodeMatrices(&pose, data, maxlen, node))
        {
            b.error = "model has an invalid node transform.";
            break;
        }

        if (sp + 2 <= MDL_MAX_NODES)
        {
            if (nextoff != 0)  { stack[sp++] = nextoff; }
            if (childoff != 0) { stack[sp++] = childoff; }
        }
    }

    if (sp > 0 && b.error == NULL)
    {
        b.error = "model node tree exceeds the extraction limit.";
    }
    /* Detached heads borrow matrix 0 from their character in-game. Export
       these standalone models at the origin until attached to a body. */
    if (!pose.hasmatrices) { pose.matrices[0].valid = TRUE; }
    for (i = 0; i < nodecount && b.error == NULL; i++)
    {
        DWORD node = nodes[i];
        DWORD flags = (unsigned short)md16(data + node);
        DWORD dataoff = mdoff(md32(data + node + 4));
        float origin[3] = { 0.0f, 0.0f, 0.0f };

        if (!MdlNodeTranslation(data, maxlen, node, origin))
        {
            b.error = "model has an invalid node transform.";
            break;
        }
        if (!closestlod || MdlNodeInClosestLod(data, maxlen, node))
        {
            MdlNodeMeshes(&b, data, maxlen, flags & 0xff, dataoff, &pose, origin);
        }
    }

    if (b.error || b.count == 0)
    {
        free(b.verts);
        free(b.texids);
        free(b.renderflags);
        *reasonout = b.error ? b.error
                              : "model produced no triangles.";
        return NULL;
    }

    *tricount = b.count / 3;
    *texids = b.texids;
    *renderflags = b.renderflags;
    return b.verts;
}

BgVertex *ModelLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            BgRenderFlags **renderflags,
                            const char **reasonout)
{
    return MdlLoadGeometry(data, maxlen, tricount, texids, renderflags, reasonout, FALSE);
}

BgVertex *ModelLoadCharacterGeometry(const unsigned char *data, DWORD maxlen,
                                     DWORD *tricount, unsigned short **texids,
                                     BgRenderFlags **renderflags,
                                     const char **reasonout)
{
    return MdlLoadGeometry(data, maxlen, tricount, texids, renderflags, reasonout, TRUE);
}

static const char *MdlClassFolder(const char *name)
{
    if (strcmp(name, "GcartblueZ") == 0 || strcmp(name, "GcartridgeZ") == 0
        || strcmp(name, "GcartrifleZ") == 0 || strcmp(name, "GcartshellZ") == 0)
    {
        return "casings";
    }

    switch (name[0])
    {
    case 'C': return "characters";
    case 'G': return "guns";
    case 'P': return "objects";
    }

    return NULL; /* setups, stans, text, bg - not models */
}

DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout)
{
    char dir[MAX_PATH];
    char path[MAX_PATH];
    char name[64];
    DWORD written = 0;
    DWORD i;
    static const char *classes[] = { "characters", "guns", "objects", "casings" };

    *reasonout = "";

    wsprintf(dir, "%s\\models", projectdir);
    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the models folder could not be created.";
        return 0;
    }

    for (i = 0; i < 4; i++)
    {
        wsprintf(path, "%s\\models\\%s", projectdir, classes[i]);
        CreateDirectory(path, NULL);
    }

    for (i = 0; RomGetFileByIndex(rom, i, name, sizeof(name), NULL, NULL); i++)
    {
        const char *cls = MdlClassFolder(name);
        DWORD offset;
        DWORD maxlen;
        DWORD tricount = 0;
        unsigned short *texids = NULL;
        BgRenderFlags *renderflags = NULL;
        BgVertex *tris;
        const char *why = "";

        if (cls == NULL || strchr(name, '/') != NULL)
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, i, name, sizeof(name), &offset, &maxlen))
        {
            continue;
        }

        tris = name[0] == 'C'
            ? ModelLoadCharacterGeometry(rom->data + offset, maxlen,
                                         &tricount, &texids, &renderflags, &why)
            : ModelLoadGeometry(rom->data + offset, maxlen,
                                 &tricount, &texids, &renderflags, &why);

        if (tris != NULL)
        {
            wsprintf(path, "%s\\models\\%s\\%s.gltf", projectdir, cls, name);

            if (GltfWriteModel(path, projectdir, tris, texids, renderflags,
                               tricount, &why))
            {
                written++;
            }

            free(tris);
            free(texids);
            free(renderflags);
        }
    }

    if (written == 0)
    {
        *reasonout = "no models could be extracted.";
    }

    return written;
}

BOOL ModelGetPropDefinition(int modelid, const char **nameout,
                            float *scaleout)
{
    if (modelid < 0 || modelid >= PROP_MODEL_COUNT)
    {
        return FALSE;
    }

    if (nameout != NULL)
    {
        *nameout = g_PropModelDefinitions[modelid].filename;
    }
    if (scaleout != NULL)
    {
        *scaleout = g_PropModelDefinitions[modelid].scale;
    }

    return TRUE;
}

BgVertex *ModelLoadProjectGeometry(const char *projectdir, int modelid,
                                   DWORD *tricount,
                                   unsigned short **tritags,
                                   BgRenderFlags **renderflags,
                                   float *modelscale,
                                   const char **reasonout)
{
    const char *name;
    char path[MAX_PATH];
    FILE *file = NULL;
    char line[512];
    DWORD vertexcount = 0;
    DWORD facecount = 0;
    DWORD i;
    BOOL hastexturetags = FALSE;
    BOOL inface = FALSE;
    BgVertex *source = NULL;
    BgVertex *result = NULL;
    unsigned short *tags = NULL;
    BgRenderFlags *flags = NULL;
    int pathlength;

    *tricount = 0;
    *tritags = NULL;
    *renderflags = NULL;
    *reasonout = "";

    if (!ModelGetPropDefinition(modelid, &name, modelscale))
    {
        *reasonout = "the setup references an unknown prop model ID.";
        return NULL;
    }

    pathlength = snprintf(path, sizeof(path),
                          "%s\\models\\objects\\%s.gltf",
                          projectdir, name);
    if (pathlength < 0 || pathlength >= (int)sizeof(path))
    {
        *reasonout = "the object model path is too long.";
        return NULL;
    }

    /* New projects use glTF. Keep the PLY reader below as a compatibility
       path so existing projects remain openable without re-extraction. */
    file = fopen(path, "rb");
    if (file != NULL)
    {
        fclose(file);
        return GltfLoadModel(path, projectdir, tricount,
                             tritags, renderflags, reasonout);
    }

    pathlength = snprintf(path, sizeof(path),
                          "%s\\models\\objects\\%s.ply",
                          projectdir, name);
    if (pathlength < 0 || pathlength >= (int)sizeof(path))
    {
        *reasonout = "the object model path is too long.";
        return NULL;
    }

    file = fopen(path, "r");
    if (file == NULL)
    {
        *reasonout = "the object's extracted model file is missing.";
        return NULL;
    }

    if (fgets(line, sizeof(line), file) == NULL || strcmp(line, "ply\n") != 0)
    {
        *reasonout = "the object model is not an ASCII PLY file.";
        goto fail;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        unsigned long count;

        if (sscanf(line, "element vertex %lu", &count) == 1)
        {
            vertexcount = (DWORD)count;
            inface = FALSE;
        }
        else if (sscanf(line, "element face %lu", &count) == 1)
        {
            facecount = (DWORD)count;
            inface = TRUE;
        }
        else if (inface && strstr(line, "texture_tag") != NULL)
        {
            hastexturetags = TRUE;
        }
        else if (strncmp(line, "end_header", 10) == 0)
        {
            break;
        }
    }

    if (vertexcount == 0 || facecount == 0
        || vertexcount > 3000000u || facecount > 1000000u)
    {
        *reasonout = "the object model has invalid PLY counts.";
        goto fail;
    }

    source = (BgVertex *)calloc((size_t)vertexcount, sizeof(*source));
    result = (BgVertex *)malloc((size_t)facecount * 3 * sizeof(*result));
    tags = (unsigned short *)malloc((size_t)facecount * sizeof(*tags));
    flags = (BgRenderFlags *)malloc((size_t)facecount * sizeof(*flags));
    if (source == NULL || result == NULL || tags == NULL || flags == NULL)
    {
        *reasonout = "out of memory loading an object model.";
        goto fail;
    }

    for (i = 0; i < vertexcount; i++)
    {
        unsigned int r, g, b, a;

        if (fgets(line, sizeof(line), file) == NULL
            || sscanf(line, "%f %f %f %u %u %u %u %f %f",
                      &source[i].x, &source[i].y, &source[i].z,
                      &r, &g, &b, &a, &source[i].s, &source[i].t) != 9
            || r > 255 || g > 255 || b > 255 || a > 255)
        {
            *reasonout = "the object model has invalid PLY vertices.";
            goto fail;
        }

        source[i].r = (unsigned char)r;
        source[i].g = (unsigned char)g;
        source[i].b = (unsigned char)b;
        source[i].a = (unsigned char)a;
    }

    for (i = 0; i < facecount; i++)
    {
        unsigned long a, b, c;
        unsigned int tag = BG_TEX_NONE;
        int fields;

        if (fgets(line, sizeof(line), file) == NULL)
        {
            *reasonout = "the object model ends inside its PLY faces.";
            goto fail;
        }

        fields = sscanf(line, "3 %lu %lu %lu %u", &a, &b, &c, &tag);
        if (fields < 3 || a >= vertexcount || b >= vertexcount
            || c >= vertexcount || (hastexturetags && fields != 4)
            || tag > 0xffffu)
        {
            *reasonout = "the object model has invalid PLY faces.";
            goto fail;
        }

        result[i * 3 + 0] = source[a];
        result[i * 3 + 1] = source[b];
        result[i * 3 + 2] = source[c];
        tags[i] = hastexturetags ? (unsigned short)tag : BG_TEX_NONE;
        flags[i] = BgRenderDefaultFlags(BG_TRI_IS_SECONDARY(tags[i]));
    }

    fclose(file);
    free(source);
    *tricount = facecount;
    *tritags = tags;
    *renderflags = flags;
    return result;

fail:
    if (file != NULL)
    {
        fclose(file);
    }
    free(source);
    free(result);
    free(tags);
    free(flags);
    return NULL;
}
