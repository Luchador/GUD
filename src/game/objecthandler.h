#ifndef _OBJECTHANDLER_
#define _OBJECTHANDLER_
#include <ultra64.h>
#include <bondtypes.h>
#include <image.h>


#define MODEL_HIT_ENTRY_POOL_SIZE 600

struct ModelAnimationScratch {
    char *bufferStart;
    char *nextFree;
};

extern struct ModelHitEntry g_ModelHitEntries[MODEL_HIT_ENTRY_POOL_SIZE];
extern struct ModelHitEntry *g_ModelHitFreeList;
extern s32 g_ModelDistanceDisabled;
extern f32 g_ModelDistanceScale;
extern u32 g_ModelAnimMergingEnabled;
extern struct ModelAnimationScratch *g_ModelAnimationScratch;
extern s32 g_ModelShadowAlpha;

extern coord3d g_ModelZeroVector;

extern struct Vertex* (*g_ModelVertexAllocator)(s32 numVertices);
extern void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);
extern Vertex g_GunfireVertexTemplate;
extern Vtx g_ShadowVertexTemplate;

void fileLoad(ModelFileHeader *header,char *name);
void initAnimationsBuffer(struct ModelAnimationScratch *animBuffer);
PropRecord *chrGiveWeapon(ChrRecord *self, s32 PropID, ITEM_IDS ItemID, s32 flags);

/* Depth-sorted model-node lists used for rendering and hit tests. */
ModelHitEntry *modelHitBuildNodeList(ModelHitEntry *head, Model *model);
void modelHitFreeChain(ModelHitEntry *entry);
void modelHitCalculateNodeDepths(ModelHitEntry *head);
ModelHitEntry *modelHitSortByDepth(ModelHitEntry *head);
void modelHitRenderNodeList(ModelRenderData *renderData, ModelHitEntry *head);
s32 modelHitFindNextBBoxHit(ModelHitEntry **entry, coord3d *rayOrigin, coord3d *rayDirection, Model **hitModel, ModelNode **hitNode);
s32 modelHitFindFirstBBoxHit(ModelHitEntry **entry, coord3d *rayOrigin, coord3d *rayDirection, Model **hitModel, ModelNode **hitNode);

void load_object_fill_header(struct ModelFileHeader *objheader, u8 *name, u8* dst, s32 size, struct texpool * buffer);


#endif
