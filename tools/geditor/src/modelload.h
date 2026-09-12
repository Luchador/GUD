#ifndef GEDITOR_MODELLOAD_H
#define GEDITOR_MODELLOAD_H

#include <windows.h>

#include "rom.h"
#include "bgload.h"
#include "bgrender.h"

/* Row-vector affine transform, matching the game's model matrices. */
typedef struct ModelTransform {
    float m[4][3];
} ModelTransform;

typedef struct ModelCharacterAttachments {
    ModelTransform head;
    ModelTransform hands[2]; /* right, left */
    BOOL hashead;
    BOOL hashands[2];
} ModelCharacterAttachments;

void ModelTransformIdentity(ModelTransform *transform);
void ModelTransformVertex(const ModelTransform *transform, BgVertex *vertex);

/* Apply one guard-skeleton animation frame to a preview copy of an exported
   body. UVs/colors/materials stay in project space. Unmatched custom geometry
   is left untouched and returns FALSE. Attachments are updated only on success.
   The 45 channels are the game's uncompressed u16 angles, with no root motion. */
BOOL ModelApplyCharacterPose(const unsigned char *data, DWORD size, int switchcount,
                              const unsigned short angles[45], BOOL flip,
                              BgVertex *vertices, DWORD tricount,
                              ModelCharacterAttachments *attachments);

/* Stable source associations for a Blender round trip. Native vertex offsets
 * retain joint-local positions and shared storage independently of glTF's
 * per-corner identities. The native tree, switches and bounds stay authored. */
typedef struct ModelSourceFace {
    DWORD command, list;
    unsigned char slot, normalmask; /* RGB holds normals for these corner bits. */
    BOOL closest;
    BgMaterial material;
    BgRenderState state; /* Draw-time constants used to interpret vertex alpha. */
} ModelSourceFace;

typedef struct ModelSourceList {
    DWORD pointer, offset, end, vertexbase;
    DWORD vertexpointer; /* Native Vertices field; its count follows at +4. */
    DWORD pointusagepointer; /* Collision node's per-vertex blood-stain links. */
    int modeltype; /* Native renderer setup: 1 = one cycle, 2/3/4 = two cycles. */
    BOOL preserve; /* Dynamic effects such as rotating muzzle flashes. */
    BgMaterial initial;
} ModelSourceList;

typedef struct ModelSource {
    BgVertex *vertices;
    DWORD *vertexoffsets; /* One native 16-byte Vtx offset per exported corner. */
    unsigned short *tags;
    BgRenderFlags *flags;
    ModelSourceFace *faces;
    ModelSourceList *lists;
    DWORD count, listcount;
    BOOL closestpreview;
} ModelSource;

BOOL ModelReadSource(const unsigned char *data, DWORD size, ModelSource *source,
                      const char **reasonout);
void ModelFreeSource(ModelSource *source);

/*
 * GoldenEye model (P/C/G file) geometry extraction.
 *
 * Parses a raw model file's node tree, collects every mesh node's
 * display lists, and emits the same textured/colored triangle soup
 * bgload produces. The unanimated pose accumulates parent node offsets
 * and applies display-list matrix selection when vertices are loaded.
 * Positions are baked relative to the root; setup placement/scale is separate.
 * Runtime joint animation and attachment to other models are not evaluated.
 */
BgVertex *ModelLoadGeometry(const unsigned char *data, DWORD maxlen,
                            DWORD *tricount, unsigned short **texids,
                            BgRenderFlags **renderflags,
                            const char **reasonout);

/* Character exports retain only the closest LOD of each body part. */
BgVertex *ModelLoadCharacterGeometry(const unsigned char *data, DWORD maxlen,
                                     DWORD *tricount, unsigned short **texids,
                                     BgRenderFlags **renderflags,
                                     const char **reasonout);

/* Model-space position of MODELNODE_OPCODE_HEAD in the unanimated body. */
BOOL ModelReadHeadAttachment(const unsigned char *data, DWORD size,
                             float position[3]);

/* Switch-table node position in the unanimated model. The switch count
   comes from its static ModelFileHeader, not the raw model file. */
BOOL ModelReadSwitchAttachment(const unsigned char *data, DWORD size,
                                int switchcount, int index, float position[3]);

/* Four native screen vertices in switch order. monitorProcessAndRender
   uses the root render matrix, bypassing the switch node's transform. */
BOOL ModelReadMonitorScreen(const unsigned char *data, DWORD size, int index, BgVertex quad[4]);

/* Held models use their authored root offset, which flattened project
   geometry omits for ordinary object placement. GROUP roots inherit the
   hand's scale directly; only HEADER roots also apply their own model scale. */
BOOL ModelReadHeldPlacement(const unsigned char *data, DWORD size,
                             float origin[3], BOOL *usesmodelscale);

/* Looks up a setup ObjectRecord's model ID in GoldenEye's canonical
   PitemZ_entries table. The returned name is the extracted P...Z
   resource filename and scale is the model's authored base scale. */
BOOL ModelGetPropDefinition(int modelid, const char **nameout,
                            float *scaleout);

/* Read the authored collision/placement box selected by
   chrobjGetBboxFromObjFile. It can differ from the visible mesh bounds. */
BOOL ModelReadPlacementBounds(const unsigned char *data, DWORD size,
                               float min[3], float max[3]);

/* Loads one extracted object-model glTF or its native replacement. Caller
   frees the returned vertex, tag and preview render-flag arrays. */
BgVertex *ModelLoadProjectGeometry(const char *projectdir, int modelid,
                                   DWORD *tricount,
                                   unsigned short **tritags,
                                   BgRenderFlags **renderflags,
                                   float *modelscale,
                                   const char **reasonout);

/* Load any extracted model by its class folder and filename stem. The viewer
   uses the same glTF/native replacement path as placed props, without setup scaling. */
BgVertex *ModelLoadProjectNamedGeometry(const char *projectdir, const char *folder,
                                        const char *name, DWORD *tricount,
                                        unsigned short **tritags,
                                        BgRenderFlags **renderflags,
                                        const char **reasonout);

/*
 * Extracts every model in the ROM's file table into
 * <projectdir>\models\{characters,guns,objects,casings}\<name>.gltf.
 * Files are self-contained glTF 2.0 assets with positions, UVs, vertex colors,
 * and GoldenEye texture/depth/blending metadata. Returns models written; 0 with *reasonout set
 * on total failure. Unparseable models are skipped.
 */
DWORD ModelExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout);

#endif /* GEDITOR_MODELLOAD_H */
