/* Rebuild render streams while retaining the original native model tree and
 * vertex/joint associations. Source identities survive Blender reindexing.
 * This first import path accepts face deletion and existing texture assignment;
 * topology, positions, colors, and UV edits are rejected explicitly. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "texload.h"

#define MODEL_LIMIT 0x00fffff0u
#define MODEL_DELETED 0xffffu

typedef struct ModelOutput { unsigned char *data; DWORD size, capacity; BOOL failed; } ModelOutput;
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
DWORD ModelDataHash(const unsigned char *data, DWORD size)
{
    DWORD hash=2166136261u, i;
    for (i=0; i<size; i++) { hash=(hash^data[i])*16777619u; }
    return hash;
}
static void Append(ModelOutput *out, const void *data, DWORD size)
{
    DWORD next;
    unsigned char *grown;
    if (out->failed || size>MODEL_LIMIT-out->size) { out->failed=TRUE; return; }
    if (out->size+size>out->capacity)
    {
        next=out->capacity ? out->capacity : 4096;
        while (next<out->size+size) { next=next>MODEL_LIMIT/2 ? MODEL_LIMIT : next*2; }
        grown=realloc(out->data,next);
        if (!grown) { out->failed=TRUE; return; }
        out->data=grown; out->capacity=next;
    }
    if (size) { memcpy(out->data+out->size,data,size); }
    out->size+=size;
}
static void Command(ModelOutput *out, DWORD w0, DWORD w1)
{
    unsigned char bytes[8]; Write32(bytes,w0); Write32(bytes+4,w1); Append(out,bytes,8);
}
static void Indices(const unsigned char *command, int slot, unsigned char indices[3])
{
    if (command[0]==0xbf) { indices[0]=command[5]/10; indices[1]=command[6]/10; indices[2]=command[7]/10; }
    else
    {
        indices[0]=command[7-slot]&15; indices[1]=command[7-slot]>>4;
        indices[2]=(command[3-slot/2]>>(slot%2*4))&15;
    }
}
static void Triangles(ModelOutput *out, unsigned char indices[4][3], int count)
{
    unsigned char cmd[8]={0};
    int i;
    if (count==0) { return; }
    if (count==1)
    {
        cmd[0]=0xbf;
        for (i=0;i<3;i++) { cmd[5+i]=indices[0][i]*10; }
    }
    else
    {
        cmd[0]=0xb1;
        for (i=0;i<count;i++)
        {
            cmd[7-i]=indices[i][0]|(indices[i][1]<<4);
            cmd[3-i/2]|=indices[i][2]<<(i%2*4);
        }
    }
    Append(out,cmd,8);
}
static void Material(ModelOutput *out, BgMaterial *current, const BgMaterial *desired)
{
    BgMaterial untextured;
    BOOL textured=BgMaterialTextureId(desired)!=BG_TEX_NONE;
    if(!textured)
    {
        untextured=*desired;untextured.textureword0=untextured.textureword1=0;desired=&untextured;
    }
    if (BgMaterialEqual(current,desired)) { return; }
    Command(out,0xe7000000u,0);
    if (current->modeword0!=desired->modeword0 || current->modeword1!=desired->modeword1)
    { Command(out,desired->modeword0,desired->modeword1); }
    if (textured && (current->textureword0!=desired->textureword0 || current->textureword1!=desired->textureword1))
    { Command(out,desired->textureword0,desired->textureword1); }
    if (current->combineword0!=desired->combineword0 || current->combineword1!=desired->combineword1)
    { Command(out,desired->combineword0,desired->combineword1); }
    *current=*desired;
}

BOOL ModelCompileImport(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir,
    unsigned char **result, DWORD *resultsize, const char **reasonout)
{
    unsigned short *choices=NULL;
    ModelOutput output={0};
    DWORD i, list, facecursor=0;
    BOOL changed=imported->count!=source->count, ok=FALSE;
    *result=NULL; *resultsize=0;
    *reasonout="The model could not be rebuilt.";
    if (!source->listcount || size>MODEL_LIMIT || imported->count>source->count) { goto done; }
    choices=malloc((source->count ? source->count : 1)*sizeof(*choices));
    if (!choices) { goto done; }
    for(i=0;i<source->count;i++) { choices[i]=MODEL_DELETED; }
    for(i=0;i<imported->count;i++)
    {
        DWORD id=imported->sourcevertices[i*3], face=id/3;
        int corner, width=1,height=1, newwidth=1,newheight=1;
        unsigned short texture=BG_TEX_ID(imported->tags[i]);
        if(face>=source->count || choices[face]!=MODEL_DELETED) { *reasonout="Faces were duplicated or added. This import supports deleting faces and changing texture assignments."; goto done; }
        if (BG_TEX_ID(source->tags[face])!=BG_TEX_NONE
            && !TexGetProjectImageSize(projectdir,BG_TEX_ID(source->tags[face]),&width,&height))
        { *reasonout="An original model texture is missing from the project."; goto done; }
        if (texture!=BG_TEX_NONE)
        {
            if (!TexGetProjectImageSize(projectdir,texture,&newwidth,&newheight))
            { *reasonout="An assigned texture is not available in the project."; goto done; }
            if (newwidth!=width || newheight!=height)
            { *reasonout="Reassigned textures must have the same dimensions in this version of model import."; goto done; }
        }
        for(corner=0;corner<3;corner++)
        {
            DWORD vertexid=imported->sourcevertices[i*3+corner];
            const BgVertex *a=&imported->vertices[i*3+corner], *b;
            if(vertexid/3!=face || vertexid%3!=(id%3+corner)%3)
            { *reasonout="A face was reshaped or its winding changed. Preserve the exported triangles and their GUD attributes."; goto done; }
            b=&source->vertices[vertexid];
            if (fabsf(a->x-b->x)>0.003f || fabsf(a->y-b->y)>0.003f || fabsf(a->z-b->z)>0.003f
                || abs((int)a->r-b->r)>1 || abs((int)a->g-b->g)>1 || abs((int)a->b-b->b)>1 || abs((int)a->a-b->a)>1
                || (texture!=BG_TEX_NONE && (fabsf(a->s-b->s/width)>0.0001f || fabsf(a->t-b->t/height)>0.0001f)))
            { *reasonout="Positions, UVs, or vertex colors changed. This first import supports face deletion and texture assignment; keep vertex colors enabled in Blender's exporter."; goto done; }
        }
        choices[face]=texture;
        if(texture!=BG_TEX_ID(source->tags[face])) { changed=TRUE; }
    }
    if(!changed)
    {
        Append(&output,data,size); goto finish;
    }
    Append(&output,data,source->lists[0].offset);
    for(list=0;list<source->listcount;list++)
    {
        const ModelSourceList *part=&source->lists[list];
        if(part->preserve)
        {
            if(output.failed || part->pointer+4>output.size) { goto done; }
            Write32(output.data+part->pointer,0x05000000u|output.size);
            Append(&output,data+part->offset,part->end-part->offset);
            continue;
        }
        DWORD pc, count=(part->end-part->offset)/8, firstface=facecursor, scan;
        int owners[16], slot, pending=0;
        unsigned char triangles[4][3];
        unsigned short *needed=calloc(count*2,sizeof(*needed));
        unsigned short *textured=needed ? needed+count : NULL;
        BgMaterial current=part->initial, original=part->initial;
        if(!needed) { goto done; }
        for(slot=0;slot<16;slot++) { owners[slot]=-1; }
        /* Mark cache slots actually read by surviving faces. Each load retains
           its original matrix and render state; unused loads disappear. */
        scan=firstface;
        for(pc=part->offset;pc<part->end;pc+=8)
        {
            const unsigned char *cmd=data+pc;
            if(cmd[0]==4)
            {
                int begin=cmd[1]&15, n=(cmd[1]>>4)+1;
                for(slot=begin;slot<begin+n && slot<16;slot++) { owners[slot]=(pc-part->offset)/8; }
            }
            while(scan<source->count && source->faces[scan].list==list && source->faces[scan].command==pc)
            {
                if(choices[scan]!=MODEL_DELETED)
                {
                    unsigned char indices[3]; int k;
                    Indices(cmd,source->faces[scan].slot,indices);
                    for(k=0;k<3;k++)
                    {
                        if(indices[k]>=16 || owners[indices[k]]<0) { free(needed); goto done; }
                        needed[owners[indices[k]]]|=1u<<indices[k];
                        if(choices[scan]!=BG_TEX_NONE) { textured[owners[indices[k]]]|=1u<<indices[k]; }
                    }
                }
                scan++;
            }
        }
        if(output.failed || part->pointer+4>output.size) { free(needed); goto done; }
        Write32(output.data+part->pointer,0x05000000u|output.size);
        /* The first surviving use establishes its own material. Keep the
           original texture scale at vertex loads, where Fast3D applies it. */
        current.modeword0=current.modeword1=0;
        current.combineword0=current.combineword1=0;
        current.textureword0=current.textureword1=0;
        for(pc=part->offset;pc<part->end;pc+=8)
        {
            const unsigned char *cmd=data+pc;
            if(cmd[0]==0xb1 || cmd[0]==0xbf)
            {
                while(facecursor<source->count && source->faces[facecursor].list==list && source->faces[facecursor].command==pc)
                {
                    DWORD face=facecursor++;
                    BgMaterial desired=source->faces[face].material;
                    if(choices[face]==MODEL_DELETED) { continue; }
                    if(choices[face]!=BG_TEX_ID(source->tags[face])) { BgMaterialSetTexture(&desired,choices[face]); }
                    if(BgMaterialTextureId(&desired)==BG_TEX_NONE) { desired.textureword0=desired.textureword1=0; }
                    if(!BgMaterialEqual(&current,&desired))
                    {
                        Triangles(&output,triangles,pending); pending=0;
                        Material(&output,&current,&desired);
                    }
                    Indices(cmd,source->faces[face].slot,triangles[pending++]);
                    if(pending==4) { Triangles(&output,triangles,pending); pending=0; }
                }
                continue;
            }
            if(BgMaterialReadCommand(&original,Read32(cmd),Read32(cmd+4))) { continue; }
            Triangles(&output,triangles,pending); pending=0;
            if(cmd[0]==4)
            {
                unsigned short mask=needed[(pc-part->offset)/8];
                int first=cmd[1]&15, end=first+(cmd[1]>>4)+1;
                DWORD mode=original.modeword0;
                if(!textured[(pc-part->offset)/8]) { mode &= ~0xffu; }
                if(mask && (current.modeword0!=mode || current.modeword1!=original.modeword1))
                {
                    Command(&output,mode,original.modeword1);
                    current.modeword0=mode;current.modeword1=original.modeword1;
                }
                for(slot=first;slot<end;)
                {
                    int begin,n;
                    if(!(mask&(1u<<slot))) { slot++; continue; }
                    begin=slot++;
                    while(slot<end && (mask&(1u<<slot))) { slot++; }
                    n=slot-begin;
                    Command(&output,0x04000000u|((n-1)<<20)|(begin<<16)|(n*16),Read32(cmd+4)+(begin-first)*16);
                }
            }
            else
            {
                Append(&output,cmd,8);
            }
        }
        free(needed);
    }
finish:
    if(output.failed) { *reasonout="The rebuilt model exceeded the supported size or available memory."; goto done; }
    *result=output.data; *resultsize=output.size; output.data=NULL; ok=TRUE; *reasonout="";
done:
    free(choices); free(output.data); return ok;
}
