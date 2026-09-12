/* Rebuild render streams while retaining the original native model tree and
 * vertex/joint associations. Source identities survive Blender reindexing.
 * Position/UV/color edits update native Vtx records in place. Face deletion and
 * texture assignment rebuild commands without changing vertex/joint bindings.
 * Topology and lighting normals remain authored. Conflicting shared-vertex
 * edits are rejected instead of changing another face or splitting a binding. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "texload.h"

#define MODEL_LIMIT 0x00fffff0u
#define MODEL_DELETED 0xffffu

typedef struct ModelOutput { unsigned char *data; DWORD size, capacity; BOOL failed; } ModelOutput;
typedef struct ModelVertexEdit {
    DWORD offset;
    unsigned char bytes[16];
    BOOL textured, alphaeditable;
} ModelVertexEdit;
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static int Read16(const unsigned char *p)
{
    int value = (int)p[0] * 256 + p[1];
    return value < 32768 ? value : value - 65536;
}
static BOOL WriteRounded16(unsigned char *p, double value)
{
    int rounded;
    value = round(value);
    if (!isfinite(value) || value < -32768 || value > 32767) { return FALSE; }
    rounded = (int)value;
    p[0] = (unsigned int)rounded >> 8; p[1] = (unsigned int)rounded;
    return TRUE;
}
static int VertexEditCompare(const void *left, const void *right)
{
    DWORD a = ((const ModelVertexEdit *)left)->offset;
    DWORD b = ((const ModelVertexEdit *)right)->offset;
    return a < b ? -1 : a > b;
}

static BOOL PrepareVertexColor(ModelVertexEdit *edit, const ModelSource *source,
    DWORD id, const BgVertex *vertex, BgRenderAlpha alpha, const char **reasonout)
{
    const BgVertex *original = &source->vertices[id];
    if (source->faces[id / 3].normalmask & (1u << (id % 3)))
    {
        /* G_LIGHTING is applied at G_VTX time. Even a later geometry-mode
         * change cannot make these cached normals safe to overwrite as RGB.
         * Keep the old one-byte round-trip tolerance for the neutral preview. */
        if (abs((int)vertex->r - original->r) > 1
            || abs((int)vertex->g - original->g) > 1
            || abs((int)vertex->b - original->b) > 1)
        { *reasonout = "This part stores lighting normals in its RGB bytes. Keep its RGB colors unchanged; color-based parts can be painted."; return FALSE; }
    }
    else
    {
        edit->bytes[12] = vertex->r; edit->bytes[13] = vertex->g; edit->bytes[14] = vertex->b;
    }

    /* Editable exports contain effective alpha, which can be supplied by a
     * material constant instead of the native Vtx. Preserve hidden alpha on
     * a no-op; never bake that constant back into the vertex by accident. */
    edit->alphaeditable = alpha.shade && alpha.constant != 0;
    if (!edit->alphaeditable)
    {
        if (vertex->a != original->a)
        { *reasonout = "This part's material controls its opacity independently of vertex alpha. Keep its exported alpha unchanged."; return FALSE; }
    }
    else if (BgRenderVertexAlpha(alpha, edit->bytes[15]) != vertex->a)
    {
        unsigned int value = ((unsigned int)vertex->a * 255u + alpha.constant / 2u) / alpha.constant;
        if (value > 255u || BgRenderVertexAlpha(alpha, (unsigned char)value) != vertex->a)
        { *reasonout = "The requested alpha cannot be represented by this part's native vertex alpha and material."; return FALSE; }
        edit->bytes[15] = (unsigned char)value;
    }
    return TRUE;
}

/* Export uses a translated rest pose, while N64 Vtx positions remain local
 * to the matrix selected at their original load. Applying the exported delta
 * to the original integer position reverses that translation, including when
 * a triangle's cached vertices belong to different joints. */
