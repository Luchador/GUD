#include <ultra64.h>
#include <memp.h>
#include "bondconstants.h"
#include "bondtypes.h"
#include "vtxstore.h"
#include "propobj.h"
#include "model.h"


#define VTXSTORE_BLOCK_UNUSED -1
#define VTXSTORE_BLOCK_FREE    0

typedef struct VtxStoreBlock
{
    Vertex *vertices;
    ModelFileHeader *modelFile;
    s32 destroyedLevel;
    s16 vertexCount;
    s16 refCount;
    s16 nextIndex;
    s16 previousIndex;
} VtxStoreBlock;


static s32 g_ChrVtxStoreVertexCapacity;
static s32 g_ChrVtxStoreBlockCapacity;
static s32 g_ObjVtxStoreVertexCapacity;
static s32 g_ObjVtxStoreBlockCapacity;
static Vertex *g_ChrVtxStoreVertices;
static Vertex *g_ObjVtxStoreVertices;
static VtxStoreBlock *g_ChrVtxStoreBlocks;
static VtxStoreBlock *g_ObjVtxStoreBlocks;
static s16 g_ChrVtxStoreFreeVertices;
static s16 g_ObjVtxStoreFreeVertices;


/**
 * Initialize the fixed vertex stores used for character damage and destroyed
 * object deformation.
 */
void vtxstoreReset(void)
{
    LEVELID stage;
    s32 i;

    if (getPlayerCount() >= 2)
    {
        g_ChrVtxStoreVertexCapacity = 3000;
        g_ChrVtxStoreBlockCapacity = 80;
        g_ObjVtxStoreVertexCapacity = 500;
        g_ObjVtxStoreBlockCapacity = 20;
    }
    else
    {
        stage = lvlGetCurrentStageToLoad();

        g_ChrVtxStoreVertexCapacity = 500;
        g_ChrVtxStoreBlockCapacity = 20;

        g_ObjVtxStoreVertexCapacity = 1500;
        g_ObjVtxStoreBlockCapacity = 40;
    }

    g_ChrVtxStoreBlocks = mempAllocBytesInBank(g_ChrVtxStoreBlockCapacity * sizeof(VtxStoreBlock), MEMPOOL_STAGE);
    g_ChrVtxStoreVertices = mempAllocBytesInBank(g_ChrVtxStoreVertexCapacity * sizeof(Vertex), MEMPOOL_STAGE);
    g_ObjVtxStoreBlocks = mempAllocBytesInBank(g_ObjVtxStoreBlockCapacity * sizeof(VtxStoreBlock), MEMPOOL_STAGE);
    g_ObjVtxStoreVertices = mempAllocBytesInBank(g_ObjVtxStoreVertexCapacity * sizeof(Vertex), MEMPOOL_STAGE);

    g_ChrVtxStoreFreeVertices = g_ChrVtxStoreVertexCapacity;
    g_ChrVtxStoreBlocks[0].vertices = g_ChrVtxStoreVertices;
    g_ChrVtxStoreBlocks[0].vertexCount = g_ChrVtxStoreVertexCapacity;
    g_ChrVtxStoreBlocks[0].refCount = VTXSTORE_BLOCK_FREE;
    g_ChrVtxStoreBlocks[0].nextIndex = -1;
    g_ChrVtxStoreBlocks[0].previousIndex = -1;

    for (i = 1; i < g_ChrVtxStoreBlockCapacity; i++)
    {
        g_ChrVtxStoreBlocks[i].refCount = VTXSTORE_BLOCK_UNUSED;
    }

    g_ObjVtxStoreFreeVertices = g_ObjVtxStoreVertexCapacity;
    g_ObjVtxStoreBlocks[0].vertices = g_ObjVtxStoreVertices;
    g_ObjVtxStoreBlocks[0].vertexCount = g_ObjVtxStoreVertexCapacity;
    g_ObjVtxStoreBlocks[0].refCount = VTXSTORE_BLOCK_FREE;
    g_ObjVtxStoreBlocks[0].nextIndex = -1;
    g_ObjVtxStoreBlocks[0].previousIndex = -1;

    for (i = 1; i < g_ObjVtxStoreBlockCapacity; i++)
    {
        g_ObjVtxStoreBlocks[i].refCount = VTXSTORE_BLOCK_UNUSED;
    }
}


/**
 * Replace model references to one object-deformation allocation with another.
 */
