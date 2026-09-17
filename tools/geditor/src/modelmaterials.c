#include "modelmaterials.h"
#include "modelload.h"
#include "texload.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATERIAL_LIMIT 4096u
#define MODEL_LIMIT 0x00fffff0u
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p, DWORD n)
{ p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
static DWORD Hash(const unsigned char *p, DWORD size)
{ DWORD h=2166136261u; while (size--) h=(h^*p++)*16777619u; return h; }

void ModelMaterialsFree(ModelMaterials *m)
{ free(m->slots); free(m->faces); memset(m,0,sizeof(*m)); }

BOOL ModelMaterialsCopy(ModelMaterials *out, const ModelMaterials *in, const char **why)
{
    memset(out,0,sizeof(*out));
    if (!in->count) return TRUE;
    out->slots=malloc((size_t)in->count*sizeof(*out->slots));
    out->faces=malloc((size_t)(in->facecount?in->facecount:1)*sizeof(*out->faces));
    if (!out->slots || !out->faces)
    { ModelMaterialsFree(out); *why="Out of memory retaining material slots."; return FALSE; }
    memcpy(out->slots,in->slots,(size_t)in->count*sizeof(*out->slots));
    memcpy(out->faces,in->faces,(size_t)in->facecount*sizeof(*out->faces));
    out->count=in->count; out->facecount=in->facecount; return TRUE;
}

DWORD ModelMaterialsNativeSize(const unsigned char *data, DWORD size)
{
    if (size>=16 && !memcmp(data+size-16,"GMS1",4))
    {
        DWORD native=Read32(data+size-12), bytes=Read32(data+size-8);
        if (native<=size-16 && bytes<=size-16-native && size-16-native-bytes<16)
            return native;
    }
    return size;
}

BOOL ModelMaterialsRead(const unsigned char *data, DWORD size, DWORD faces,
    ModelMaterials *m, const char **why)
{
    DWORD native, bytes, count, i, cursor;
    const unsigned char *p;
    memset(m,0,sizeof(*m));
    if (size<16 || memcmp(data+size-16,"GMS1",4)) return TRUE;
    *why="The model's material slots are damaged or do not match its geometry.";
    native=ModelMaterialsNativeSize(data,size);
    if (native==size) return FALSE;
    bytes=Read32(data+size-8); p=data+native;
    if (bytes<8 || Hash(p,bytes)!=Read32(data+size-4)) return FALSE;
    count=Read32(p);
    if (!count || count>MATERIAL_LIMIT || Read32(p+4)!=faces
        || count>(bytes-8)/132 || faces>(bytes-8-count*132)/28
        || bytes!=8+count*132+faces*28) return FALSE;
    m->slots=calloc(count,sizeof(*m->slots));
    m->faces=calloc(faces?faces:1,sizeof(*m->faces));
    if (!m->slots || !m->faces)
    { *why="Out of memory reading material slots."; goto fail; }
    m->count=count; m->facecount=faces; cursor=8;
    for (i=0;i<count;i++,cursor+=132)
    {
        if (!memchr(p+cursor,0,128) || Read32(p+cursor+128)>BG_TEX_NONE) goto fail;
        memcpy(m->slots[i].name,p+cursor,128); m->slots[i].texture=Read32(p+cursor+128);
    }
    for (i=0;i<faces;i++,cursor+=28)
    {
        DWORD k;
        m->faces[i].slot=Read32(p+cursor);
        if (m->faces[i].slot>=count) goto fail;
        for (k=0;k<6;k++)
        {
            DWORD word=Read32(p+cursor+4+k*4);
            memcpy(&m->faces[i].uv[k],&word,4);
            if (!isfinite(m->faces[i].uv[k])) goto fail;
        }
    }
    *why=""; return TRUE;
fail:
    ModelMaterialsFree(m); return FALSE;
}

BOOL ModelMaterialsAttach(unsigned char **data, DWORD *size,
    const ModelMaterials *m, const char **why)
{
    DWORD native=ModelMaterialsNativeSize(*data,*size), bytes, total, i, cursor;
    unsigned char *out, *p;
    if (!m->count) return TRUE;
    *why="The material slots exceed the model format limits.";
    if (m->count>MATERIAL_LIMIT || m->facecount>MODEL_LIMIT/28) return FALSE;
    bytes=8+m->count*132+m->facecount*28;
    if (native>MODEL_LIMIT-31 || bytes>MODEL_LIMIT-native-31) return FALSE;
    total=(native+bytes+31)&~15u;
    out=calloc(total,1);
    if (!out) { *why="Out of memory retaining material slots."; return FALSE; }
    memcpy(out,*data,native); p=out+native;
    Write32(p,m->count); Write32(p+4,m->facecount); cursor=8;
    for (i=0;i<m->count;i++,cursor+=132)
    {
        if (!memchr(m->slots[i].name,0,128) || m->slots[i].texture>BG_TEX_NONE) goto invalid;
        memcpy(p+cursor,m->slots[i].name,128); Write32(p+cursor+128,m->slots[i].texture);
    }
    for (i=0;i<m->facecount;i++,cursor+=28)
    {
        DWORD k;
        if (m->faces[i].slot>=m->count) goto invalid;
        Write32(p+cursor,m->faces[i].slot);
        for (k=0;k<6;k++)
        {
            DWORD word;
            if (!isfinite(m->faces[i].uv[k])) goto invalid;
            memcpy(&word,&m->faces[i].uv[k],4); Write32(p+cursor+4+k*4,word);
        }
    }
    memcpy(out+total-16,"GMS1",4); Write32(out+total-12,native);
    Write32(out+total-8,bytes); Write32(out+total-4,Hash(p,bytes));
    free(*data); *data=out; *size=total; *why=""; return TRUE;
invalid:
    free(out); *why="A material slot has invalid names, indices or UV coordinates."; return FALSE;
}

BOOL ModelMaterialsEnsure(ModelSource *source, const char *project, const char **why)
{
    ModelMaterials *m=&source->materials;
    DWORD face;
    if (m->count) return m->facecount==source->count;
    m->slots=calloc(source->count?source->count:1,sizeof(*m->slots));
    m->faces=calloc(source->count?source->count:1,sizeof(*m->faces));
    if (!m->slots || !m->faces)
    { ModelMaterialsFree(m); *why="Out of memory identifying model materials."; return FALSE; }
    m->facecount=source->count;
    for (face=0;face<source->count;face++)
    {
        DWORD previous, slot, k;
        int width=1,height=1;
        unsigned int texture=BG_TEX_ID(source->tags[face]);
        for (previous=0;previous<face;previous++)
        {
            if (source->faces[previous].list==source->faces[face].list
                && source->flags[previous]==source->flags[face]
                && BgMaterialEqual(&source->faces[previous].material,&source->faces[face].material)) break;
        }
        if (previous<face) slot=m->faces[previous].slot;
        else
        {
            if (m->count==MATERIAL_LIMIT)
            { ModelMaterialsFree(m); *why="The model has too many material slots."; return FALSE; }
            slot=m->count++; m->slots[slot].texture=texture;
            snprintf(m->slots[slot].name,sizeof(m->slots[slot].name),"Material %lu (part %lu)",
                (unsigned long)slot+1,(unsigned long)source->faces[face].list+1);
        }
        m->faces[face].slot=slot;
        if (texture!=BG_TEX_NONE && !TexGetProjectImageSize(project,texture,&width,&height))
        { ModelMaterialsFree(m); *why="A model texture is missing from the project."; return FALSE; }
        for (k=0;k<3;k++)
        {
            m->faces[face].uv[k*2]=source->vertices[face*3+k].s/width;
            m->faces[face].uv[k*2+1]=source->vertices[face*3+k].t/height;
        }
    }
    return TRUE;
}

void ModelMaterialsMatch(ModelMaterials *m, const ModelMaterials *previous)
{
    DWORD i,j;
    for (i=0;i<m->count;i++)
    {
        DWORD match=0, matches=0, names=0;
        for (j=0;j<m->count;j++) names+=!strcmp(m->slots[i].name,m->slots[j].name);
        for (j=0;j<previous->count;j++)
            if (!strcmp(m->slots[i].name,previous->slots[j].name)) { match=j; matches++; }
        if (names==1 && matches==1) m->slots[i].texture=previous->slots[match].texture;
    }
}
