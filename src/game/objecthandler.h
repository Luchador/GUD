#ifndef _OBJECTHANDLER_
#define _OBJECTHANDLER_
#include <ultra64.h>
#include <bondtypes.h>
#include <image.h>

struct ModelAnimationScratch {
    char *reserved;
    char *bufferStart;
    char *nextFree;
};

struct AnimModelSlot {
    s16 unk00;
    s16 unk02;
    s32 unk04;
    s32 unk08;
    s32 unk0c;
    void *unk10;
    s32 unk14;
    s32 unk18;
    s32 unk1c;
    s32 unk20;
    s32 unk24;
    s32 unk28;
    s32 unk2c;
    s32 unk30;
    s32 unk34;
    s32 unk38;
    s32 unk3c;
    s32 unk40;
    s32 unk44;
    s32 unk48;
    s32 unk4c;
    s32 unk50;
    s32 unk54;
    s32 unk58;
    s32 unk5c;
    s32 unk60;
    s32 unk64;
    s32 unk68;
    s32 unk6c;
    s32 unk70;
    s32 unk74;
    s32 unk78;
    s32 unk7c;
    s32 unk80;
    s32 unk84;
    s32 unk88;
    s32 unk8c;
    s32 unk90;
    s32 unk94;
    s32 unk98;
    s32 unk9c;
    s32 posValidateFunc;
    s32 unka4;
    s32 unka8;
    s32 unkac;
    s32 unkb0;
    s32 unkb4;
    s32 unkb8;

    // is this struct size 0xbc or 0xc0 ?
    //s32 unkbc;
};

struct ModelSlot {
    s16 unk00;
    s16 unk02;
    s32 unk04;
    s32 unk08;
    s32 unk0c;
    void *unk10;
    s32 unk14;
    s32 unk18;
    s32 unk1c;
};

extern struct AnimModelSlot *g_AnimModelSlots;
extern struct ModelSlot *g_ModelSlots;

#define MODEL_HIT_ENTRY_POOL_SIZE 600

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
void load_object_into_memory_unused_maybe(ModelFileHeader *header,int *recallstring,int *targetloc,int sizeleft);
void initAnimationsBuffer(struct ModelAnimationScratch *animBuffer);

// tentative signature
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