static void vtxstoreFixRefs(Vertex *find, Vertex *replacement)
{
    PropRecord *prop;
    ObjectRecord *obj;
    union ModelRwData *rwdata;
    ModelNode *node;
    ModelFileHeader *modelFile;

    prop = chrpropGetActiveTail();

    while (prop != NULL)
    {
        if (prop->type == PROP_TYPE_OBJ)
        {
            obj = prop->obj;
            modelFile = obj->model->obj;
            node = modelFile->RootNode;

            while (node != NULL)
            {
                if ((node->Opcode & 0xff) == MODELNODE_OPCODE_DLCOLLISION)
                {
                    rwdata = modelGetNodeRwData(obj->model, node);

                    if (rwdata->DisplayListCollisions.Vertices == find)
                    {
                        rwdata->DisplayListCollisions.Vertices = replacement;
                    }

                    break;
                }

                if (node->Child != NULL)
                {
                    node = node->Child;
                }
                else
                {
                    while (node != NULL)
                    {
                        if (node->Next != NULL)
                        {
                            node = node->Next;
                            break;
                        }

                        node = node->Parent;
                    }
                }
            }
        }

        prop = prop->prev;
    }
}


/**
 * Merge duplicate object-deformation allocations when the object vertex store
 * is running low, then ask the object system to release an offscreen allocation
 * if necessary.
 */
void vtxstoreTick(void)
{
    VtxStoreBlock *originalBlock;
    VtxStoreBlock *duplicateBlock;
    s16 originalIndex;
    s16 duplicateIndex;
    s16 blockIndex;
    s16 nextIndex;
    s32 mergedDuplicates;

    mergedDuplicates = FALSE;

    if (g_ObjVtxStoreFreeVertices < (g_ObjVtxStoreVertexCapacity >> 2))
    {
        for (originalIndex = 0; originalIndex < g_ObjVtxStoreBlockCapacity - 1; originalIndex++)
        {
            originalBlock = &g_ObjVtxStoreBlocks[originalIndex];

            if (originalBlock->refCount > 0)
            {
                for (duplicateIndex = originalIndex + 1; duplicateIndex < g_ObjVtxStoreBlockCapacity; duplicateIndex++)
                {
                    duplicateBlock = &g_ObjVtxStoreBlocks[duplicateIndex];

                    if (duplicateBlock->refCount > 0
                        && originalBlock->modelFile == duplicateBlock->modelFile
                        && originalBlock->destroyedLevel == duplicateBlock->destroyedLevel)
                    {
                        vtxstoreFixRefs(duplicateBlock->vertices, originalBlock->vertices);

                        originalBlock->refCount += duplicateBlock->refCount;
                        duplicateBlock->refCount = VTXSTORE_BLOCK_FREE;
                        g_ObjVtxStoreFreeVertices += duplicateBlock->vertexCount;
                        mergedDuplicates = TRUE;
                    }
                }
            }
        }
    }

    if (mergedDuplicates)
    {
        blockIndex = 0;

        while (blockIndex >= 0)
        {
            nextIndex = g_ObjVtxStoreBlocks[blockIndex].nextIndex;

            if (nextIndex >= 0
                && g_ObjVtxStoreBlocks[blockIndex].refCount == VTXSTORE_BLOCK_FREE
                && g_ObjVtxStoreBlocks[nextIndex].refCount == VTXSTORE_BLOCK_FREE)
            {
                g_ObjVtxStoreBlocks[blockIndex].vertexCount += g_ObjVtxStoreBlocks[nextIndex].vertexCount;
                g_ObjVtxStoreBlocks[nextIndex].refCount = VTXSTORE_BLOCK_UNUSED;
                g_ObjVtxStoreBlocks[blockIndex].nextIndex = g_ObjVtxStoreBlocks[nextIndex].nextIndex;
                nextIndex = g_ObjVtxStoreBlocks[blockIndex].nextIndex;

                if (nextIndex >= 0)
                {
                    g_ObjVtxStoreBlocks[nextIndex].previousIndex = blockIndex;
                }
            }
            else
            {
                blockIndex = nextIndex;
            }
        }
    }

    if (g_ObjVtxStoreFreeVertices < (g_ObjVtxStoreVertexCapacity >> 2))
    {
        sub_GAME_7F056690();
    }
}


/**
 * Allocate a contiguous run of vertices from the requested store.
 */