static BOOL PrepareVertexEdit(ModelVertexEdit *edit, const unsigned char *data,
    const ModelSource *source, DWORD id, const BgVertex *vertex, int width,
    int height, BOOL textured, BgRenderAlpha alpha, const char **reasonout)
{
    const BgVertex *original = &source->vertices[id];
    const unsigned char *native;
    double delta[3] = {(double)vertex->x - original->x,
        (double)vertex->y - original->y, (double)vertex->z - original->z};
    int axis;
    edit->offset = source->vertexoffsets[id];
    edit->textured = textured;
    if (edit->offset > source->lists[0].offset
        || source->lists[0].offset - edit->offset < 16)
    { *reasonout = "A model vertex is outside its native vertex data."; return FALSE; }
    native = data + edit->offset;
    memcpy(edit->bytes, native, 16);
    for (axis = 0; axis < 3; axis++)
    {
        if (!WriteRounded16(edit->bytes + axis * 2, Read16(native + axis * 2) + delta[axis]))
        { *reasonout = "An edited position exceeds the N64 signed 16-bit joint-local range (-32768 to 32767)."; return FALSE; }
    }
    /* Retain unused UVs when a material is removed. Other textured faces can
     * still share this Vtx and establish its UVs during the merge below. */
    if (textured)
    {
        if (!WriteRounded16(edit->bytes + 8, (double)vertex->s * width * 32.0)
            || !WriteRounded16(edit->bytes + 10, (double)vertex->t * height * 32.0))
        { *reasonout = "An edited UV exceeds the N64 signed 16-bit texture-coordinate range (1/32 texel units)."; return FALSE; }
        if ((source->flags[id / 3] & BG_RENDER_ENVIRONMENT)
            && memcmp(edit->bytes + 8, native + 8, 4))
        { *reasonout = "This part uses generated reflection UVs. Keep its UVs unchanged; ordinary texture UVs can be edited."; return FALSE; }
    }
    return PrepareVertexColor(edit, source, id, vertex, alpha, reasonout);
}

static BOOL MergeVertexEdits(ModelVertexEdit *edits, DWORD *count, const char **reasonout)
{
    DWORD i, used = 0;
    if (*count) { qsort(edits, *count, sizeof(*edits), VertexEditCompare); }
    for (i = 0; i < *count; i++)
    {
        ModelVertexEdit *previous = used ? &edits[used - 1] : NULL;
        if (previous && previous->offset == edits[i].offset)
        {
            if (memcmp(previous->bytes, edits[i].bytes, 6))
            { *reasonout = "Faces sharing a native vertex have different positions. Move all exported copies of that vertex together, including copies in other parts or LODs."; return FALSE; }
            if (previous->textured && edits[i].textured
                && memcmp(previous->bytes + 8, edits[i].bytes + 8, 4))
            { *reasonout = "Faces sharing a native vertex have different UVs. Move their UV corners together; creating a new UV seam requires splitting native vertices and is not supported."; return FALSE; }
            if (memcmp(previous->bytes + 12, edits[i].bytes + 12, 3))
            { *reasonout = "Faces sharing a native vertex have different RGB colors or share its lighting normals. Paint all color copies consistently and keep any shared normals unchanged."; return FALSE; }
            if (previous->alphaeditable && edits[i].alphaeditable
                && previous->bytes[15] != edits[i].bytes[15])
            { *reasonout = "Faces sharing a native vertex have different alpha values. Paint all copies that use vertex alpha consistently."; return FALSE; }
            if (edits[i].textured)
            {
                memcpy(previous->bytes + 8, edits[i].bytes + 8, 4);
                previous->textured = TRUE;
            }
            if (edits[i].alphaeditable)
            {
                previous->bytes[15] = edits[i].bytes[15];
                previous->alphaeditable = TRUE;
            }
        }
        else
        {
            if (previous && edits[i].offset - previous->offset < 16)
            { *reasonout = "This model has overlapping native vertex records that cannot be safely edited."; return FALSE; }
            edits[used++] = edits[i];
        }
    }
    *count = used;
    return TRUE;
}

