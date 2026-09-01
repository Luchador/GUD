#ifndef _VTXSTORE_H_
#define _VTXSTORE_H_
#include <ultra64.h>
#include "bondtypes.h"

typedef enum VtxStoreType
{
    VTXSTORE_TYPE_CHR = 0xCCCC,
    VTXSTORE_TYPE_OBJ = 0x0B0B
} VtxStoreType;

void vtxstoreReset(void);
void vtxstoreTick(void);
Vertex *vtxstoreAllocate(s32 vertexCount, VtxStoreType type, ModelFileHeader *modelFile, s32 destroyedLevel);
void vtxstoreFree(Vertex *vertices);

#endif