Vertex *vtxstoreAllocate(s32 vertexCount, VtxStoreType type, ModelFileHeader *modelFile, s32 destroyedLevel)
{
    VtxStoreBlock *blocks;
    VtxStoreBlock *block;
    s16 *freeVertexCount;
    s16 blockIndex;
    s16 unusedIndex;
    s16 originalVertexCount;
    s32 blockCapacity;
    s32 visitedBlocks;
    s32 splitBlock;

    switch (type)
    {
        case VTXSTORE_TYPE_CHR:
            blocks = g_ChrVtxStoreBlocks;
            freeVertexCount = &g_ChrVtxStoreFreeVertices;
            blockCapacity = g_ChrVtxStoreBlockCapacity;
            break;

        case VTXSTORE_TYPE_OBJ:
            blocks = g_ObjVtxStoreBlocks;
            freeVertexCount = &g_ObjVtxStoreFreeVertices;
            blockCapacity = g_ObjVtxStoreBlockCapacity;
            break;

        default:
            return NULL;
    }

    blockIndex = 0;
    visitedBlocks = 0;

    while (blockIndex >= 0 && visitedBlocks <= blockCapacity)
    {
        block = &blocks[blockIndex];

        if (block->refCount == VTXSTORE_BLOCK_FREE && block->vertexCount >= vertexCount)
        {
            break;
        }

        blockIndex = block->nextIndex;
        visitedBlocks++;
    }

    if (blockIndex < 0 || visitedBlocks > blockCapacity)
    {
        return NULL;
    }

    block = &blocks[blockIndex];
    originalVertexCount = block->vertexCount;
    block->modelFile = modelFile;
    block->destroyedLevel = destroyedLevel;
    block->refCount++;
    splitBlock = FALSE;

    if (originalVertexCount != vertexCount)
    {
        for (unusedIndex = 0; unusedIndex < blockCapacity; unusedIndex++)
        {
            if (blocks[unusedIndex].refCount == VTXSTORE_BLOCK_UNUSED)
            {
                block->vertexCount = vertexCount;
                blocks[unusedIndex].vertices = block->vertices + vertexCount;
                blocks[unusedIndex].vertexCount = originalVertexCount - vertexCount;
                blocks[unusedIndex].refCount = VTXSTORE_BLOCK_FREE;
                blocks[unusedIndex].previousIndex = blockIndex;
                blocks[unusedIndex].nextIndex = block->nextIndex;

                if (block->nextIndex >= 0)
                {
                    blocks[block->nextIndex].previousIndex = unusedIndex;
                }

                block->nextIndex = unusedIndex;
                splitBlock = TRUE;
                break;
            }
        }
    }

    if (splitBlock)
    {
        *freeVertexCount -= vertexCount;
    }
    else
    {
        *freeVertexCount -= originalVertexCount;
    }

    return block->vertices;
}


/**
 * Release a vertex-store allocation and coalesce adjacent free blocks.
 */
void vtxstoreFree(Vertex *vertices)
{
    VtxStoreBlock *blocks;
    VtxStoreBlock *block;
    s16 *freeVertexCount;
    s16 blockIndex;
    s16 adjacentIndex;

    if (vertices >= g_ChrVtxStoreVertices
        && vertices < g_ChrVtxStoreVertices + g_ChrVtxStoreVertexCapacity)
    {
        blocks = g_ChrVtxStoreBlocks;
        freeVertexCount = &g_ChrVtxStoreFreeVertices;
    }
    else if (vertices >= g_ObjVtxStoreVertices
        && vertices < g_ObjVtxStoreVertices + g_ObjVtxStoreVertexCapacity)
    {
        blocks = g_ObjVtxStoreBlocks;
        freeVertexCount = &g_ObjVtxStoreFreeVertices;
    }
    else
    {
        return;
    }

    blockIndex = 0;

    while (blockIndex >= 0)
    {
        block = &blocks[blockIndex];

        if (block->vertices == vertices)
        {
            block->refCount--;

            if (block->refCount == VTXSTORE_BLOCK_FREE)
            {
                *freeVertexCount += block->vertexCount;

                adjacentIndex = block->nextIndex;

                if (adjacentIndex >= 0 && blocks[adjacentIndex].refCount == VTXSTORE_BLOCK_FREE)
                {
                    block->vertexCount += blocks[adjacentIndex].vertexCount;
                    block->nextIndex = blocks[adjacentIndex].nextIndex;
                    blocks[adjacentIndex].refCount = VTXSTORE_BLOCK_UNUSED;

                    if (block->nextIndex >= 0)
                    {
                        blocks[block->nextIndex].previousIndex = blockIndex;
                    }
                }

                adjacentIndex = block->previousIndex;

                if (adjacentIndex >= 0 && blocks[adjacentIndex].refCount == VTXSTORE_BLOCK_FREE)
                {
                    blocks[adjacentIndex].vertexCount += block->vertexCount;
                    blocks[adjacentIndex].nextIndex = block->nextIndex;
                    block->refCount = VTXSTORE_BLOCK_UNUSED;
                    blockIndex = adjacentIndex;
                    block = &blocks[blockIndex];

                    if (block->nextIndex >= 0)
                    {
                        blocks[block->nextIndex].previousIndex = blockIndex;
                    }
                }
            }

            return;
        }

        blockIndex = block->nextIndex;
    }
}
