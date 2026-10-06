#include <ultra64.h>
#include <gbi_extension.h>
#include "chrobjdata.h"
#include "image.h"
#include "lightfixture.h"
#include "gmath.h"
#include "model.h"
#include "ob.h"
#include "objecthandler.h"
#include "quaternion.h"
#include "tex.h"

/* Preflight a private cutscene buffer before the unbounded legacy loader runs.
 * Only the caller's scratch and texture pool are changed. In particular, do
 * not cache a raw size in ob.c: a later stage load still needs expansion space.
 * Models using the water/light extensions retain the existing stage path. */
bool modelGetBufferRequirements(ModelFileHeader *definition, u8 *name,
        u8 *scratch, s32 capacity, struct texpool *pool, ModelBufferRequirements *result)
{
    ModelFileHeader header = *definition;
    ModelNode *node = NULL;
    Gfx *gdl = NULL;
    Gfx *command;
    s32 size = fileReadRawToBuffer(name, scratch, capacity);
    s32 first;
    s32 extra = 0;
    s32 i;
    s32 texture;

    if (size <= 0 || header.numSwitches < 0 || header.numtextures < 0) return FALSE;
    first = header.numSwitches * 4 + header.numtextures * sizeof(ModelFileTextures);
    if (first < 0 || first > size - sizeof(ModelNode)) return FALSE;
    header.Switches = (ModelNode **)scratch;
    header.Textures = (ModelFileTextures *)(scratch + header.numSwitches * 4);
    header.RootNode = (ModelNode *)(scratch + first);

    for (i = 0; i < header.numtextures; i++)
    {
        texture = (u32)header.Textures[i].TextureID;
        if ((u32)texture < g_TextureRomConfig.textureCount)
        {
            texLoadFromTextureNum(texture, pool);
            if (!texFindInPool(texture, pool)) return FALSE;
        }
    }

    sub_GAME_7F075A90(&header, 0x5000000, scratch);
    modelIterateDisplayLists(&header, &node, &gdl);
    if (!gdl) return FALSE;
    first = (u32)gdl & 0xffffff;
    if (first < 0 || first > size || (first & 7)) return FALSE;

    /* texLoadFromGdl copies non-markers one for one. Ordinary texture markers
     * emit at most 32 commands (7 mip levels, palette, tile/size, sync and
     * texture state); detail markers get 64 for their second upload. Count
     * the original marker too for a deliberately conservative upper bound. */
    for (i = first; i <= size - sizeof(Gfx); i += sizeof(Gfx))
    {
        command = (Gfx *)(scratch + i);
        if ((command->words.w0 >> 24) == (u8)G_NOOP)
        {
            s32 type = command->words.w0 & 7;
            if (type > 4) return FALSE;
            texture = command->words.w1 & 0xfff;
            if (texture == 1508 || texture == 1511 || check_if_imageID_is_light(texture)) return FALSE;
            texLoadFromTextureNum(texture, pool);
            if (!texFindInPool(texture, pool)) return FALSE;
            if (type == TEXTURETYPE_DETAIL)
            {
                texture = (command->words.w1 >> 12) & 0xfff;
                texLoadFromTextureNum(texture, pool);
                if (!texFindInPool(texture, pool)) return FALSE;
            }
            extra += (type == TEXTURETYPE_DETAIL ? 64 : 32) * sizeof(Gfx);
            if (extra > capacity - size) return FALSE;
        }
    }

    result->bytes = (size + extra + 15) & ~15;
    result->workspace = result->bytes + size - first;
    modelCalculateRwDataLen(&header);
    result->rwWords = header.numRecords;
    return result->workspace <= capacity;
}


void sub_GAME_7F0762E0(ModelFileHeader *objheader, u8 *name, u8 *dst, struct texpool *buffer)
{
    ModelNode *node;
    s32 romremaining;
    Gfx *gdl;
    s32 pcremaining;
    u32 replacementgdl;
    ModelNode *curnode;
    Gfx *curgdl;
    s32 delta;
    s32 filedata;
    s32 filenum;

    filedata = (s32) objheader->Switches;
    filenum = fileGetIndex((char *) name);

    romremaining = get_rom_remaining_buffer_for_index(filenum);
    pcremaining = get_pc_remaining_buffer_for_index(filenum);
    node = 0;
    modelIterateDisplayLists(objheader, &node, &gdl);

    if (gdl != 0)
    {
        name = (u8 *) ((pcremaining - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)))) + ((s32) filedata));
        
        /* The signed lvalue cast is required for the compiler to choose the target registers. */
        replacementgdl = (u32)*(s32 *)&gdl;
        
        delta = ((s32) ((romremaining + filedata) - (s32) name)) - ((s32) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)));
        
        texCopyGdls((Gfx *) (((u8 *) objheader->Switches) + (((u32) gdl) & 0x00ffffff)), (Gfx *) ((romremaining + filedata) - (s32) name), (s32) name);

        texLoadFromModelFileHeader(objheader, buffer);

        if (node != 0)
        {
            do
            {
                curnode = node;
                curgdl = gdl;
                modelIterateDisplayLists(objheader, &node, &gdl);
                
                if (gdl != 0)
                {
                    name = (u8 *) (((s32) gdl) - ((s32) curgdl));
                }
                else
                {
                    name = (u8 *) ((((s32) (filedata + pcremaining)) - ((s32) objheader->Switches)) - (((u32) curgdl) & 0x00ffffff));
                }
                
                modelNodeReplaceGdl((u32) objheader, curnode, curgdl, (Gfx *) replacementgdl);
                
                replacementgdl += texLoadFromGdl( (Gfx *) ((((u8 *) objheader->Switches) + (((u32) curgdl) & 0x00ffffff)) + delta), (s32) name, (Gfx *) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff)), buffer);
            } 
            while (node != 0);
        }

        name = (u8 *) (((s32) (((u8 *) objheader->Switches) + (replacementgdl & 0x00ffffff))) - filedata);

        fileSetSize(filenum, (u8 *) filedata, (((s32) name + 0xf) & (~0xf)), dst == 0);
    }
}


/***
 * NTSC addres 0x7F0764A4.
*/
void load_object_fill_header(struct ModelFileHeader *objheader, u8 *name, u8* dst, s32 size, struct texpool * buffer)
{
    void *filedata;

    if (dst != 0)
    {
        filedata = _fileNameLoadToAddr(name, 0, dst, size);
    }
    else
    {
        filedata = _fileNameLoadToBank(name, 0, 0x100, 4);
    }
    
    objheader->Switches = (struct ModelNode **)filedata;
    
    // hmmmmmmmmmmmm
    objheader->Textures = (struct ModelFileTextures *)&((s32*)filedata)[objheader->numSwitches];
    
    objheader->RootNode = (struct ModelNode *)&objheader->Textures[objheader->numtextures];
    
    sub_GAME_7F075A90(objheader, 0x5000000, filedata);
    sub_GAME_7F0762E0(objheader, name, dst, buffer);
}


void fileLoad(struct ModelFileHeader *header,char *name)
{
   load_object_fill_header(header,name,0,0,0);
   return;
}