/* Dynamic lists are deliberately outside the editable export. If they share
 * storage, do not let an ordinary face edit modify their hidden vertices. */
static BOOL CheckDynamicVertices(const unsigned char *data, const ModelSource *source,
    const ModelVertexEdit *edits, DWORD count, const char **reasonout)
{
    DWORD list, pc, i;
    for (list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        if (!part->preserve) { continue; }
        for (pc = part->offset; pc < part->end; pc += 8)
        {
            const unsigned char *cmd = data + pc;
            DWORD raw, start, end;
            if (cmd[0] != 4) { continue; }
            raw = Read32(cmd + 4);
            start = (raw & 0xffffffu) + ((raw >> 24) == 5 ? 0 : part->vertexbase);
            end = start + ((cmd[1] >> 4) + 1) * 16;
            for (i = 0; i < count; i++)
            {
                if (edits[i].offset < end && edits[i].offset + 16 > start
                    && memcmp(edits[i].bytes, data + edits[i].offset, 16))
                { *reasonout = "An edited vertex is shared with a preserved dynamic model effect. Keep that vertex unchanged."; return FALSE; }
            }
        }
    }
    return TRUE;
}
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
    ModelVertexEdit *edits=NULL;
    ModelOutput output={0};
    DWORD i, list, facecursor=0, editcount=0;
    BOOL changed=imported->count!=source->count, ok=FALSE;
    *result=NULL; *resultsize=0;
    *reasonout="The model could not be rebuilt.";
    if (!source->listcount || source->lists[0].offset>size || size>MODEL_LIMIT
        || imported->count>source->count || (source->count && !source->vertexoffsets)) { goto done; }
    choices=malloc((source->count ? source->count : 1)*sizeof(*choices));
    edits=malloc((size_t)(imported->count ? imported->count : 1)*3*sizeof(*edits));
    if (!choices || !edits) { goto done; }
    for(i=0;i<source->count;i++) { choices[i]=MODEL_DELETED; }
    for(i=0;i<imported->count;i++)
    {
        DWORD id=imported->sourcevertices[i*3], face=id/3;
        int corner, width=1,height=1, newwidth=1,newheight=1;
        unsigned short texture=BG_TEX_ID(imported->tags[i]);
        BgMaterial material;
        BgRenderAlpha alpha;
        if(face>=source->count || choices[face]!=MODEL_DELETED) { *reasonout="Faces were duplicated or added. Preserve the exported triangles and their GUD attributes; existing faces may be moved or deleted."; goto done; }
        material = source->faces[face].material;
        if (texture != BG_TEX_ID(source->tags[face])) { BgMaterialSetTexture(&material, texture); }
        alpha = BgRenderGetMaterialAlpha(&source->faces[face].state, &material);
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
            const BgVertex *a=&imported->vertices[i*3+corner];
            if(vertexid/3!=face || vertexid%3!=(id%3+corner)%3)
            { *reasonout="A face's vertex identities or winding changed. Preserve the exported triangles and their GUD attributes."; goto done; }
            if (!PrepareVertexEdit(&edits[editcount++], data, source, vertexid, a,
                                  width, height, texture!=BG_TEX_NONE, alpha, reasonout)) { goto done; }
        }
        choices[face]=texture;
        if(texture!=BG_TEX_ID(source->tags[face])) { changed=TRUE; }
    }
    if (!MergeVertexEdits(edits, &editcount, reasonout)
        || !CheckDynamicVertices(data, source, edits, editcount, reasonout)) { goto done; }
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
    for (i=0; i<editcount; i++) { memcpy(output.data+edits[i].offset, edits[i].bytes, 16); }
    *result=output.data; *resultsize=output.size; output.data=NULL; ok=TRUE; *reasonout="";
done:
    free(choices); free(edits); free(output.data); return ok;
}
