/* Rebuild render streams while retaining the original native model tree and
 * vertex/joint associations. Source identities survive Blender reindexing.
 * Compatible edits update Vtx records in place. UV/color seams allocate native
 * variants and rebuild their loads, preserving load-time matrix/render state.
 * Triangle topology, local positions, lighting normals and model nodes remain
 * authored; a seam never welds vertices or averages the imported attributes.
 * A separate retopology path rebuilds rigid parts using their material slots. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelcompile.h"
#include "texload.h"

#define MODEL_LIMIT 0x00fffff0u
#define MODEL_DELETED 0xffffu

typedef struct ModelOutput { unsigned char *data; DWORD size, capacity; BOOL failed; } ModelOutput;
typedef struct ModelVertexEdit {
    DWORD offset, id, target;
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
    if (a != b) { return a < b ? -1 : 1; }
    a = ((const ModelVertexEdit *)left)->id;
    b = ((const ModelVertexEdit *)right)->id;
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
    edit->id = id;
    edit->offset = edit->target = source->vertexoffsets[id];
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

/* Attributes unused by a face are wildcards, not reasons to split. Position
 * disagreements still indicate separate edits to copies of one authored point.
 * Sorting by source ID makes variant selection independent of glTF face order. */
static BOOL MergeVertexEdits(ModelVertexEdit *edits, DWORD *count,
    DWORD *corners, BOOL *split, const char **reasonout)
{
    DWORD i, used = 0, first = 0;
    if (*count) { qsort(edits, *count, sizeof(*edits), VertexEditCompare); }
    for (i = 0; i < *count; i++)
    {
        ModelVertexEdit edit = edits[i];
        DWORD match;
        if (used && edits[first].offset == edit.offset)
        {
            if (memcmp(edits[first].bytes, edit.bytes, 6))
            { *reasonout = "Faces sharing a native vertex have different positions. Move all exported copies of that vertex together, including copies in other parts or LODs."; return FALSE; }
        }
        else
        {
            if (used && edit.offset - edits[first].offset < 16)
            { *reasonout = "This model has overlapping native vertex records that cannot be safely edited."; return FALSE; }
            first = used;
        }
        for (match = first; match < used; match++)
        {
            ModelVertexEdit *previous = &edits[match];
            if (previous->textured && edit.textured && memcmp(previous->bytes + 8, edit.bytes + 8, 4)) { continue; }
            if (memcmp(previous->bytes + 12, edit.bytes + 12, 3)) { continue; }
            if (previous->alphaeditable && edit.alphaeditable && previous->bytes[15] != edit.bytes[15]) { continue; }
            if (edit.textured) { memcpy(previous->bytes + 8, edit.bytes + 8, 4); previous->textured = TRUE; }
            if (edit.alphaeditable) { previous->bytes[15] = edit.bytes[15]; previous->alphaeditable = TRUE; }
            break;
        }
        if (match == used)
        {
            if (used != first) { edit.target = 0; *split = TRUE; }
            edits[used++] = edit;
        }
        corners[edit.id] = match;
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

BOOL ModelCompileVertexColor(const unsigned char *data, DWORD size, const ModelSource *source,
    DWORD corner, const unsigned char rgba[4], unsigned char **result, const char **reasonout)
{
    ModelVertexEdit edit = {0};
    BgVertex vertex;
    BgRenderAlpha alpha;
    DWORD i;
    *result = NULL;
    if (!data || !source || !rgba || corner / 3 >= source->count
        || !source->vertexoffsets || !source->listcount)
    { *reasonout = "The painted model vertex is no longer available."; return FALSE; }
    edit.offset = source->vertexoffsets[corner];
    if (edit.offset > size || size - edit.offset < 16
        || edit.offset > source->lists[0].offset || source->lists[0].offset - edit.offset < 16)
    { *reasonout = "The painted vertex is outside the model's vertex data."; return FALSE; }
    memcpy(edit.bytes, data + edit.offset, 16);
    vertex = source->vertices[corner];
    vertex.r = rgba[0]; vertex.g = rgba[1]; vertex.b = rgba[2]; vertex.a = rgba[3];
    alpha = BgRenderGetMaterialAlpha(&source->faces[corner / 3].state,
                                     &source->faces[corner / 3].material);
    if (!PrepareVertexColor(&edit, source, corner, &vertex, alpha, reasonout)) { return FALSE; }
    /* A vertex can be loaded as RGB in one part and as normals in another.
       Shared storage must retain the lighting data in both uses. */
    for (i = 0; i < source->count * 3; i++)
    {
        if (source->vertexoffsets[i] == edit.offset
            && (source->faces[i / 3].normalmask & (1u << (i % 3)))
            && memcmp(edit.bytes + 12, data + edit.offset + 12, 3))
        { *reasonout = "This vertex is shared with a part that stores lighting normals. Its RGB cannot be painted."; return FALSE; }
    }
    if (!CheckDynamicVertices(data, source, &edit, 1, reasonout)) { return FALSE; }
    *result = malloc(size);
    if (!*result) { *reasonout = "Out of memory painting the model vertex."; return FALSE; }
    memcpy(*result, data, size);
    memcpy(*result + edit.offset + 12, edit.bytes + 12, 4);
    *reasonout = "";
    return TRUE;
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
    /* Rebinding materials revisits their original pipe sync. Reuse it rather
     * than adding another sync on every edit/save/reimport cycle. */
    if (w0==0xe7000000u && !w1 && out->size>=8
        && Read32(out->data+out->size-8)==w0 && !Read32(out->data+out->size-4)) return;
    unsigned char bytes[8]; Write32(bytes,w0); Write32(bytes+4,w1); Append(out,bytes,8);
}

BOOL ModelCompileDefaultCulling(const unsigned char *data,DWORD size,const ModelSource *source,
    unsigned char **result,DWORD *resultsize,const char **why)
{
    ModelOutput out={0};DWORD previous=0;BOOL ok=FALSE;
    *result=NULL;*resultsize=0;size=ModelMaterialsNativeSize(data,size);
    *why="Could not set the new character's backface culling.";
    for (DWORD l=0;l<source->listcount;l++) {
        const ModelSourceList *p=&source->lists[l];
        if (p->offset<previous || p->end<p->offset || p->end>size || p->pointer+4>p->offset) goto done;
        Append(&out,data+previous,p->offset-previous);
        if (out.failed) goto done;
        Write32(out.data+p->pointer,0x05000000u|out.size);
        Command(&out,0xb6000000u,0x3000);Command(&out,0xb7000000u,0x2000);
        for (DWORD pc=p->offset;pc<p->end;pc+=8) {
            DWORD a=Read32(data+pc),b=Read32(data+pc+4);
            if (data[pc]==0xb6 || data[pc]==0xb7) b&=~0x3000u;
            Command(&out,a,b);
        }
        previous=p->end;
    }
    Append(&out,data+previous,size-previous);
    if (out.failed) goto done;
    *result=out.data;*resultsize=out.size;out.data=NULL;ok=TRUE;*why="";
done:
    free(out.data);return ok;
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
BOOL ModelCompileDeleteFaces(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, unsigned char **result, DWORD *resultsize, const char **reasonout)
{
    ModelOutput out = {0};
    unsigned char *deleted = NULL;
    unsigned short *needed = NULL;
    DWORD cursor = 0, previous = 0;
    BOOL ok = FALSE;
    *result = NULL; *resultsize = 0;
    *reasonout = "Invalid model face selection.";
    if (!data || !source || !source->listcount || !source->count || !faces || !count) return FALSE;
    size = ModelMaterialsNativeSize(data, size);
    deleted = calloc(source->count, 1);
    if (!deleted) { *reasonout = "Out of memory deleting model faces."; goto done; }
    for (DWORD i = 0; i < count; i++)
    {
        if (faces[i] >= source->count) goto done;
        deleted[faces[i]] = 1;
    }
    *reasonout = "The model's display-list layout could not be rebuilt.";
    for (DWORD list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        DWORD endface = cursor;
        BOOL affected = FALSE;
        while (endface < source->count && source->faces[endface].list == list)
            affected |= deleted[endface++];
        if (part->offset < previous || part->end < part->offset || part->end > size
            || (part->end - part->offset) % 8) goto done;
        Append(&out, data + previous, part->offset - previous);
        if (out.failed || part->pointer > out.size || out.size - part->pointer < 4) goto done;
        Write32(out.data + part->pointer, 0x05000000u | out.size);
        previous = part->end;
        if (!affected)
        {
            /* Includes dynamic lists which have no editable source faces. */
            Append(&out, data + part->offset, part->end - part->offset);
            cursor = endface;
            continue;
        }
        DWORD commands = (part->end - part->offset) / 8, scan = cursor;
        int owners[16];
        needed = calloc(commands, sizeof(*needed));
        if (!needed) { *reasonout = "Out of memory deleting model faces."; goto done; }
        for (int slot = 0; slot < 16; slot++) owners[slot] = -1;
        /* Keep each surviving vertex load at its original matrix/render state.
         * Removing triangles alone would still transform their unused vertices. */
        for (DWORD pc = part->offset; pc < part->end; pc += 8)
        {
            const unsigned char *cmd = data + pc;
            if (cmd[0] == 4)
            {
                int begin = cmd[1] & 15, end = begin + (cmd[1] >> 4) + 1;
                if (end > 16) goto done;
                for (int slot = begin; slot < end; slot++) owners[slot] = (pc - part->offset) / 8;
            }
            while (scan < endface && source->faces[scan].command == pc)
            {
                if (!deleted[scan])
                {
                    unsigned char indices[3];
                    Indices(cmd, source->faces[scan].slot, indices);
                    for (int k = 0; k < 3; k++)
                    {
                        if (indices[k] >= 16 || owners[indices[k]] < 0) goto done;
                        needed[owners[indices[k]]] |= 1u << indices[k];
                    }
                }
                scan++;
            }
        }
        if (scan != endface) goto done;
        for (DWORD pc = part->offset; pc < part->end; pc += 8)
        {
            const unsigned char *cmd = data + pc;
            if (cmd[0] == 0xb1 || cmd[0] == 0xbf)
            {
                unsigned char triangles[4][3];
                int n = 0;
                while (cursor < endface && source->faces[cursor].command == pc)
                {
                    if (!deleted[cursor]) Indices(cmd, source->faces[cursor].slot, triangles[n++]);
                    cursor++;
                }
                Triangles(&out, triangles, n);
            }
            else if (cmd[0] == 4)
            {
                unsigned short mask = needed[(pc - part->offset) / 8];
                int first = cmd[1] & 15, end = first + (cmd[1] >> 4) + 1;
                for (int slot = first; slot < end;)
                {
                    if (!(mask & (1u << slot))) { slot++; continue; }
                    int begin = slot++;
                    while (slot < end && (mask & (1u << slot))) slot++;
                    int n = slot - begin;
                    Command(&out, 0x04000000u | ((n - 1) << 20) | (begin << 16) | (n * 16),
                        Read32(cmd + 4) + (begin - first) * 16);
                }
            }
            else Append(&out, cmd, 8);
        }
        free(needed); needed = NULL;
    }
    if (cursor != source->count) goto done;
    Append(&out, data + previous, size - previous);
    if (out.failed) { *reasonout = "The rebuilt model exceeded the supported size or available memory."; goto done; }
    *result = out.data; *resultsize = out.size; out.data = NULL;
    ok = TRUE; *reasonout = "";
done:
    free(needed); free(deleted); free(out.data); return ok;
}

static void MaterialWithSync(ModelOutput *out, BgMaterial *current, const BgMaterial *desired, BOOL sync)
{
    BgMaterial untextured;
    BOOL textured=BgMaterialTextureId(desired)!=BG_TEX_NONE;
    if(!textured)
    {
        untextured=*desired;untextured.textureword0=untextured.textureword1=0;desired=&untextured;
    }
    if (BgMaterialEqual(current,desired)) { return; }
    if (sync) { Command(out,0xe7000000u,0); }
    if (current->modeword0!=desired->modeword0 || current->modeword1!=desired->modeword1)
    { Command(out,desired->modeword0,desired->modeword1); }
    if (textured && (current->textureword0!=desired->textureword0 || current->textureword1!=desired->textureword1))
    { Command(out,desired->textureword0,desired->textureword1); }
    if (current->combineword0!=desired->combineword0 || current->combineword1!=desired->combineword1)
    { Command(out,desired->combineword0,desired->combineword1); }
    *current=*desired;
}
static void Material(ModelOutput *out, BgMaterial *current, const BgMaterial *desired)
{ MaterialWithSync(out, current, desired, TRUE); }

/* Collision display-list nodes can replace segment 4 with a deformed copy at
 * runtime. Grow their whole contiguous vertex array, retain the old indices,
 * and update numVertices. Appending file-absolute vertices alone would bypass
 * that runtime storage. Collision points/bounds stay authored: seams share XYZ. */
typedef struct ModelVertexBuffer {
    DWORD base, count, newbase, newcount;
    DWORD *targets;
    BOOL owns;
} ModelVertexBuffer;

static BOOL SplitBuffers(ModelOutput *out, const unsigned char *data,
    const ModelSource *source, ModelVertexEdit *edits, DWORD editcount,
    const DWORD *corners, const unsigned short *choices,
    ModelVertexBuffer *buffers, const char **reasonout)
{
    static const unsigned char zero[16] = {0};
    DWORD i, list;
    Append(out, zero, (16 - out->size % 16) % 16);
    for (i = 0; i < editcount; i++)
    {
        if (!edits[i].target) { edits[i].target = out->size; Append(out, edits[i].bytes, 16); }
    }
    for (list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        ModelVertexBuffer *buffer = &buffers[list];
        DWORD previous, face, additions = 0;
        if (part->preserve) { continue; }
        for (previous = 0; previous < list; previous++)
        { if (source->lists[previous].vertexpointer == part->vertexpointer) { break; } }
        if (previous < list) { *buffer = buffers[previous]; buffer->owns = FALSE; continue; }
        if (part->vertexpointer + 6 > source->lists[0].offset) { return FALSE; }
        buffer->base = buffer->newbase = part->vertexbase;
        buffer->count = buffer->newcount = (DWORD)data[part->vertexpointer + 4] * 256 + data[part->vertexpointer + 5];
        buffer->targets = calloc(editcount ? editcount : 1, sizeof(*buffer->targets));
        if (!buffer->targets) { return FALSE; }
        buffer->owns = TRUE;
        for (face = 0; face < source->count; face++)
        {
            int k;
            if (choices[face] == MODEL_DELETED || source->lists[source->faces[face].list].vertexpointer != part->vertexpointer) { continue; }
            for (k = 0; k < 3; k++)
            {
                DWORD variant = corners[face * 3 + k], offset = edits[variant].offset;
                if (edits[variant].target == offset || offset < buffer->base
                    || offset - buffer->base >= buffer->count * 16 || buffer->targets[variant]) { continue; }
                buffer->targets[variant] = 1; additions++;
            }
        }
        if (!additions)
        {
            free(buffer->targets); buffer->targets = NULL; buffer->owns = FALSE;
            continue;
        }
        if (buffer->base > source->lists[0].offset
            || buffer->count * 16 > source->lists[0].offset - buffer->base
            || buffer->count + additions > 32767)
        { *reasonout = "The split vertices exceed this part's native vertex-buffer range or count."; return FALSE; }
        buffer->newbase = out->size; buffer->newcount += additions;
        Append(out, data + buffer->base, buffer->count * 16);
        for (i = 0; i < editcount && !out->failed; i++)
        {
            DWORD offset = edits[i].offset;
            if (buffer->targets[i])
            { buffer->targets[i] = out->size; Append(out, edits[i].bytes, 16); }
            else if (edits[i].target == offset && offset >= buffer->base
                && offset - buffer->base <= (buffer->count - 1) * 16)
            {
                buffer->targets[i] = buffer->newbase + offset - buffer->base;
                memcpy(out->data + buffer->targets[i], edits[i].bytes, 16);
            }
        }
        if (out->failed) { return FALSE; }
        Write32(out->data + part->vertexpointer, 0x05000000u | buffer->newbase);
        out->data[part->vertexpointer + 4] = buffer->newcount >> 8;
        out->data[part->vertexpointer + 5] = buffer->newcount;
        if (part->pointusagepointer)
        {
            DWORD raw, oldlinks, newlinks;
            if (part->pointusagepointer + 4 > source->lists[0].offset) { return FALSE; }
            raw = Read32(data + part->pointusagepointer); oldlinks = raw & 0xffffffu;
            if (!raw)
            {
                if (Read16(data + part->vertexpointer + 6) != 0)
                { *reasonout = "This collision part has no vertex-usage table to extend."; return FALSE; }
                continue;
            }
            if ((raw >> 24) != 5 || oldlinks > source->lists[0].offset
                || buffer->count * 2 > source->lists[0].offset - oldlinks)
            { *reasonout = "This part's vertex-usage table is outside its native data."; return FALSE; }
            newlinks = out->size;
            Append(out, data + oldlinks, buffer->count * 2);
            for (i = buffer->count; i < buffer->newcount; i++) { Append(out, zero, 2); }
            if (out->failed) { return FALSE; }
            /* Preserve every existing chain, inserting variants after their
             * original index. Collision points and related-node links remain
             * unchanged; chrCreateBloodStain now reaches the extra copies. */
            for (i = 0; i < buffer->count; i++)
            {
                int next = Read16(data + oldlinks + i * 2);
                if (next >= (int)buffer->count)
                { *reasonout = "This part has an invalid vertex-usage index."; return FALSE; }
            }
            for (i = 0; i < editcount; i++)
            {
                DWORD originalindex, index, target = buffer->targets[i];
                unsigned char *links = out->data + newlinks;
                if (!target || target - buffer->newbase < buffer->count * 16) { continue; }
                if ((edits[i].offset - buffer->base) % 16)
                { *reasonout = "A split vertex is misaligned within its native vertex array."; return FALSE; }
                originalindex = (edits[i].offset - buffer->base) / 16;
                index = (target - buffer->newbase) / 16;
                memcpy(links + index * 2, links + originalindex * 2, 2);
                links[originalindex * 2] = index >> 8; links[originalindex * 2 + 1] = index;
            }
            Write32(out->data + part->pointusagepointer, 0x05000000u | newlinks);
            Append(out, zero, (16 - out->size % 16) % 16);
        }
    }
    return !out->failed;
}

static BOOL SplitVertexAddress(const ModelVertexBuffer *buffer,
    const ModelVertexEdit *edit, DWORD variant, DWORD segment, DWORD *address, const char **reasonout)
{
    DWORD target = buffer->targets && buffer->targets[variant]
        ? buffer->targets[variant] : edit->target;
    if (segment == 4)
    {
        if (target < buffer->newbase || target - buffer->newbase >= buffer->newcount * 16)
        { *reasonout = "A split vertex lies outside its part's runtime vertex buffer."; return FALSE; }
        target -= buffer->newbase;
    }
    else if (segment != 5)
    { *reasonout = "This model uses an unsupported vertex segment."; return FALSE; }
    *address = segment << 24 | target;
    return TRUE;
}

/* Recipes retain the state at the original G_VTX, not the state at G_TRI.
 * A physical cache slot may hold any recipe/variant pair. This permits more
 * than sixteen total variants: only a triangle's three vertices must coexist.
 * RSP state inherited from the caller is never guessed or replaced with a
 * preview default. Exotic state transitions we cannot restore are rejected. */
typedef struct ModelLoadState {
    DWORD matrix, mode0, mode1, geometry, known, epoch;
} ModelLoadState;
typedef struct ModelLoadRecipe {
    ModelLoadState state;
    DWORD key, segment;
} ModelLoadRecipe;
typedef struct ModelCacheEntry { DWORD key, variant; } ModelCacheEntry;
#define MODEL_NO_VERTEX 0xffffffffu

static BOOL LoadState(ModelOutput *out, ModelLoadState *current,
    const ModelLoadState *desired, BgMaterial *material, const char **reasonout)
{
    DWORD clear, set;
    if (current->epoch != desired->epoch || current->known != desired->known
        || (!desired->matrix && current->matrix != desired->matrix))
    { *reasonout = "This seam crosses inherited or unsupported RSP vertex state that cannot be safely restored."; return FALSE; }
    clear = current->geometry & ~desired->geometry & current->known;
    set = desired->geometry & ~current->geometry & current->known;
    if (clear) { Command(out, 0xb6000000u, clear); }
    if (set) { Command(out, 0xb7000000u, set); }
    if (current->matrix != desired->matrix) { Command(out, 0x01020040u, desired->matrix); }
    if (current->mode0 != desired->mode0 || current->mode1 != desired->mode1)
    { Command(out, desired->mode0, desired->mode1); }
    *current = *desired;
    material->modeword0 = current->mode0; material->modeword1 = current->mode1;
    return TRUE;
}

static BOOL SplitList(ModelOutput *out, const unsigned char *data,
    const ModelSource *source, DWORD list, DWORD *facecursor,
    const unsigned short *choices, const ModelVertexEdit *edits,
    const DWORD *corners, const ModelVertexBuffer *buffer, const unsigned char *rebind, const char **reasonout)
{
    const ModelSourceList *part = &source->lists[list];
    DWORD count = (part->end - part->offset) / 8, pc, scan = *facecursor;
    DWORD *firstuses = malloc((size_t)count * 16 * sizeof(*firstuses));
    int owners[16], slot, pending = 0;
    unsigned char triangles[4][3];
    ModelLoadRecipe recipes[16];
    ModelCacheEntry cache[16];
    BgMaterial original = part->initial, current = part->initial;
    ModelLoadState state = {0};
    BOOL ok = FALSE;
    if (!firstuses) { return FALSE; }
    memset(firstuses, 0xff, (size_t)count * 16 * sizeof(*firstuses));
    memset(cache, 0xff, sizeof(cache));
    memset(recipes, 0, sizeof(recipes));
    for (slot = 0; slot < 16; slot++) { owners[slot] = -1; }
    for (pc = part->offset; pc < part->end; pc += 8)
    {
        const unsigned char *cmd = data + pc;
        if (cmd[0] == 4)
        {
            int first = cmd[1] & 15, n = (cmd[1] >> 4) + 1;
            for (slot = first; slot < first + n && slot < 16; slot++) { owners[slot] = (pc - part->offset) / 8; }
        }
        /* These commands consume/modify cache indices or the matrix stack.
         * Keeping their bytes after remapping vertices would silently corrupt
         * geometry, so keep their existing unsupported status explicit. */
        if (cmd[0] == 0xbe || cmd[0] == 0xbd || cmd[0] == 0xb2
            || (cmd[0] == 0xbc && cmd[3] == 0x0c))
        { *reasonout = "Vertex splitting is not supported for this part's cache-modification or matrix-stack commands."; goto done; }
        while (scan < source->count && source->faces[scan].list == list && source->faces[scan].command == pc)
        {
            if (choices[scan] != MODEL_DELETED)
            {
                unsigned char idx[3]; int k;
                Indices(cmd, source->faces[scan].slot, idx);
                for (k = 0; k < 3; k++)
                {
                    DWORD *first;
                    if (idx[k] >= 16 || owners[idx[k]] < 0) { goto done; }
                    first = &firstuses[owners[idx[k]] * 16 + idx[k]];
                    if (*first == MODEL_NO_VERTEX) { *first = corners[scan * 3 + k]; }
                }
            }
            scan++;
        }
    }
    if (out->failed || part->pointer + 4 > out->size) { goto done; }
    Write32(out->data + part->pointer, 0x05000000u | out->size);
    /* The renderer may supply an inherited matrix; only an explicit G_MTX
     * gives us an address we can safely restore after a later matrix change. */
    state.mode0 = current.modeword0; state.mode1 = current.modeword1;
    current.textureword0 = current.textureword1 = 0;
    current.combineword0 = current.combineword1 = 0;
    for (pc = part->offset; pc < part->end; pc += 8)
    {
        const unsigned char *cmd = data + pc;
        if (cmd[0] == 0xbf || cmd[0] == 0xb1)
        {
            while (*facecursor < source->count && source->faces[*facecursor].list == list && source->faces[*facecursor].command == pc)
            {
                DWORD face = (*facecursor)++, variants[3];
                BgMaterial desired = source->faces[face].material;
                unsigned char idx[3], mapped[3];
                unsigned short locked = 0;
                int k;
                if (choices[face] == MODEL_DELETED) { continue; }
                Indices(cmd, source->faces[face].slot, idx);
                for (k = 0; k < 3; k++)
                {
                    variants[k] = corners[face * 3 + k]; mapped[k] = 16;
                    for (slot = 0; slot < 16; slot++)
                    {
                        if (cache[slot].key == recipes[idx[k]].key && cache[slot].variant == variants[k])
                        { mapped[k] = (unsigned char)slot; locked |= 1u << slot; break; }
                    }
                }
                for (k = 0; k < 3; k++)
                {
                    DWORD address;
                    ModelLoadState saved = state;
                    if (mapped[k] < 16) { continue; }
                    slot = idx[k];
                    if (locked & (1u << slot))
                    { for (slot = 0; slot < 16 && (locked & (1u << slot)); slot++) {} }
                    Triangles(out, triangles, pending); pending = 0;
                    if (!SplitVertexAddress(buffer, &edits[variants[k]], variants[k], recipes[idx[k]].segment, &address, reasonout)
                        || !LoadState(out, &state, &recipes[idx[k]].state, &current, reasonout)) { goto done; }
                    Command(out, 0x04000010u | ((DWORD)slot << 16), address);
                    if (!LoadState(out, &state, &saved, &current, reasonout)) { goto done; }
                    cache[slot].key = recipes[idx[k]].key; cache[slot].variant = variants[k];
                    mapped[k] = (unsigned char)slot; locked |= 1u << slot;
                }
                if (choices[face] != BG_TEX_ID(source->tags[face]) || (rebind && rebind[face])) { BgMaterialSetTexture(&desired, choices[face]); }
                if (!BgMaterialEqual(&current, &desired))
                { Triangles(out, triangles, pending); pending = 0; Material(out, &current, &desired); }
                state.mode0 = current.modeword0; state.mode1 = current.modeword1;
                memcpy(triangles[pending++], mapped, 3);
                if (pending == 4) { Triangles(out, triangles, pending); pending = 0; }
            }
            continue;
        }
        if (BgMaterialReadCommand(&original, Read32(cmd), Read32(cmd + 4))) { continue; }
        Triangles(out, triangles, pending); pending = 0;
        if (cmd[0] == 4)
        {
            int first = cmd[1] & 15, end = first + (cmd[1] >> 4) + 1;
            DWORD addresses[16], variants[16], serial = (pc - part->offset) / 8;
            if (end > 16) { goto done; }
            DWORD loadmode=original.modeword0;
            for (slot=first;slot<end;slot++)
            {
                DWORD variant=firstuses[serial*16+slot];
                if (variant!=MODEL_NO_VERTEX && edits[variant].textured) loadmode|=1u;
            }
            if (state.mode0 != loadmode || state.mode1 != original.modeword1)
            { Command(out, loadmode, original.modeword1); }
            state.mode0 = current.modeword0 = loadmode;
            state.mode1 = current.modeword1 = original.modeword1;
            for (slot = first; slot < end; slot++)
            {
                variants[slot] = firstuses[serial * 16 + slot];
                recipes[slot].key = pc * 16 + slot;
                recipes[slot].segment = cmd[4]; recipes[slot].state = state;
                if (variants[slot] == MODEL_NO_VERTEX) { continue; }
                if (!SplitVertexAddress(buffer, &edits[variants[slot]], variants[slot], cmd[4], &addresses[slot], reasonout)) { goto done; }
                cache[slot].key = recipes[slot].key; cache[slot].variant = variants[slot];
            }
            for (slot = first; slot < end;)
            {
                int begin, n;
                if (variants[slot] == MODEL_NO_VERTEX) { slot++; continue; }
                begin = slot++;
                while (slot < end && variants[slot] != MODEL_NO_VERTEX && addresses[slot] == addresses[slot - 1] + 16) { slot++; }
                n = slot - begin;
                Command(out, 0x04000000u | ((n - 1) << 20) | (begin << 16) | (n * 16), addresses[begin]);
            }
        }
        else
        {
            Append(out, cmd, 8);
            if (cmd[0] == 1) { state.matrix = Read32(cmd + 4); }
            else if (cmd[0] == 0xb6 || cmd[0] == 0xb7)
            {
                /* Culling and smooth-shading bits are draw-time state. */
                DWORD bits = Read32(cmd + 4) & 0x001f0004u; state.known |= bits;
                if (cmd[0] == 0xb6) { state.geometry &= ~bits; } else { state.geometry |= bits; }
            }
            else if (cmd[0] < 0xc0 && cmd[0] != 0xb8 && cmd[0] != 0xb9 && cmd[0] != 0xba && cmd[0] != 0)
            { state.epoch++; } /* Lights, fog, viewport, segments, etc. */
        }
    }
    ok = !out->failed;
done:
    free(firstuses); return ok;
}

/* Retopology uses material slots as native draw anchors. The original vertex
 * cache recipes still supply matrix and load-time state; each new
 * corner gets its own native attributes, independent of obsolete corner IDs. */
int ModelImportKeepsTopology(const ModelSource *source, const GltfModelImport *imported)
{
    unsigned char *seen;
    BOOL same = FALSE;
    if (!imported->count) { return TRUE; }
    if (!imported->sourcevertices || imported->count > source->count) { return FALSE; }
    seen = calloc(source->count ? source->count : 1, 1);
    if (!seen) { return -1; }
    for (DWORD i = 0; i < imported->count; i++)
    {
        DWORD id = imported->sourcevertices[i * 3], face = id / 3;
        if (face >= source->count || seen[face]) { goto done; }
        seen[face] = 1;
        for (DWORD k = 0; k < 3; k++)
        {
            DWORD corner = imported->sourcevertices[i * 3 + k];
            if (corner / 3 != face || corner % 3 != (id % 3 + k) % 3) { goto done; }
        }
    }
    same = TRUE;
done:
    free(seen); return same;
}

typedef struct ModelTopologyFace { DWORD reference, imported; } ModelTopologyFace;
static int TopologyFaceCompare(const void *a, const void *b)
{
    const ModelTopologyFace *x = a, *y = b;
    if (x->reference != y->reference) { return x->reference < y->reference ? -1 : 1; }
    return x->imported < y->imported ? -1 : x->imported > y->imported;
}

static BOOL TopologyStateEqual(const BgRenderState *a, const BgRenderState *b)
{
    return a->othermode == b->othermode && a->othermodehigh == b->othermodehigh
        && a->othermodeknown == b->othermodeknown && a->othermodehighknown == b->othermodehighknown
        && a->geometryknown == b->geometryknown && a->geometrymode == b->geometrymode && a->zbuffer == b->zbuffer
        && a->environmentalpha == b->environmentalpha && a->primitivealpha == b->primitivealpha
        && a->primitiveword0 == b->primitiveword0 && a->primitiveword1 == b->primitiveword1
        && a->surfacepolicy == b->surfacepolicy && a->surfacebasemode == b->surfacebasemode;
}

static BOOL TopologyRigid(const unsigned char *data, const ModelSource *source, DWORD list, const char **why)
{
    const ModelSourceList *part = &source->lists[list];
    DWORD matrix = 0;
    double translation[3] = {0}; BOOL first = TRUE;
    *why = "Retriangulation requires a rigid, vertex-colored part without generated reflection UVs. Keep the original topology for skinned or normal-lit parts.";
    if (part->preserve) { return FALSE; }
    for (DWORD pc = part->offset; pc < part->end; pc += 8)
        if (data[pc] == 1)
        {
            DWORD next = Read32(data + pc + 4);
            if (matrix && matrix != next) { return FALSE; }
            matrix = next;
        }
    for (DWORD f = 0; f < source->count; f++) if (source->faces[f].list == list)
    {
        if (source->faces[f].normalmask || (source->flags[f] & BG_RENDER_ENVIRONMENT_MASK)) { return FALSE; }
        for (DWORD k = 0; k < 3; k++)
        {
            DWORD id = f * 3 + k, at = source->vertexoffsets[id];
            const BgVertex *v = &source->vertices[id];
            double position[3] = {v->x, v->y, v->z};
            if (at > source->lists[0].offset || source->lists[0].offset - at < 16) { return FALSE; }
            for (int axis = 0; axis < 3; axis++)
            {
                double delta = position[axis] - Read16(data + at + axis * 2);
                if (!isfinite(delta) || (!first && fabs(delta - translation[axis]) > .001)) { return FALSE; }
                if (first) { translation[axis] = delta; }
            }
            first = FALSE;
        }
    }
    return !first;
}

static BOOL TopologySpan(DWORD offset, DWORD count, DWORD stride, DWORD limit)
{ return offset <= limit && count <= (limit - offset) / stride; }

/* Reuse the existing allocation for reductions and same-sized reimports.
 * Larger arrays get a new bounded allocation before the display lists. */
static DWORD TopologyStore(ModelOutput *out, DWORD old, DWORD capacity,
    const unsigned char *bytes, DWORD count, DWORD stride)
{
    DWORD at = old;
    if (count <= capacity)
    {
        if (capacity) { memset(out->data + at, 0, (size_t)capacity * stride); }
        if (count) { memcpy(out->data + at, bytes, (size_t)count * stride); }
    }
    else
    {
        static const unsigned char zero[16] = {0};
        Append(out, zero, (16 - out->size % 16) % 16); at = out->size;
        Append(out, bytes, count * stride);
    }
    return at;
}

/* Collision-point indices are also used by character blood-stain propagation.
 * Regenerate their XYZ groups and acyclic per-vertex chains with the new Vtx
 * numbering. Cross-node associations require a joint-aware remesher. */
static BOOL TopologyBuffers(ModelOutput *out, const unsigned char *data, const ModelSource *source,
    ModelVertexEdit *edits, const DWORD *owners, DWORD editcount, ModelVertexBuffer *buffers, const char **why)
{
    DWORD limit = source->lists[0].offset;
    for (DWORD list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        ModelVertexBuffer *buffer = &buffers[list];
        DWORD previous, count = 0, oldcount, pointcount = 0, pointbase = 0, linkbase = 0;
        for (previous = 0; previous < list; previous++)
            if (source->lists[previous].vertexpointer == part->vertexpointer) { break; }
        if (previous < list) { *buffer = buffers[previous]; continue; }
        for (DWORD v = 0; v < editcount; v++) { count += owners[v] == part->vertexpointer; }
        if (!count) { continue; }
        *why = "This part's native vertex or collision tables cannot be safely rebuilt.";
        if (part->preserve || !TopologySpan(part->vertexpointer, 1, 6, limit)) { return FALSE; }
        oldcount = (DWORD)(unsigned short)Read16(data + part->vertexpointer + 4);
        if (count > 32767 || !TopologySpan(part->vertexbase, oldcount, 16, limit)) { return FALSE; }
        /* A dynamic/other node must never lose the buffer it still references. */
        for (DWORD other = 0; other < source->listcount; other++)
        {
            const ModelSourceList *p = &source->lists[other];
            if (p->preserve && p->vertexpointer == part->vertexpointer) { return FALSE; }
            if (p->vertexpointer == part->vertexpointer || !TopologySpan(p->vertexpointer, 1, 6, limit)) { continue; }
            DWORD n = (unsigned short)Read16(data + p->vertexpointer + 4);
            if (p->vertexbase < part->vertexbase + oldcount * 16
                && part->vertexbase < p->vertexbase + n * 16) { return FALSE; }
        }
        if (part->pointusagepointer)
        {
            if (part->pointusagepointer != part->vertexpointer + 12 || !TopologySpan(part->vertexpointer, 1, 16, limit)) { return FALSE; }
            pointcount = (unsigned short)Read16(data + part->vertexpointer + 6);
            DWORD points = Read32(data + part->vertexpointer + 8), links = Read32(data + part->pointusagepointer);
            pointbase = points & 0xffffffu; linkbase = links & 0xffffffu;
            if ((pointcount && (points >> 24) != 5) || (links && (links >> 24) != 5)
                || !TopologySpan(pointbase, pointcount, 16, limit)
                || (links && !TopologySpan(linkbase, oldcount, 2, limit))) { return FALSE; }
            /* Check every part for incoming as well as outgoing associations. */
            for (DWORD other = 0; other < source->listcount; other++)
            {
                const ModelSourceList *p = &source->lists[other];
                if (!p->pointusagepointer) { continue; }
                if (!TopologySpan(p->vertexpointer, 1, 12, limit)) { return FALSE; }
                DWORD n = (unsigned short)Read16(data + p->vertexpointer + 6);
                DWORD at = Read32(data + p->vertexpointer + 8) & 0xffffffu;
                if (!TopologySpan(at, n, 16, limit)) { return FALSE; }
                for (DWORD i = 0; i < n; i++) if (Read32(data + at + i * 16 + 8))
                { *why = "Retriangulation cannot yet rebuild collision-point links between model parts. Keep this model's original topology."; return FALSE; }
            }
        }
        unsigned char *vertices = malloc((size_t)count * 16), *points = calloc(count, 16), *links = malloc((size_t)count * 2);
        DWORD *last = malloc((size_t)count * sizeof(*last));
        DWORD slots = 1; while (slots < count * 2) { slots *= 2; }
        DWORD *groups = calloc(slots, sizeof(*groups));
        if (!vertices || !points || !links || !last || !groups)
        { free(vertices); free(points); free(links); free(last); free(groups); *why = "Out of memory rebuilding model vertex tables."; return FALSE; }
        DWORD cursor = 0, unique = 0;
        memset(links, 0xff, (size_t)count * 2);
        for (DWORD v = 0; v < editcount; v++) if (owners[v] == part->vertexpointer)
        {
            memcpy(vertices + cursor * 16, edits[v].bytes, 16);
            edits[v].target = cursor * 16; cursor++;
        }
        for (DWORD v = 0; v < count; v++)
        {
            DWORD h = ModelDataHash(vertices + v * 16, 6) & (slots - 1);
            while (groups[h] && memcmp(points + (groups[h] - 1) * 16, vertices + v * 16, 6)) { h = (h + 1) & (slots - 1); }
            DWORD p = groups[h] ? groups[h] - 1 : unique;
            if (!groups[h])
            {
                memcpy(points + p * 16, vertices + v * 16, 6);
                WriteRounded16(points + p * 16 + 6, v);
                WriteRounded16(points + p * 16 + 12, -1); groups[h] = ++unique;
            }
            else { WriteRounded16(links + last[p] * 2, v); }
            last[p] = v;
        }
        buffer->base = part->vertexbase; buffer->count = oldcount;
        buffer->newbase = TopologyStore(out, part->vertexbase, oldcount, vertices, count, 16);
        buffer->newcount = count;
        if (!out->failed)
        {
            Write32(out->data + part->vertexpointer, 0x05000000u | buffer->newbase);
            WriteRounded16(out->data + part->vertexpointer + 4, count);
            for (DWORD v = 0; v < editcount; v++) if (owners[v] == part->vertexpointer) { edits[v].target += buffer->newbase; }
            if (part->pointusagepointer)
            {
                DWORD p = TopologyStore(out, pointbase, pointcount, points, unique, 16);
                DWORD l = TopologyStore(out, linkbase, Read32(data + part->pointusagepointer) ? oldcount : 0, links, count, 2);
                if (!out->failed)
                {
                    WriteRounded16(out->data + part->vertexpointer + 6, unique);
                    Write32(out->data + part->vertexpointer + 8, 0x05000000u | p);
                    Write32(out->data + part->pointusagepointer, 0x05000000u | l);
                }
            }
        }
        free(vertices); free(points); free(links); free(last); free(groups);
        if (out->failed) { *why = "Out of memory rebuilding model vertex tables."; return FALSE; }
    }
    return TRUE;
}

static BOOL TopologyList(ModelOutput *out, const unsigned char *data, const ModelSource *source,
    DWORD list, DWORD *cursor, const unsigned short *choices, const ModelVertexEdit *edits,
    const DWORD *corners, const ModelVertexBuffer *buffer, const char **why)
{
    const ModelSourceList *part = &source->lists[list];
    BgMaterial original = part->initial, current = part->initial;
    ModelLoadState state = {0}, recipes[16] = {{0}};
    DWORD segments[16] = {0};
    unsigned valid = 0; BOOL synced = FALSE;
    state.mode0 = current.modeword0; state.mode1 = current.modeword1;
    current.textureword0 = current.textureword1 = 0;
    current.combineword0 = current.combineword1 = 0;
    if (out->failed || part->pointer + 4 > out->size) { return FALSE; }
    Write32(out->data + part->pointer, 0x05000000u | out->size);
    for (DWORD pc = part->offset; pc < part->end; pc += 8)
    {
        const unsigned char *cmd = data + pc;
        if (cmd[0] == 4)
        {
            int first = cmd[1] & 15, end = first + (cmd[1] >> 4) + 1;
            if (end > 16) { return FALSE; }
            for (int slot = first; slot < end; slot++)
            {
                recipes[slot] = state; recipes[slot].mode0 = original.modeword0;
                recipes[slot].mode1 = original.modeword1; valid |= 1u << slot;
                segments[slot] = cmd[4];
            }
            continue;
        }
        if (cmd[0] == 0xbf || cmd[0] == 0xb1)
        {
            while (*cursor < source->count && source->faces[*cursor].list == list && source->faces[*cursor].command == pc)
            {
                DWORD first = *cursor, end = first, variants[16], count = 0;
                unsigned char idx[3];
                Indices(cmd, source->faces[first].slot, idx);
                for (int k = 0; k < 3; k++)
                    if (idx[k] >= 16 || !(valid & (1u << idx[k]))
                        || memcmp(&recipes[idx[0]], &recipes[idx[k]], sizeof(state)))
                    { *why = "Retriangulation cannot combine this part's different vertex-load states."; return FALSE; }
                BgMaterial desired = source->faces[first].material;
                if (choices[first] != BG_TEX_ID(source->tags[first])) { BgMaterialSetTexture(&desired, choices[first]); }
                /* Build a working set of at most 16 exact vertices. Keep face
                 * order, including intentionally doubled/reversed triangles. */
                for (; end < source->count && source->faces[end].list == list && source->faces[end].command == pc; end++)
                {
                    BgMaterial next = source->faces[end].material;
                    unsigned char nextidx[3]; Indices(cmd, source->faces[end].slot, nextidx);
                    if (choices[end] != BG_TEX_ID(source->tags[end])) { BgMaterialSetTexture(&next, choices[end]); }
                    if (!BgMaterialEqual(&next, &desired)) { break; }
                    for (int k = 0; k < 3; k++)
                        if (nextidx[k] >= 16 || !(valid & (1u << nextidx[k]))
                            || memcmp(&recipes[idx[0]], &recipes[nextidx[k]], sizeof(state)))
                        { *why = "Retriangulation cannot combine this part's different vertex-load states."; return FALSE; }
                    DWORD missing = 0;
                    for (DWORD k = 0; k < 3; k++)
                    {
                        DWORD v = 0; while (v < count && variants[v] != corners[end * 3 + k]) { v++; }
                        missing += v == count;
                    }
                    if (count + missing > 16) { break; }
                    for (DWORD k = 0; k < 3; k++)
                    {
                        DWORD v = 0; while (v < count && variants[v] != corners[end * 3 + k]) { v++; }
                        if (v == count) { variants[count++] = corners[end * 3 + k]; }
                    }
                }
                if (end == first) { return FALSE; }
                /* Sorting the working set coalesces adjacent native records
                 * into multi-vertex G_VTX commands, instead of loading seams
                 * one vertex at a time. It never changes triangle order. */
                for (DWORD i = 1; i < count; i++)
                {
                    DWORD v = variants[i], j = i;
                    while (j && edits[variants[j - 1]].target > edits[v].target)
                    { variants[j] = variants[j - 1]; j--; }
                    variants[j] = v;
                }
                ModelLoadState saved = state, load = recipes[idx[0]];
                if (choices[first] != BG_TEX_NONE) { load.mode0 |= 1u; }
                if (!LoadState(out, &state, &load, &current, why)) { return FALSE; }
                for (DWORD slot = 0; slot < count;)
                {
                    DWORD start = slot++, address;
                    while (slot < count && edits[variants[slot]].target == edits[variants[slot - 1]].target + 16) { slot++; }
                    DWORD n = slot - start;
                    /* Gun display-list nodes bind only segment 5. Preserve
                     * the original addressing convention: segment 4 is
                     * relative to a runtime vertex buffer, while segment 5
                     * is relative to the complete model file. */
                    if (!SplitVertexAddress(buffer, &edits[variants[start]], variants[start], segments[idx[0]], &address, why)) { return FALSE; }
                    Command(out, 0x04000000u | ((n - 1) << 20) | (start << 16) | (n * 16), address);
                }
                if (!LoadState(out, &state, &saved, &current, why)) { return FALSE; }
                /* Reuse a retained pipe sync so repeated topology imports do
                 * not accumulate an extra barrier before every material. */
                BOOL change = !BgMaterialEqual(&current, &desired);
                MaterialWithSync(out, &current, &desired, !synced);
                if (change) { synced = TRUE; }
                state.mode0 = current.modeword0; state.mode1 = current.modeword1;
                unsigned char triangles[4][3]; int pending = 0;
                for (DWORD f = first; f < end; f++)
                {
                    for (DWORD k = 0; k < 3; k++)
                    {
                        DWORD slot = 0; while (slot < count && variants[slot] != corners[f * 3 + k]) { slot++; }
                        if (slot == count) { return FALSE; }
                        triangles[pending][k] = (unsigned char)slot;
                    }
                    if (++pending == 4) { Triangles(out, triangles, pending); pending = 0; }
                }
                Triangles(out, triangles, pending); *cursor = end; synced = FALSE;
            }
            continue;
        }
        if (BgMaterialReadCommand(&original, Read32(cmd), Read32(cmd + 4))) { continue; }
        if (cmd[0] == 0xbe || cmd[0] == 0xbd || cmd[0] == 0xb2 || (cmd[0] == 0xbc && cmd[3] == 0x0c))
        { *why = "Retriangulation cannot preserve this part's vertex-cache or matrix-stack modifications."; return FALSE; }
        Append(out, cmd, 8);
        if (cmd[0] == 0xe7) { synced = TRUE; }
        if (cmd[0] == 1) { state.matrix = Read32(cmd + 4); }
        else if (cmd[0] == 0xb6 || cmd[0] == 0xb7)
        {
            DWORD bits = Read32(cmd + 4) & 0x001f0004u; state.known |= bits;
            if (cmd[0] == 0xb6) { state.geometry &= ~bits; } else { state.geometry |= bits; }
        }
        else if (cmd[0] < 0xc0 && cmd[0] != 0xb8 && cmd[0] != 0xb9 && cmd[0] != 0xba && cmd[0] != 0) { state.epoch++; }
    }
    return !out->failed;
}

static BOOL CompileRetopology(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, BOOL headgeometry, const char **why)
{
    ModelSource expanded = *source;
    ModelTopologyFace *order = NULL;
    ModelVertexEdit *edits = NULL;
    ModelVertexBuffer *buffers = NULL;
    DWORD *references = NULL, *corners = NULL, *owners = NULL, *hashes = NULL;
    unsigned short *choices = NULL;
    ModelOutput out = {0}; BOOL ok = FALSE;
    DWORD editcount = 0, hashcount = 1, facecursor = 0;
    *result = NULL; *resultsize = 0;
    expanded.faces = NULL; expanded.tags = NULL;
    size = ModelMaterialsNativeSize(data, size);
    *why = "Retriangulated imports must contain 1 to 10000 faces and retain their exported material slots.";
    if (!imported->count || imported->count > 10000 || !source->listcount
        || source->lists[0].offset > size || size > MODEL_LIMIT
        || !source->materials.count || imported->materials.facecount != imported->count) { goto done; }
    references = malloc((size_t)imported->materials.count * sizeof(*references));
    order = malloc((size_t)imported->count * sizeof(*order));
    expanded.faces = malloc((size_t)imported->count * sizeof(*expanded.faces));
    expanded.tags = malloc((size_t)imported->count * sizeof(*expanded.tags));
    choices = malloc((size_t)imported->count * sizeof(*choices));
    edits = malloc((size_t)imported->count * 3 * sizeof(*edits));
    corners = malloc((size_t)imported->count * 3 * sizeof(*corners));
    owners = malloc((size_t)imported->count * 3 * sizeof(*owners));
    buffers = calloc(source->listcount, sizeof(*buffers));
    while (hashcount < imported->count * 6) { hashcount *= 2; }
    hashes = calloc(hashcount, sizeof(*hashes));
    *why = "Out of memory rebuilding model topology.";
    if (!references || !order || !expanded.faces || !expanded.tags || !choices || !edits || !corners || !owners || !buffers || !hashes) { goto done; }
    for (DWORD slot = 0; slot < imported->materials.count; slot++)
    {
        /* A detached rigid head has one attachment, so new material names
         * need no exported part IDs. This opt-in is never used for bodies. */
        if (headgeometry) { references[slot]=0;continue; }
        DWORD original = MODEL_NO_VERTEX, reference = MODEL_NO_VERTEX;
        for (DWORD s = 0; s < source->materials.count; s++)
            if (!strcmp(imported->materials.slots[slot].name, source->materials.slots[s].name))
            {
                if (original != MODEL_NO_VERTEX) { goto ambiguous; }
                original = s;
            }
        for (DWORD f = 0; f < source->count; f++) if (source->materials.faces[f].slot == original)
        {
            if (reference == MODEL_NO_VERTEX) { reference = f; }
            else if (source->faces[f].list != source->faces[reference].list
                || source->flags[f] != source->flags[reference]
                || !TopologyStateEqual(&source->faces[f].state, &source->faces[reference].state)
                || !BgMaterialEqual(&source->faces[f].material, &source->faces[reference].material)) { goto ambiguous; }
        }
        if (reference == MODEL_NO_VERTEX) { goto ambiguous; }
        references[slot] = reference;
    }
    for (DWORD f = 0; f < imported->count; f++)
    {
        DWORD slot = imported->materials.faces[f].slot;
        if (slot >= imported->materials.count) { goto ambiguous; }
        order[f] = (ModelTopologyFace){references[slot], f};
    }
    qsort(order, imported->count, sizeof(*order), TopologyFaceCompare);
    if (!ModelMaterialsCopy(ordered, &imported->materials, why)) { goto done; }
    for (DWORD list = 0; list < source->listcount; list++)
    {
        BOOL used = FALSE;
        for (DWORD f = 0; f < imported->count; f++) { used |= source->faces[order[f].reference].list == list; }
        if (used && !TopologyRigid(data, source, list, why)) { goto done; }
    }
    for (DWORD f = 0; f < imported->count; f++)
    {
        DWORD ref = order[f].reference, input = order[f].imported;
        const ModelSourceFace *face = &source->faces[ref];
        const ModelSourceList *part = &source->lists[face->list];
        BgMaterial material = face->material; int width = 1, height = 1;
        unsigned short texture = BG_TEX_ID(imported->tags[input]);
        expanded.faces[f] = *face; expanded.tags[f] = source->tags[ref]; choices[f] = texture;
        ordered->faces[f] = imported->materials.faces[input];
        if (texture != BG_TEX_ID(source->tags[ref])) { BgMaterialSetTexture(&material, texture); }
        if (texture != BG_TEX_NONE && !TexGetProjectImageSize(projectdir, texture, &width, &height))
        { *why = "An assigned model image is missing from the project."; goto done; }
        BgRenderAlpha alpha = BgRenderGetMaterialAlpha(&face->state, &material);
        for (DWORD k = 0; k < 3; k++)
        {
            ModelVertexEdit edit = {0};
            if (!PrepareVertexEdit(&edit, data, source, ref * 3 + k, &imported->vertices[input * 3 + k],
                width, height, texture != BG_TEX_NONE, alpha, why)) { goto done; }
            /* Only native records owned by this part may use its segment 4. */
            DWORD count = (unsigned short)Read16(data + part->vertexpointer + 4);
            if (edit.offset < part->vertexbase || edit.offset - part->vertexbase >= count * 16)
            { *why = "Retriangulation cannot remap this part's externally shared vertices."; goto done; }
            DWORD h = (ModelDataHash(edit.bytes, 16) ^ part->vertexpointer) & (hashcount - 1);
            while (hashes[h] && (owners[hashes[h] - 1] != part->vertexpointer
                || memcmp(edits[hashes[h] - 1].bytes, edit.bytes, 16))) { h = (h + 1) & (hashcount - 1); }
            if (!hashes[h]) { owners[editcount] = part->vertexpointer; edits[editcount] = edit; hashes[h] = ++editcount; }
            else { edits[hashes[h] - 1].textured |= edit.textured; }
            corners[f * 3 + k] = hashes[h] - 1;
        }
        const unsigned char *a = edits[corners[f * 3]].bytes, *b = edits[corners[f * 3 + 1]].bytes, *c = edits[corners[f * 3 + 2]].bytes;
        double ab[3], ac[3];
        for (int axis = 0; axis < 3; axis++) { ab[axis] = Read16(b + axis * 2) - Read16(a + axis * 2); ac[axis] = Read16(c + axis * 2) - Read16(a + axis * 2); }
        if (ab[0] * ac[1] == ab[1] * ac[0] && ab[0] * ac[2] == ab[2] * ac[0] && ab[1] * ac[2] == ab[2] * ac[1])
        { *why = "A rebuilt triangle collapses at native integer precision."; goto done; }
    }
    expanded.count = imported->count;
    Append(&out, data, source->lists[0].offset);
    if (out.failed || !TopologyBuffers(&out, data, source, edits, owners, editcount, buffers, why)) { goto done; }
    /* PointUsage is an s16 array; pad its tail for native Gfx DMA alignment. */
    static const unsigned char padding[8] = {0};
    Append(&out, padding, (8 - out.size % 8) % 8);
    if (out.failed) { goto done; }
    for (DWORD list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        if (part->preserve)
        {
            Write32(out.data + part->pointer, 0x05000000u | out.size);
            Append(&out, data + part->offset, part->end - part->offset);
        }
        else if (!TopologyList(&out, data, &expanded, list, &facecursor, choices, edits, corners, &buffers[list], why)) { goto done; }
    }
    if (out.failed || facecursor != imported->count) { goto done; }
    *result = out.data; *resultsize = out.size; out.data = NULL; *why = ""; ok = TRUE;
    goto done;
ambiguous:
    *why = "New triangles cannot be matched to one native part. Retain the exported material slot names and keep each slot within its original part.";
done:
    if (!ok) { ModelMaterialsFree(ordered); }
    free(out.data); free(references); free(order); free(expanded.faces); free(expanded.tags);
    free(choices); free(edits); free(corners); free(owners); free(hashes); free(buffers); return ok;
}

BOOL ModelCompileRetopology(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, const char **why)
{
    return CompileRetopology(data,size,source,imported,projectdir,ordered,result,resultsize,FALSE,why);
}

BOOL ModelCompileHeadGeometry(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir, ModelMaterials *ordered,
    unsigned char **result, DWORD *resultsize, const char **why)
{
    *result=NULL;*resultsize=0;
    /* Separate hair/LOD branches need authored assignments. Do not guess
     * which triangles a cap or distance switch should hide. */
    if (!source->count || source->listcount!=1 || source->haslods)
    { *why="New head geometry needs a single-part template such as CheadbrosnanZ. Multi-part templates require a GEditor export.";return FALSE; }
    return CompileRetopology(data,size,source,imported,projectdir,ordered,result,resultsize,TRUE,why);
}

/* Closest point on a triangle, with barycentric coordinates for transferring
 * its dominant native joint. Surface distance avoids attaching coat vertices
 * to a nearby hand merely because the torso has fewer vertices. */
static double BodyTriangleDistance(const double p[3],const BgVertex v[3],double weights[3])
{
    double q[3][3]={{v[0].x,v[0].y,v[0].z},{v[1].x,v[1].y,v[1].z},{v[2].x,v[2].y,v[2].z}};
    double ab[3],ac[3],ap[3],aa=0,bb=0,cc=0,dd=0,ee=0,best=1e100;
    for(int a=0;a<3;a++) {
        ab[a]=q[1][a]-q[0][a];ac[a]=q[2][a]-q[0][a];ap[a]=p[a]-q[0][a];
        aa+=ab[a]*ab[a];bb+=ab[a]*ac[a];cc+=ac[a]*ac[a];dd+=ap[a]*ab[a];ee+=ap[a]*ac[a];
    }
    double det=aa*cc-bb*bb;
    if(det>1e-12) {
        double u=(cc*dd-bb*ee)/det,w=(aa*ee-bb*dd)/det;
        if(u>=0 && w>=0 && u+w<=1) {
            best=0;for(int a=0;a<3;a++) {double d=ap[a]-u*ab[a]-w*ac[a];best+=d*d;}
            weights[0]=1-u-w;weights[1]=u;weights[2]=w;return best;
        }
    }
    for(int edge=0;edge<3;edge++) {
        int next=(edge+1)%3;double length=0,dot=0,distance=0;
        for(int a=0;a<3;a++) {double d=q[next][a]-q[edge][a];length+=d*d;dot+=(p[a]-q[edge][a])*d;}
        double t=length>0 ? fmax(0,fmin(1,dot/length)) : 0;
        for(int a=0;a<3;a++) {double d=p[a]-q[edge][a]-t*(q[next][a]-q[edge][a]);distance+=d*d;}
        if(distance<best) {best=distance;weights[0]=weights[1]=weights[2]=0;weights[edge]=1-t;weights[next]=t;}
    }
    return best;
}

typedef struct BodyPoint {
    float xyz[3];
    DWORD corner;
} BodyPoint;
typedef struct BodyNeighbours {
    DWORD matrix, reference, distinct[3];
    unsigned count;
    BOOL mixed, fix;
} BodyNeighbours;
static int BodyPointCompare(const void *left,const void *right)
{
    const BodyPoint *a=left,*b=right;
    for(int axis=0;axis<3;axis++) {
        if(a->xyz[axis]<b->xyz[axis]) return -1;
        if(a->xyz[axis]>b->xyz[axis]) return 1;
    }
    return 0;
}
/* Locate the native joint owning a primary or half-rotation matrix. Never
 * infer bone relationships from consecutive matrix numbers. */
static DWORD BodyMatrixNode(const unsigned char *data,DWORD size,const ModelSource *source,DWORD matrix)
{
    for(DWORD l=0;l<source->listcount;l++) {
        DWORD node=source->lists[l].node;unsigned visited=0;
        while(node && visited++<512) {
            if(!TopologySpan(node,1,24,size)) return 0;
            unsigned flags=(unsigned short)Read16(data+node),op=flags&255;
            DWORD at=Read32(data+node+4)&0xffffffu;
            if(op==1 || op==2 || op==0x15) {
                DWORD field=op==1 ? 2 : op==0x15 ? 12 : 14;
                if(!TopologySpan(at,1,field+2,size)) return 0;
                if(Read16(data+at+field)==(int)matrix) return node;
                if(op==2 && (flags&0x100) && TopologySpan(at,1,18,size)
                    && Read16(data+at+16)==(int)matrix) return node;
            }
            node=Read32(data+node+8)&0xffffffu;
        }
    }
    return 0;
}
static DWORD BodyParentJoint(const unsigned char *data,DWORD size,DWORD node)
{
    unsigned visited=0;
    while(node && visited++<512) {
        if(!TopologySpan(node,1,24,size)) return 0;
        node=Read32(data+node+8)&0xffffffu;
        if(!node) return 0;
        if(!TopologySpan(node,1,24,size)) return 0;
        unsigned op=(unsigned short)Read16(data+node)&255;
        if(op==1 || op==2 || op==0x15) return node;
    }
    return 0;
}
/* Arms-down bodies can nearly touch their hips. Surface distance alone can
 * bind one sleeve point to the torso. Weld positions only for this analysis
 * (retain all UV/color splits), then correct unanimous, isolated outliers.
 * Mixed bindings and parent/child/half-rotation seams remain authored. */
static BOOL BodyFixIsolatedBindings(const unsigned char *data,DWORD size,const ModelSource *source,
    const BgVertex *vertices,DWORD count,DWORD *references,DWORD *fixed,const char **why)
{
    DWORD corners=count*3,points=0;
    BodyPoint *sorted=malloc((size_t)corners*sizeof(*sorted));
    DWORD *groups=malloc((size_t)corners*sizeof(*groups)),*matrices=malloc((size_t)corners*sizeof(*matrices));
    BodyNeighbours *neighbours=calloc(corners,sizeof(*neighbours));
    *fixed=0;
    if(!sorted || !groups || !matrices || !neighbours) {
        *why="Out of memory checking body joint connectivity.";
        free(sorted);free(groups);free(matrices);free(neighbours);return FALSE;
    }
    for(DWORD i=0;i<corners;i++) {
        sorted[i]=(BodyPoint){{vertices[i].x,vertices[i].y,vertices[i].z},i};
    }
    qsort(sorted,corners,sizeof(*sorted),BodyPointCompare);
    for(DWORD i=0;i<corners;i++) {
        DWORD corner=sorted[i].corner,matrix=source->vertexmatrices[references[corner]];
        if(!i || BodyPointCompare(sorted+i-1,sorted+i)) {
            matrices[points]=matrix;neighbours[points].matrix=MODEL_NO_VERTEX;points++;
        } else if(matrices[points-1]!=matrix) matrices[points-1]=MODEL_NO_VERTEX;
        groups[corner]=points-1;
    }
    for(DWORD f=0;f<count;f++) for(DWORD k=0;k<3;k++) {
        DWORD point=groups[f*3+k];BodyNeighbours *n=&neighbours[point];
        for(DWORD j=0;j<3;j++) if(j!=k) {
            DWORD other=groups[f*3+j],matrix=matrices[other];
            if(other==point) continue;
            if(matrix==MODEL_NO_VERTEX) {n->mixed=TRUE;continue;}
            if(n->matrix==MODEL_NO_VERTEX) {n->matrix=matrix;n->reference=references[f*3+j];}
            else if(n->matrix!=matrix) n->mixed=TRUE;
            unsigned seen=0;while(seen<n->count && n->distinct[seen]!=other) seen++;
            if(seen==n->count && n->count<3) n->distinct[n->count++]=other;
        }
    }
    /* Decisions use the original neighbours, so face order cannot propagate
     * a correction across the mesh or turn a whole region into another limb. */
    for(DWORD p=0;p<points;p++) {
        BodyNeighbours *n=&neighbours[p];
        if(n->mixed || n->count<3 || matrices[p]==MODEL_NO_VERTEX || n->matrix==matrices[p]) continue;
        DWORD a=BodyMatrixNode(data,size,source,matrices[p]),b=BodyMatrixNode(data,size,source,n->matrix);
        if(!a || !b || a==b || BodyParentJoint(data,size,a)==b || BodyParentJoint(data,size,b)==a) continue;
        n->fix=TRUE;(*fixed)++;
    }
    for(DWORD i=0;i<corners;i++) {
        DWORD p=groups[i];BodyNeighbours *n=&neighbours[p];
        if(n->fix) references[i]=n->reference;
    }
    free(sorted);free(groups);free(matrices);free(neighbours);return TRUE;
}

static BOOL CompileBodyGeometry(const unsigned char *data,DWORD size,const ModelSource *source,
    const GltfModelImport *imported,BOOL fit,ModelMaterials *ordered,
    unsigned char **result,DWORD *resultsize,BOOL repair,DWORD *fixed,const char **why)
{
    ModelOutput out={0};BOOL ok=FALSE;
    BgVertex *standing=NULL;ModelTransform *transforms=NULL;
    ModelVertexEdit *edits=NULL;ModelVertexBuffer *buffers=NULL;
    DWORD *owners=NULL,*matrices=NULL,*corners=NULL,*drawlists=NULL,*hashes=NULL;
    DWORD *sourceowners=NULL,*references=NULL;unsigned char *prefix=NULL,*required=NULL,*used=NULL;
    DWORD unique=0,hashcount=1,emitted=0;
    double lo[2][3],hi[2][3],scale=1;
    *result=NULL;*resultsize=0;
    *why="A raw body must contain 1 to 10000 opaque triangles and a complete standing, arms-down humanoid mesh.";
    if (!imported->count || imported->count>10000 || !source->count || !source->listcount
        || !source->vertexmatrices || imported->materials.facecount!=imported->count) goto done;
    if (!ModelBodyBindPose(data,size,source,&standing,&transforms,why)) goto done;
    for (int mesh=0;mesh<2;mesh++) {
        BOOL first=TRUE;DWORD count=mesh ? imported->count : source->count;
        const BgVertex *v=mesh ? imported->vertices : standing;
        for (DWORD i=0;i<count*3;i++) {
            if (!mesh && !source->faces[i/3].closest) continue;
            double p[3]={v[i].x,v[i].y,v[i].z};
            for (int a=0;a<3;a++) {
                if (!isfinite(p[a])) { *why="The body has a non-finite position.";goto done; }
                if (first || p[a]<lo[mesh][a]) lo[mesh][a]=p[a];
                if (first || p[a]>hi[mesh][a]) hi[mesh][a]=p[a];
            }
            first=FALSE;
        }
        if (first || hi[mesh][1]-lo[mesh][1]<1e-6) { *why="The body and its template must have nonzero height.";goto done; }
    }
    if (fit) scale=(hi[0][1]-lo[0][1])/(hi[1][1]-lo[1][1]);
    DWORD limit=source->lists[0].offset;
    if (limit>size) goto done;
    edits=calloc((size_t)imported->count*3,sizeof(*edits));
    owners=malloc((size_t)imported->count*3*sizeof(*owners));
    matrices=malloc((size_t)imported->count*3*sizeof(*matrices));
    corners=malloc((size_t)imported->count*3*sizeof(*corners));
    drawlists=malloc((size_t)imported->count*sizeof(*drawlists));
    sourceowners=malloc((size_t)source->count*3*sizeof(*sourceowners));
    references=malloc((size_t)imported->count*3*sizeof(*references));
    buffers=calloc(source->listcount,sizeof(*buffers));prefix=malloc(limit);
    required=calloc(source->listcount,1);used=calloc(source->listcount,1);
    while (hashcount<imported->count*6) hashcount*=2;
    hashes=calloc(hashcount,sizeof(*hashes));
    *why="Out of memory binding the new character body.";
    if (!edits || !owners || !matrices || !corners || !drawlists || !sourceowners
        || !buffers || !prefix || !required || !used || !hashes || !references) goto done;
    memcpy(prefix,data,limit);
    /* Binding comes from G_VTX, including half-rotation seam matrices. Buffer
     * ownership can differ from the list drawing a triangle across a joint. */
    for (DWORD i=0;i<source->count*3;i++) {
        sourceowners[i]=MODEL_NO_VERTEX;
        if (!source->faces[i/3].closest) continue;
        for (DWORD l=0;l<source->listcount;l++) {
            const ModelSourceList *p=&source->lists[l];
            if (p->preserve || !TopologySpan(p->vertexpointer,1,6,limit)) continue;
            DWORD n=(unsigned short)Read16(data+p->vertexpointer+4),at=source->vertexoffsets[i];
            if (at>=p->vertexbase && at-p->vertexbase<n*16) { sourceowners[i]=l;required[l]=1;break; }
        }
        if (source->faces[i/3].normalmask || (!repair && sourceowners[i]==MODEL_NO_VERTEX))
        { *why="This template has unsupported body vertex ownership or lighting normals.";goto done; }
    }
    /* Texture/UV splitting can leave file-absolute loads referencing an older
     * copy of a joint's vertex array. Rehome those records into a live buffer
     * for the SAME matrix before rebuilding its collision-point chains. */
    if(repair) for(DWORD i=0;i<source->count*3;i++) if(sourceowners[i]==MODEL_NO_VERTEX) {
        DWORD owner=MODEL_NO_VERTEX;double best=1e100;
        for(DWORD j=0;j<source->count*3;j++) if(sourceowners[j]!=MODEL_NO_VERTEX
            && source->vertexmatrices[j]==source->vertexmatrices[i]) {
            double distance=0;
            for(int a=0;a<3;a++) {double d=Read16(data+source->vertexoffsets[i]+a*2)-Read16(data+source->vertexoffsets[j]+a*2);distance+=d*d;}
            if(distance<best) {best=distance;owner=sourceowners[j];}
        }
        if(owner==MODEL_NO_VERTEX) { *why="A body vertex has no live buffer for its joint.";goto done; }
        sourceowners[i]=owner;
    }
    for (DWORD i=0;i<imported->count*3;i++) {
        if(repair) {references[i]=i;continue;}
        const BgVertex *v=&imported->vertices[i];double p[3]={v->x,v->y,v->z};
        if (fit) for (int a=0;a<3;a++) p[a]=(p[a]-(lo[1][a]+hi[1][a])*.5)*scale+(lo[0][a]+hi[0][a])*.5;
        DWORD nearest=MODEL_NO_VERTEX;double best=1e100;
        for (DWORD f=0;f<source->count;f++) if (source->faces[f].closest) {
            double weights[3],d=BodyTriangleDistance(p,standing+f*3,weights);
            if (d<best) {
                best=d;double largest=-1;
                for (DWORD k=0;k<3;k++) {
                    double weight=0;
                    for (DWORD j=0;j<3;j++) if (source->vertexmatrices[f*3+j]==source->vertexmatrices[f*3+k]) weight+=weights[j];
                    if (weight>largest) { largest=weight;nearest=f*3+k; }
                }
            }
        }
        if (nearest==MODEL_NO_VERTEX || best>pow((hi[0][1]-lo[0][1])*.15,2))
        { *why="The body does not fit the template's standing pose. Use a complete Y-up, +Z-forward body with arms down.";goto done; }
        references[i]=nearest;
    }
    if(!BodyFixIsolatedBindings(data,size,source,imported->vertices,imported->count,references,fixed,why)) goto done;
    if(repair && !*fixed) {ok=TRUE;*why="";goto done;}
    for (DWORD i=0;i<imported->count*3;i++) {
        DWORD nearest=references[i];
        const BgVertex *v=&imported->vertices[i];double p[3]={v->x,v->y,v->z};
        if (fit) for (int a=0;a<3;a++) p[a]=(p[a]-(lo[1][a]+hi[1][a])*.5)*scale+(lo[0][a]+hi[0][a])*.5;
        DWORD owner=sourceowners[nearest],matrix=source->vertexmatrices[nearest];
        const ModelTransform *t=&transforms[nearest];ModelVertexEdit edit={0};edit.id=nearest;
        if(repair) memcpy(edit.bytes,data+source->vertexoffsets[i],16);
        /* Rotation matrices are orthonormal: transpose is the inverse. */
        for (int a=0;a<3 && (!repair || source->vertexmatrices[i]!=matrix);a++) {
            double local=0;for (int b=0;b<3;b++) local+=(p[b]-t->m[3][b])*t->m[a][b];
            if (!WriteRounded16(edit.bytes+a*2,local)) { *why="A bound body vertex is outside native coordinate range.";goto done; }
        }
        if(!repair) {edit.bytes[12]=v->r;edit.bytes[13]=v->g;edit.bytes[14]=v->b;edit.bytes[15]=v->a;}
        DWORD h=(ModelDataHash(edit.bytes,16)^source->lists[owner].vertexpointer^matrix)&(hashcount-1);
        while (hashes[h] && (owners[hashes[h]-1]!=source->lists[owner].vertexpointer
            || matrices[hashes[h]-1]!=matrix || memcmp(edits[hashes[h]-1].bytes,edit.bytes,16))) h=(h+1)&(hashcount-1);
        if (!hashes[h]) {
            owners[unique]=source->lists[owner].vertexpointer;matrices[unique]=matrix;
            edits[unique]=edit;hashes[h]=++unique;
        }
        corners[i]=hashes[h]-1;used[owner]=1;
        /* One native list owns the face; individual loads keep their joints. */
        if (i%3==0) drawlists[i/3]=repair ? source->faces[i/3].list : owner;
    }
    for (DWORD f=0;f<imported->count;f++) {
        double q[3][3],ab[3],ac[3],area=0;
        for (DWORD k=0;k<3;k++) {
            const ModelVertexEdit *v=&edits[corners[f*3+k]];
            const ModelTransform *t=&transforms[v->id];
            for (int a=0;a<3;a++) {
                q[k][a]=t->m[3][a];
                for (int b=0;b<3;b++) q[k][a]+=Read16(v->bytes+b*2)*t->m[b][a];
            }
        }
        for (int a=0;a<3;a++) {ab[a]=q[1][a]-q[0][a];ac[a]=q[2][a]-q[0][a];}
        for (int a=0;a<3;a++) {double cross=ab[(a+1)%3]*ac[(a+2)%3]-ab[(a+2)%3]*ac[(a+1)%3];area+=cross*cross;}
        if (area<1e-12) { *why="A body triangle collapses at native integer precision.";goto done; }
    }
    for (DWORD l=0;l<source->listcount;l++) {
        const ModelSourceList *p=&source->lists[l];
        if (!repair && required[l] && !used[l]) { *why="The body is missing a template body part. Import a complete standing, arms-down body, not a head or prop.";goto done; }
        if (p->preserve) { *why="This body template contains unsupported dynamic parts.";goto done; }
        if (p->pointusagepointer) {
            if (!TopologySpan(p->vertexpointer,1,16,limit)) goto done;
            DWORD n=(unsigned short)Read16(data+p->vertexpointer+6),at=Read32(data+p->vertexpointer+8)&0xffffffu;
            if (!TopologySpan(at,n,16,limit)) goto done;
            /* Old cross-part blood associations refer to discarded vertices.
             * Rebuild local point chains from the new joint-owned buffers. */
            for (DWORD j=0;j<n;j++) { Write32(prefix+at+j*16+8,0);WriteRounded16(prefix+at+j*16+12,-1); }
        }
        if (!repair && required[l]) {
            DWORD node=p->node;int visited=0;
            while (node && visited++<512) {
                if (!TopologySpan(node,1,24,limit)) goto done;
                if ((Read16(data+node)&255)==8) {
                    DWORD at=Read32(data+node+4)&0xffffffu;
                    if (!TopologySpan(at,1,8,limit)) goto done;
                    Write32(prefix+at,0);Write32(prefix+at+4,0x7f7fffffu);
                }
                node=Read32(data+node+8)&0xffffffu;
            }
            if (node) goto done;
        }
    }
    Append(&out,prefix,limit);
    if (out.failed || !TopologyBuffers(&out,prefix,source,edits,owners,unique,buffers,why)
        || !ModelMaterialsCopy(ordered,&imported->materials,why)) goto done;
    static const unsigned char padding[8]={0};Append(&out,padding,(8-out.size%8)%8);
    for (DWORD l=0;l<source->listcount;l++) {
        const ModelSourceList *part=&source->lists[l];
        if (out.failed || part->pointer+4>out.size) goto done;
        Write32(out.data+part->pointer,0x05000000u|out.size);
        if(!repair) {
            Command(&out,0xb6000000u,0x000e3000u);Command(&out,0xb7000000u,0x2205u);
            BgMaterial material;BgMaterialInit(&material);BgMaterialSetTexture(&material,BG_TEX_NONE);
            Command(&out,material.modeword0,material.modeword1);Command(&out,material.combineword0,material.combineword1);
        }
        DWORD currentmatrix=MODEL_NO_VERTEX,cache[16],nextslot=0;int pending=0;
        DWORD pc=part->offset;
        unsigned char triangles[4][3];memset(cache,0xff,sizeof(cache));
        for (DWORD f=0;f<imported->count;f++) if (drawlists[f]==l) {
            if(repair) {
                /* Keep every original material/state command and face order.
                 * Only matrix selections, vertex loads and triangles change. */
                for(;pc<source->faces[f].command;pc+=8) {
                    unsigned op=data[pc];
                    if(op==1 || op==4 || op==0xb1 || op==0xbf || op==0xb8) continue;
                    if(op==6 || op==0xbe || op==0xbd || op==0xb2 || (op==0xbc && data[pc+3]==0x0c))
                    { *why="This body's display lists use unsupported vertex-cache commands.";goto done; }
                    Triangles(&out,triangles,pending);pending=0;Append(&out,data+pc,8);
                }
            }
            ordered->faces[emitted++]=imported->materials.faces[f];
            unsigned char indices[3];unsigned locked=0;
            for (DWORD k=0;k<3;k++) {
                DWORD v=corners[f*3+k];indices[k]=16;
                for (DWORD s=0;s<16;s++) if(cache[s]==v) {indices[k]=s;locked|=1u<<s;break;}
            }
            for (DWORD k=0;k<3;k++) if(indices[k]==16) {
                DWORD v=corners[f*3+k],slot=nextslot;
                while(locked&(1u<<slot)) slot=(slot+1)%16;
                Triangles(&out,triangles,pending);pending=0;
                if (matrices[v]!=currentmatrix) { currentmatrix=matrices[v];Command(&out,0x01020040u,0x03000000u|currentmatrix*64); }
                DWORD addr=0x05000000u|edits[v].target;
                unsigned char *previous=out.data+out.size-8;
                DWORD n=(previous[1]>>4)+1,first=previous[1]&15;
                if(previous[0]==4 && first+n==slot && Read32(previous+4)+n*16==addr) {
                    /* Adjacent joint-local records can share a single DMA. */
                    Write32(previous,0x04000000u|(n<<20)|(first<<16)|((n+1)*16));
                } else Command(&out,0x04000010u|(slot<<16),addr);
                cache[slot]=v;indices[k]=slot;locked|=1u<<slot;nextslot=(slot+1)%16;
            }
            memcpy(triangles[pending++],indices,3);
            if(pending==4) {Triangles(&out,triangles,pending);pending=0;}
        }
        Triangles(&out,triangles,pending);
        if(repair) for(;pc<part->end;pc+=8) {
            unsigned op=data[pc];
            if(op==6 || op==0xbe || op==0xbd || op==0xb2 || (op==0xbc && data[pc+3]==0x0c))
            { *why="This body's display lists use unsupported vertex-cache commands.";goto done; }
            if(op!=1 && op!=4 && op!=0xb1 && op!=0xbf && op!=0xb8) Append(&out,data+pc,8);
        }
        Command(&out,0xb8000000u,0);
    }
    if (out.failed || emitted!=imported->count) goto done;
    *result=out.data;*resultsize=out.size;out.data=NULL;ok=TRUE;*why="";
done:
    if (!ok) ModelMaterialsFree(ordered);
    free(out.data);free(standing);free(transforms);free(edits);free(buffers);free(owners);free(matrices);
    free(corners);free(drawlists);free(hashes);free(sourceowners);free(references);free(prefix);free(required);free(used);return ok;
}

BOOL ModelCompileBodyGeometry(const unsigned char *data,DWORD size,const ModelSource *source,
    const GltfModelImport *imported,BOOL fit,ModelMaterials *ordered,
    unsigned char **result,DWORD *resultsize,const char **why)
{
    DWORD fixed;
    return CompileBodyGeometry(data,size,source,imported,fit,ordered,result,resultsize,FALSE,&fixed,why);
}

BOOL ModelCompileRepairBodyBindings(const unsigned char *data,DWORD size,const ModelSource *source,
    unsigned char **result,DWORD *resultsize,DWORD *fixed,const char **why)
{
    BgVertex *standing=NULL;ModelTransform *transforms=NULL;ModelMaterials ordered={0};BOOL ok=FALSE;
    *result=NULL;*resultsize=*fixed=0;
    if(source->haslods) { *why="Binding repair is for imported bodies with a single LOD.";return FALSE; }
    if(!ModelBodyBindPose(data,size,source,&standing,&transforms,why)) return FALSE;
    GltfModelImport imported={0};imported.count=source->count;imported.vertices=standing;imported.materials=source->materials;
    ok=CompileBodyGeometry(data,size,source,&imported,FALSE,&ordered,result,resultsize,TRUE,fixed,why);
    if(ok && *result) ok=ModelMaterialsAttach(result,resultsize,&ordered,why);
    if(!ok) {free(*result);*result=NULL;*resultsize=*fixed=0;}
    free(standing);free(transforms);ModelMaterialsFree(&ordered);return ok;
}

BOOL ModelCompileImport(const unsigned char *data, DWORD size, const ModelSource *source,
    const GltfModelImport *imported, const char *projectdir,
    unsigned char **result, DWORD *resultsize, const char **reasonout)
{
    unsigned short *choices=NULL;
    ModelVertexEdit *edits=NULL;
    DWORD *corners=NULL;
    ModelVertexBuffer *buffers=NULL;
    ModelOutput output={0};
    DWORD i, list, facecursor=0, editcount=0;
    BOOL changed=imported->count!=source->count, split=FALSE, ok=FALSE;
    *result=NULL; *resultsize=0;
    size=ModelMaterialsNativeSize(data,size);
    *reasonout="The model could not be rebuilt.";
    if (!source->listcount || source->lists[0].offset>size || size>MODEL_LIMIT
        || imported->count>source->count || (source->count && !source->vertexoffsets)) { goto done; }
    choices=malloc((source->count ? source->count : 1)*sizeof(*choices));
    edits=malloc((size_t)(imported->count ? imported->count : 1)*3*sizeof(*edits));
    corners=malloc((size_t)(source->count ? source->count : 1)*3*sizeof(*corners));
    if (!choices || !edits || !corners) { goto done; }
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
        if (texture != BG_TEX_ID(source->tags[face]) || (imported->rebind && imported->rebind[face]))
            BgMaterialSetTexture(&material, texture);
        if (!BgMaterialEqual(&material,&source->faces[face].material)) changed=TRUE;
        alpha = BgRenderGetMaterialAlpha(&source->faces[face].state, &material);
        if (BG_TEX_ID(source->tags[face])!=BG_TEX_NONE
            && !TexGetProjectImageSize(projectdir,BG_TEX_ID(source->tags[face]),&width,&height))
        { *reasonout="An original model texture is missing from the project."; goto done; }
        if (texture!=BG_TEX_NONE)
        {
            if (!TexGetProjectImageSize(projectdir,texture,&newwidth,&newheight))
            { *reasonout="An assigned texture is not available in the project."; goto done; }
        }
        for(corner=0;corner<3;corner++)
        {
            DWORD vertexid=imported->sourcevertices[i*3+corner];
            const BgVertex *a=&imported->vertices[i*3+corner];
            if(vertexid/3!=face || vertexid%3!=(id%3+corner)%3)
            { *reasonout="A face's vertex identities or winding changed. Preserve the exported triangles and their GUD attributes."; goto done; }
            if (!PrepareVertexEdit(&edits[editcount++], data, source, vertexid, a,
                                  newwidth, newheight, texture!=BG_TEX_NONE, alpha, reasonout)) { goto done; }
        }
        choices[face]=texture;
        if(texture!=BG_TEX_ID(source->tags[face])) { changed=TRUE; }
    }
    if (!MergeVertexEdits(edits, &editcount, corners, &split, reasonout)
        || !CheckDynamicVertices(data, source, edits, editcount, reasonout)) { goto done; }
    if(!changed && !split)
    {
        Append(&output,data,size); goto finish;
    }
    Append(&output,data,source->lists[0].offset);
    if (split)
    {
        buffers=calloc(source->listcount,sizeof(*buffers));
        if (!buffers || !SplitBuffers(&output,data,source,edits,editcount,corners,choices,buffers,reasonout)) { goto done; }
    }
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
        if (split)
        {
            DWORD face; BOOL remap=buffers[list].targets!=NULL;
            for (face=facecursor; !remap && face<source->count && source->faces[face].list==list; face++)
            {
                int k;
                if (choices[face]==MODEL_DELETED) { continue; }
                for (k=0; k<3; k++)
                {
                    const ModelVertexEdit *edit=&edits[corners[face*3+k]];
                    if (edit->target!=edit->offset) { remap=TRUE; break; }
                }
            }
            if (remap)
            {
                if (!SplitList(&output,data,source,list,&facecursor,choices,edits,corners,&buffers[list],imported->rebind,reasonout)) { goto done; }
                continue;
            }
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
                    if(choices[face]!=BG_TEX_ID(source->tags[face]) || (imported->rebind && imported->rebind[face])) { BgMaterialSetTexture(&desired,choices[face]); }
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
                else mode |= 1u; /* Assigned images need UV processing at G_VTX. */
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
    for (i=0; i<editcount; i++) { memcpy(output.data+edits[i].target, edits[i].bytes, 16); }
    *result=output.data; *resultsize=output.size; output.data=NULL; ok=TRUE; *reasonout="";
done:
    if (buffers)
    {
        for (i=0; i<source->listcount; i++) { if (buffers[i].owns) { free(buffers[i].targets); } }
    }
    free(buffers); free(corners); free(choices); free(edits); free(output.data); return ok;
}

/* Surface presets use the SDK's AA_[ZB_]OPA_SURF / TEX_EDGE / XLU_SURF
 * encodings. Only the surface bits and the active cycle's final blender input
 * change: cycle type, first-cycle fog, combiner and alpha/depth source survive.
 * These are explicit surface overrides for ordinary models, not fade controls. */
static DWORD PropertySurface(DWORD original, int surface, int modeltype)
{
    DWORD bits = surface == 0 ? 0x2078u : surface == 1 ? 0x3078u : 0x49D8u;
    DWORD blender = modeltype == 1 ? 0x000F0000u : 0x00030000u;
    DWORD mask = 0xFFF8u | blender;
    if (!(original & 0x10u)) { bits &= ~0x830u; }
    if (surface != 2) { bits |= modeltype == 1 ? 0x00050000u : 0x00010000u; }
    return (original & ~mask) | bits;
}
static void PropertyState(ModelOutput *out, DWORD fromcull, DWORD tocull,
    DWORD frommode, DWORD tomode, int modeltype)
{
    if (fromcull != tocull)
    {
        Command(out, 0xb6000000u, 0x3000u);
        if (tocull) { Command(out, 0xb7000000u, tocull); }
    }
    if (frommode != tomode)
    {
        DWORD blender = modeltype == 1 ? 0x000F0000u : 0x00030000u;
        Command(out, 0xe7000000u, 0); /* Required before changing RDP state. */
        if ((frommode ^ tomode) & 0xFFF8u)
        { Command(out, 0xb900030du, tomode & 0xFFF8u); }
        if ((frommode ^ tomode) & blender)
        { Command(out, modeltype == 1 ? 0xb9001004u : 0xb9001002u, tomode & blender); }
    }
}
static BgMaterial PropertyWrap(BgMaterial material, int wrapu, int wrapv)
{
    /* Wrapping an untextured face must not enable texturing. */
    if (BgMaterialTextureId(&material) != BG_TEX_NONE)
    {
        if (wrapu >= 0) { BgMaterialSetWrap(&material, FALSE, wrapu); }
        if (wrapv >= 0) { BgMaterialSetWrap(&material, TRUE, wrapv); }
    }
    return material;
}
static void PropertyTexture(ModelOutput *out, BgMaterial *live, const BgMaterial *desired)
{
    if ((desired->textureword0 >> 24) == 0xc0 &&
        (live->textureword0 != desired->textureword0 || live->textureword1 != desired->textureword1))
    {
        /* texLoadFromGdl supplies the pipe sync when expanding this marker. */
        Command(out, desired->textureword0, desired->textureword1);
        live->textureword0 = desired->textureword0;
        live->textureword1 = desired->textureword1;
    }
}
BOOL ModelCompileProperties(const unsigned char *data, DWORD size, const ModelSource *source,
    const DWORD *faces, DWORD count, int culling, int surface, int wrapu, int wrapv,
    unsigned char **result, DWORD *resultsize, const char **reasonout)
{
    ModelOutput out = {0};
    unsigned char *changed = NULL;
    DWORD i, list, cursor = 0, previous = 0;
    BOOL any = FALSE, ok = FALSE;
    *result = NULL; *resultsize = 0;
    size=ModelMaterialsNativeSize(data,size);
    *reasonout = "Invalid model face properties.";
    if (!data || !source || !source->listcount || !faces || !count
        || culling < -1 || culling > 2 || surface < -1 || surface > 2
        || wrapu < -1 || wrapu > 2 || wrapv < -1 || wrapv > 2) { return FALSE; }
    changed = calloc(source->count, 1);
    if (!changed) { *reasonout = "Out of memory editing model properties."; return FALSE; }
    for (i = 0; i < count; i++)
    {
        const ModelSourceFace *face;
        DWORD cull, mode;
        BgMaterial material;
        int type;
        if (faces[i] >= source->count) { goto done; }
        face = &source->faces[faces[i]];
        type = (face->state.othermodehigh & 0x00300000u) == 0 ? 1 : 4;
        cull = culling < 0 ? face->state.geometrymode & 0x3000u
            : culling == 1 ? 0x2000u : culling == 2 ? 0x1000u : 0;
        mode = surface < 0 ? face->state.othermode : PropertySurface(face->state.othermode, surface, type);
        if (culling >= 0)
        {
            /* RSP display lists cannot push/pop an unknown geometry mode.
             * Never guess the caller's culling, especially for mirrored guns. */
            if ((face->state.geometryknown & 0x3000u) != 0x3000u)
            { *reasonout = "These faces inherit culling from the game renderer. Their caller state cannot be safely restored. Select faces with an explicit culling state (such as the double-sided foliage)."; goto done; }
        }
        if (surface >= 0 && (source->lists[face->list].modeltype < 1
            || source->lists[face->list].modeltype > 4
            || (face->state.othermodehigh & 0x00200000u)))
        { *reasonout = "This part inherits a custom render pipeline. Surface presets require a standard model part using one-cycle or two-cycle rendering."; goto done; }
        changed[faces[i]] = cull != (face->state.geometrymode & 0x3000u) || mode != face->state.othermode;
        material = PropertyWrap(face->material, wrapu, wrapv);
        if (material.textureword0 != face->material.textureword0) { changed[faces[i]] |= 2; }
        any |= changed[faces[i]];
    }
    if (!any) { Append(&out, data, size); goto finish; }
    *reasonout = "The model's display-list layout could not be rebuilt.";
    for (list = 0; list < source->listcount; list++)
    {
        const ModelSourceList *part = &source->lists[list];
        DWORD pc, livecull = 0, livemode = 0, savedcull = 0, savedmode = 0;
        BOOL active = FALSE;
        BOOL wraplist = FALSE;
        BgMaterial originaltexture = part->initial, livetexture = part->initial;
        int activetype = 4;
        for (i = cursor; i < source->count && source->faces[i].list == list; i++)
        { wraplist |= (changed[i] & 2) != 0; }
        if (part->offset < previous || part->end < part->offset || part->end > size) { goto done; }
        Append(&out, data + previous, part->offset - previous);
        if (out.failed || part->pointer + 4 > out.size) { goto done; }
        Write32(out.data + part->pointer, 0x05000000u | out.size);
        for (pc = part->offset; pc < part->end; pc += 8)
        {
            DWORD first = cursor, end;
            BOOL editcommand = FALSE;
            while (cursor < source->count && source->faces[cursor].list == list
                && source->faces[cursor].command == pc)
            { editcommand |= changed[cursor++]; }
            end = cursor;
            if (!editcommand)
            {
                if (active) { PropertyState(&out, livecull, savedcull, livemode, savedmode, activetype); active = FALSE; }
                if (wraplist)
                {
                    unsigned char op = data[pc];
                    if (op == 0xc0)
                    {
                        originaltexture.textureword0 = Read32(data + pc);
                        originaltexture.textureword1 = Read32(data + pc + 4);
                        /* Fold wrap-only restores/rebinds until the next draw.
                         * Keep other texture bindings in their authored place,
                         * including detail images, mip levels and tile shifts. */
                        if (((livetexture.textureword0 ^ originaltexture.textureword0) & ~0x00f00000u)
                            || livetexture.textureword1 != originaltexture.textureword1)
                        { PropertyTexture(&out, &livetexture, &originaltexture); }
                        continue;
                    }
                    if (first < end) { PropertyTexture(&out, &livetexture, &source->faces[first].material); }
                    else if (op != 0x01 && op != 0x04 && op != 0xe7 && op != 0xb6
                        && op != 0xb7 && op != 0xb9 && op != 0xba && op != 0xfc)
                    {
                        /* Preserve exit state and ordering at other commands.
                         * In particular, the loader patches the preceding BB
                         * texture command, so never fold a bind across it. */
                        PropertyTexture(&out, &livetexture, &originaltexture);
                        livetexture.textureword0 = 0;
                    }
                }
                Append(&out, data + pc, 8); continue;
            }
            while (first < end)
            {
                const ModelSourceFace *face = &source->faces[first];
                DWORD oldcull = face->state.geometrymode & 0x3000u;
                DWORD cull = oldcull, mode = face->state.othermode;
                DWORD stop = first + 1;
                unsigned char triangles[4][3];
                int n = 0, type = (face->state.othermodehigh & 0x00300000u) == 0 ? 1 : 4;
                while (stop < end && changed[stop] == changed[first]) { stop++; }
                if (changed[first])
                {
                    if (culling >= 0) { cull = culling == 1 ? 0x2000u : culling == 2 ? 0x1000u : 0; }
                    if (surface >= 0) { mode = PropertySurface(mode, surface, type); }
                }
                PropertyState(&out, active ? livecull : oldcull, cull,
                    active ? livemode : face->state.othermode, mode, type);
                if (wraplist)
                {
                    BgMaterial material = (changed[first] & 2)
                        ? PropertyWrap(face->material, wrapu, wrapv) : face->material;
                    PropertyTexture(&out, &livetexture, &material);
                }
                for (i = first; i < stop; i++) { Indices(data + pc, source->faces[i].slot, triangles[n++]); }
                Triangles(&out, triangles, n);
                savedcull = oldcull; savedmode = face->state.othermode;
                livecull = cull; livemode = mode; activetype = type; active = cull != savedcull || mode != savedmode;
                first = stop;
            }
        }
        previous = part->end;
    }
    Append(&out, data + previous, size - previous);
finish:
    if (out.failed) { *reasonout = "The edited model exceeds the supported size or available memory."; goto done; }
    *result = out.data; *resultsize = out.size; out.data = NULL; *reasonout = ""; ok = TRUE;
done:
    free(changed); free(out.data); return ok;
}
