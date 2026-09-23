/* Structural LOD edits. Keep the original prefix/identities, append independent
 * mesh data before the display lists, then write lists in native tree order.
 * Subsequent UV/material/topology edits require this same prefix/list layout. */
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelcompile.h"

#define LOD_NODE_LIMIT 512
#define LOD_BYTE_LIMIT 0x00fffff0u
typedef struct LodOutput { unsigned char *data; DWORD size, capacity; BOOL failed; } LodOutput;
typedef struct LodPart {
    DWORD node, nearbranch, farbranch, copy, data, vertices, points, links, base, count;
} LodPart;
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static DWORD Read16(const unsigned char *p) { return (DWORD)p[0]*256+p[1]; }
static void Write32(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static BOOL Span(DWORD at, DWORD count, DWORD stride, DWORD limit)
{ return at<=limit && count<=(limit-at)/stride; }
static BOOL Pointer(DWORD raw, DWORD bytes, DWORD limit)
{ return (raw>>24)==5 && (raw&0xffffffu)!=0 && Span(raw&0xffffffu,1,bytes,limit); }
static DWORD Store(LodOutput *out, const void *data, DWORD size, DWORD alignment)
{
    DWORD at=(out->size+alignment-1)&~(alignment-1), end;
    if (out->failed || at>LOD_BYTE_LIMIT || size>LOD_BYTE_LIMIT-at) { out->failed=TRUE;return 0; }
    end=at+size;
    if (end>out->capacity)
    {
        DWORD capacity=out->capacity?out->capacity:4096;
        while (capacity<end) capacity=capacity>LOD_BYTE_LIMIT/2?LOD_BYTE_LIMIT:capacity*2;
        unsigned char *grown=realloc(out->data,capacity);
        if (!grown) { out->failed=TRUE;return 0; }
        out->data=grown;out->capacity=capacity;
    }
    memset(out->data+out->size,0,end-out->size);
    if (data && size) memcpy(out->data+at,data,size);
    out->size=end;return at;
}
static BOOL Tree(const unsigned char *data, DWORD limit, DWORD root, DWORD nodes[LOD_NODE_LIMIT], DWORD *count)
{
    DWORD stack[LOD_NODE_LIMIT], pending=0;
    *count=0;stack[pending++]=root;
    while (pending)
    {
        DWORD node=stack[--pending];
        if (!node || !Span(node,1,24,limit) || *count==LOD_NODE_LIMIT) return FALSE;
        for (DWORD i=0;i<*count;i++) if (nodes[i]==node) return FALSE;
        nodes[(*count)++]=node;
        for (DWORD at=8;at<=20;at+=4)
        {
            DWORD raw=Read32(data+node+at);
            if (raw && !Pointer(raw,24,limit)) return FALSE;
        }
        DWORD next=Read32(data+node+12)&0xffffffu, child=Read32(data+node+20)&0xffffffu;
        if (pending+!!next+!!child>LOD_NODE_LIMIT) return FALSE;
        if (next) stack[pending++]=next;
        if (child) stack[pending++]=child;
    }
    return TRUE;
}
static LodPart *FindPart(LodPart *parts, DWORD count, DWORD node)
{ for (DWORD i=0;i<count;i++) if (parts[i].node==node) return parts+i; return NULL; }
static void SetPointer(LodOutput *out,DWORD at,DWORD offset)
{ Write32(out->data+at,offset?0x05000000u|offset:0); }
static void Redirect(LodOutput *out,DWORD at,DWORD old,DWORD replacement)
{ if (Read32(out->data+at)==(0x05000000u|old)) SetPointer(out,at,replacement); }

static BOOL CloneMesh(LodOutput *out,const unsigned char *data,DWORD limit,LodPart *part,const char **why)
{
    DWORD node=part->node, type=Read16(data+node)&255, raw=Read32(data+node+4), ro=raw&0xffffffu;
    DWORD bytes=type==0x18?32:20, vertexfield=type==0x18?8:12;
    *why="Only leaf display-list meshes can be separated into LODs.";
    if ((type!=4 && type!=0x18) || Read32(data+node+20) || !Pointer(raw,bytes,limit)) return FALSE;
    raw=Read32(data+ro+vertexfield);part->base=raw&0xffffffu;
    part->count=Read16(data+ro+vertexfield+4);
    *why="A shared part's vertex or collision data is outside its native model data.";
    if (!part->count || part->count>32767 || !Pointer(raw,part->count*16,limit)) return FALSE;
    part->nearbranch=Store(out,NULL,40,4);part->farbranch=Store(out,NULL,40,4);
    part->copy=Store(out,data+node,24,4);part->data=Store(out,data+ro,bytes,4);
    part->vertices=Store(out,data+part->base,part->count*16,16);
    if (out->failed) return FALSE;
    SetPointer(out,part->copy+4,part->data);
    SetPointer(out,part->data+vertexfield,part->vertices);
    if (type==0x18)
    {
        DWORD points=Read16(data+ro+14), pointptr=Read32(data+ro+16), linkptr=Read32(data+ro+20);
        if ((points && !Pointer(pointptr,points*16,limit)) || (points && !linkptr)
            || (linkptr && !Pointer(linkptr,part->count*2,limit))) return FALSE;
        if (points)
        {
            part->points=Store(out,data+(pointptr&0xffffffu),points*16,16);
            if (out->failed) return FALSE;
            SetPointer(out,part->data+16,part->points);
        }
        if (linkptr)
        {
            part->links=Store(out,data+(linkptr&0xffffffu),part->count*2,2);
            if (out->failed) return FALSE;
            SetPointer(out,part->data+20,part->links);
        }
    }
    return TRUE;
}

static BOOL LinkBranches(LodOutput *out,LodPart *part,float split,DWORD limit,const char **why)
{
    DWORD node=part->node, parent=Read32(out->data+node+8)&0xffffffu;
    DWORD prev=Read32(out->data+node+16)&0xffffffu, next=Read32(out->data+node+12)&0xffffffu;
    *why="A shared mesh has unsupported parent or sibling links.";
    if (!parent || !Span(parent,1,24,limit)) return FALSE;
    DWORD type=Read16(out->data+parent)&255, ro=Read32(out->data+parent+4)&0xffffffu;
    /* These parent records have additional native child references. Keeping
     * them synchronized is essential when the game restores switches/BSPs. */
    DWORD refs[2], count=0;
    switch (type)
    {
    case 1: case 0x14: refs[count++]=4;break;
    case 2: case 3: refs[count++]=20;break;
    case 8: refs[count++]=8;break;
    case 9: refs[count++]=24;refs[count++]=28;break;
    case 0x12: refs[count++]=0;break;
    case 0x0a: case 0x15: break;
    default: return FALSE;
    }
    if (count && !Pointer(Read32(out->data+parent+4),refs[count-1]+4,limit)) return FALSE;
    if (prev)
    {
        if (!Span(prev,1,24,out->size) || Read32(out->data+prev+12)!=(0x05000000u|node)) return FALSE;
        SetPointer(out,prev+12,part->nearbranch);
    }
    else if (Read32(out->data+parent+20)!=(0x05000000u|node)) return FALSE;
    if (next)
    {
        if (!Span(next,1,24,limit) || Read32(out->data+next+16)!=(0x05000000u|node)) return FALSE;
        SetPointer(out,next+16,part->farbranch);
    }
    Redirect(out,parent+20,node,part->nearbranch);
    for (DWORD i=0;i<count;i++) Redirect(out,ro+refs[i],node,part->nearbranch);
    DWORD splitbits, maximum;float farbranch=FLT_MAX;
    memcpy(&splitbits,&split,4);memcpy(&maximum,&farbranch,4);
    for (int low=0;low<2;low++)
    {
        DWORD at=low?part->farbranch:part->nearbranch, child=low?part->copy:node;
        out->data[at+1]=8;
        SetPointer(out,at+4,at+24);SetPointer(out,at+8,parent);
        SetPointer(out,at+12,low?next:part->farbranch);SetPointer(out,at+16,low?part->nearbranch:prev);
        SetPointer(out,at+20,child);
        Write32(out->data+at+24,low?splitbits:0);
        Write32(out->data+at+28,low?maximum:splitbits);
        SetPointer(out,at+32,child);
        SetPointer(out,child+8,at);SetPointer(out,child+12,0);SetPointer(out,child+16,0);
    }
    return TRUE;
}

BOOL ModelCompileSeparateLods(const unsigned char *data,DWORD size,const ModelSource *source,
    unsigned char **result,DWORD *resultsize,DWORD *separated,const char **why)
{
    LodOutput out={0};LodPart *parts=NULL;
    ModelMaterials materials={0};ModelSource check={0};
    DWORD nodes[LOD_NODE_LIMIT], nodecount=0, partcount=0, shared=0, *slots=NULL;
    BOOL ok=FALSE;
    *result=NULL;*resultsize=0;*separated=0;
    *why="The model has invalid LOD source data.";
    if (!data || !source || !source->listcount || source->materials.facecount!=source->count) goto done;
    size=ModelMaterialsNativeSize(data,size);
    DWORD limit=source->lists[0].offset;
    if (!source->haslods || !isfinite(source->lodsplit) || source->lodsplit<=0)
    { ok=TRUE;*why="";goto done; }
    if (limit>size || !Tree(data,limit,source->root,nodes,&nodecount)) goto done;
    parts=calloc(source->listcount,sizeof(*parts));
    if (!parts) goto memory;
    for (DWORD face=0;face<source->count;face++)
    {
        const ModelSourceFace *f=&source->faces[face];
        if (!f->closest || !f->farthest) continue;
        DWORD node=source->lists[f->list].node;
        if (!FindPart(parts,partcount,node)) parts[partcount++].node=node;
        shared++;
    }
    if (!shared) { ok=TRUE;*why="";goto done; }
    if (nodecount+partcount*3>LOD_NODE_LIMIT)
    { *why="Separating these parts would exceed the model node limit.";goto done; }
    Store(&out,data,limit,1);
    for (DWORD i=0;i<partcount;i++)
        if (out.failed || !CloneMesh(&out,data,limit,parts+i,why)) goto done;
    for (DWORD i=0;i<partcount;i++)
    {
        if (!LinkBranches(&out,parts+i,source->lodsplit,limit,why)) goto done;
        /* Original cross-node blood-stain links remain valid. Redirect links
         * between copied parts to their independent low-LOD counterparts. */
        if (parts[i].points)
        {
            DWORD n=Read16(out.data+parts[i].data+14);
            for (DWORD p=0;p<n;p++)
            {
                DWORD at=parts[i].points+p*16+8;
                LodPart *other=FindPart(parts,partcount,Read32(out.data+at)&0xffffffu);
                if (other) SetPointer(&out,at,other->copy);
            }
        }
    }
    if (!Tree(out.data,out.size,source->root,nodes,&nodecount))
    { *why="Separating the LODs produced invalid model links.";goto done; }
    /* Independent material slots keep later image assignments local to the
     * low LOD even if its source slot is also used by an unrelated part. */
    materials.slots=calloc((size_t)source->materials.count*2,sizeof(*materials.slots));
    materials.faces=calloc((size_t)source->count+shared,sizeof(*materials.faces));
    slots=malloc((size_t)source->materials.count*sizeof(*slots));
    if (!materials.slots || !materials.faces || !slots) goto memory;
    materials.count=source->materials.count;materials.facecount=source->count+shared;
    memcpy(materials.slots,source->materials.slots,(size_t)materials.count*sizeof(*materials.slots));
    memset(slots,0xff,(size_t)source->materials.count*sizeof(*slots));
    DWORD facecursor=0;
    for (DWORD n=0;n<nodecount;n++)
    {
        DWORD node=nodes[n];LodPart *copy=NULL;
        for (DWORD i=0;i<partcount;i++) if (parts[i].copy==node) { copy=parts+i;break; }
        for (DWORD list=0;list<source->listcount;list++)
        {
            const ModelSourceList *part=source->lists+list;
            if (part->node!=(copy?copy->node:node)) continue;
            if (part->end<part->offset || !Span(part->offset,part->end-part->offset,1,size)) goto invalid;
            DWORD at=Store(&out,data+part->offset,part->end-part->offset,8);
            if (out.failed) goto memory;
            DWORD pointer=part->pointer;
            if (copy)
            {
                DWORD ro=Read32(data+copy->node+4)&0xffffffu;
                if (pointer<ro || pointer-ro>4 || part->preserve) goto invalid;
                pointer=copy->data+pointer-ro;
                for (DWORD pc=at;pc<out.size;pc+=8) if (out.data[pc]==4)
                {
                    DWORD raw=Read32(out.data+pc+4), offset=raw&0xffffffu;
                    DWORD count=(out.data[pc+1]>>4)+1;
                    if ((raw>>24)==5)
                    {
                        if (offset<copy->base || !Span(offset-copy->base,count,16,copy->count*16)) goto external;
                        SetPointer(&out,pc+4,copy->vertices+offset-copy->base);
                    }
                    else if ((raw>>24)!=4 || !Span(offset,count,16,copy->count*16)) goto external;
                }
            }
            SetPointer(&out,pointer,at);
            for (DWORD f=0;f<source->count;f++) if (source->faces[f].list==list)
            {
                if (facecursor>=materials.facecount) goto invalid;
                ModelMaterialFace face=source->materials.faces[f];
                if (copy)
                {
                    if (slots[face.slot]==0xffffffffu)
                    {
                        DWORD slot=slots[face.slot]=materials.count++;
                        materials.slots[slot]=source->materials.slots[face.slot];
                        snprintf(materials.slots[slot].name,sizeof(materials.slots[slot].name),
                            "Low LOD %lu: %.96s",(unsigned long)slot+1,source->materials.slots[face.slot].name);
                    }
                    face.slot=slots[face.slot];
                }
                materials.faces[facecursor++]=face;
            }
        }
    }
    if (facecursor!=materials.facecount) goto invalid;
    if (!ModelMaterialsAttach(&out.data,&out.size,&materials,why)
        || !ModelReadSource(out.data,out.size,&check,why)) goto done;
    if (check.count!=materials.facecount) goto invalid;
    for (DWORD f=0;f<check.count;f++) if (check.faces[f].closest && check.faces[f].farthest) goto invalid;
    *result=out.data;*resultsize=out.size;*separated=shared;out.data=NULL;ok=TRUE;*why="";
    goto done;
external:
    *why="A shared part loads vertices outside its own buffer. Its LODs cannot yet be separated.";goto done;
invalid:
    *why="The shared parts could not be separated without changing the native model layout.";goto done;
memory:
    *why="Out of memory separating model LODs.";
done:
    if (out.failed) *why="The separated model exceeds memory or native size limits.";
    free(out.data);free(parts);free(slots);ModelMaterialsFree(&materials);ModelFreeSource(&check);return ok;
}
