#include "propcompile.h"
#include "modelcompile.h"
#include "texload.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct PropOutput { unsigned char *data; DWORD size, capacity; BOOL failed; } PropOutput;
static void Put16(unsigned char *p,int n) { p[0]=(unsigned)n>>8; p[1]=n; }
static void Put32(unsigned char *p,DWORD n) { p[0]=n>>24; p[1]=n>>16; p[2]=n>>8; p[3]=n; }
static void PutFloat(unsigned char *p,float f) { DWORD n; memcpy(&n,&f,4); Put32(p,n); }
static void Append(PropOutput *out,const void *data,DWORD size)
{
    if (out->failed || size>0x1000000u-out->size) { out->failed=TRUE; return; }
    if (out->size+size>out->capacity)
    {
        DWORD next=out->capacity?out->capacity*2:4096;
        if (next<out->size+size) next=out->size+size;
        unsigned char *grown=realloc(out->data,next);
        if (!grown) { out->failed=TRUE; return; }
        out->data=grown; out->capacity=next;
    }
    memcpy(out->data+out->size,data,size); out->size+=size;
}
static void Command(PropOutput *out,DWORD a,DWORD b)
{ unsigned char cmd[8]; Put32(cmd,a); Put32(cmd+4,b); Append(out,cmd,8); }
static void Node(unsigned char *p,int type,DWORD data,DWORD parent,DWORD child)
{
    Put16(p,type); Put32(p+4,0x05000000u|data);
    Put32(p+8,parent?0x05000000u|parent:0); Put32(p+20,child?0x05000000u|child:0);
}

static void Material(PropOutput *out,unsigned short tag,BgRenderFlags flags,BOOL blend)
{
    BgMaterial material;
    DWORD cull=(flags&BG_RENDER_CULL_FRONT)?0x1000u:(flags&BG_RENDER_CULL_BACK)?0x2000u:0;
    BgMaterialInit(&material); BgMaterialSetTexture(&material,BG_TEX_ID(tag));
    BgMaterialSetWrap(&material,FALSE,(flags&BG_RENDER_CLAMP_S)?BG_TEXTURE_CLAMP:
        (flags&BG_RENDER_MIRROR_S)?BG_TEXTURE_MIRROR:BG_TEXTURE_REPEAT);
    BgMaterialSetWrap(&material,TRUE,(flags&BG_RENDER_CLAMP_T)?BG_TEXTURE_CLAMP:
        (flags&BG_RENDER_MIRROR_T)?BG_TEXTURE_MIRROR:BG_TEXTURE_REPEAT);
    Command(out,0xe7000000u,0);
    Command(out,0xba001402u,0x00100000u); /* two cycles, including AA-off fallback */
    Command(out,0xb9000002u,0); /* no alpha compare */
    if (blend)
    {
        /* PASS then source-alpha blending: no coverage-to-alpha substitution.
         * Fog must be disabled before G_VTX so it cannot overwrite vertex A. */
        Command(out,0xb900031du,0x0c1849d8u);
    }
    /* Opaque faces retain the renderer's room lighting, depth and AA mode.
     * Reasserting the AA bits here would undo the user's AA-off setting. */
    Command(out,0xb6000000u,0x000e3000u|(blend?0x00010000u:0));
    Command(out,0xb7000000u,0x00000205u|cull); /* Z buffer, shade, smooth shade */
    if (BG_TEX_ID(tag)!=BG_TEX_NONE) Command(out,material.textureword0,material.textureword1);
    Command(out,material.modeword0,material.modeword1);
    Command(out,material.combineword0,material.combineword1);
}

BOOL PropCompile(const BgVertex *vertices,const unsigned short *tags,
    const BgRenderFlags *flags,DWORD count,const char *projectdir,
    unsigned char **data,DWORD *size,float *radius,const char **why)
{
    PropOutput out={0};
    unsigned char header[176]={0}, *native=NULL;
    DWORD *indices=NULL,*hashes=NULL, slots=1, unique=0, face, corner, pass;
    float bounds[2][3]={{32767,32767,32767},{-32768,-32768,-32768}};
    float allbounds[2][3]={{32767,32767,32767},{-32768,-32768,-32768}};
    double radiussquared=0;
    BOOL opaque=FALSE,ok=FALSE;
    *data=NULL; *size=0; *radius=0;
    *why="A new prop must contain between 1 and 10000 triangles.";
    if (!vertices || !tags || !flags || !count || count>10000) return FALSE;
    while (slots<count*6) slots*=2;
    indices=malloc((size_t)count*3*sizeof(*indices)); hashes=calloc(slots,sizeof(*hashes));
    native=malloc((size_t)count*3*16);
    *why="Out of memory compiling the prop.";
    if (!indices || !hashes || !native) goto done;
    for (face=0;face<count;face++)
    {
        int width=1,height=1;
        BOOL blend=(flags[face]&BG_RENDER_BLEND)!=0;
        if (BG_TEX_ID(tags[face])!=BG_TEX_NONE
            && !TexGetProjectImageSize(projectdir,BG_TEX_ID(tags[face]),&width,&height))
        { *why="A model image is missing from the project."; goto done; }
        opaque|=!blend;
        for (corner=0;corner<3;corner++)
        {
            const BgVertex *v=&vertices[face*3+corner];
            unsigned char record[16]={0};
            double p[3]={round(v->x*1000.0),round(v->y*1000.0),round(v->z*1000.0)};
            double s=BG_TEX_ID(tags[face])==BG_TEX_NONE?0:round(v->s*32.0*width);
            double t=BG_TEX_ID(tags[face])==BG_TEX_NONE?0:round(v->t*32.0*height);
            DWORD hash;
            int axis;
            for (axis=0;axis<3;axis++)
            {
                if (!isfinite(p[axis]) || p[axis]<-32768 || p[axis]>32767)
                { *why="The prop exceeds native position limits (about 32 metres from its origin)."; goto done; }
                Put16(record+axis*2,(int)p[axis]);
                allbounds[0][axis]=fminf(allbounds[0][axis],(float)p[axis]);
                allbounds[1][axis]=fmaxf(allbounds[1][axis],(float)p[axis]);
                if (!blend)
                {
                    bounds[0][axis]=fminf(bounds[0][axis],(float)p[axis]);
                    bounds[1][axis]=fmaxf(bounds[1][axis],(float)p[axis]);
                }
            }
            radiussquared=fmax(radiussquared,p[0]*p[0]+p[1]*p[1]+p[2]*p[2]);
            if (!isfinite(s) || !isfinite(t) || s<-32768 || s>32767 || t<-32768 || t>32767)
            { *why="A prop UV exceeds the native signed 16-bit texture-coordinate range."; goto done; }
            Put16(record+8,(int)s); Put16(record+10,(int)t);
            record[12]=v->r; record[13]=v->g; record[14]=v->b; record[15]=v->a;
            hash=ModelDataHash(record,16)&(slots-1);
            while (hashes[hash] && memcmp(native+(hashes[hash]-1)*16,record,16)) hash=(hash+1)&(slots-1);
            if (!hashes[hash])
            { memcpy(native+unique*16,record,16); hashes[hash]=++unique; }
            indices[face*3+corner]=hashes[hash]-1;
        }
        /* Reject triangles collapsed by native position quantization. */
        {
            const unsigned char *a=native+indices[face*3]*16,*b=native+indices[face*3+1]*16,*c=native+indices[face*3+2]*16;
            double ab[3],ac[3]; int axis;
            for (axis=0;axis<3;axis++)
            {
                int av=(short)((a[axis*2]<<8)|a[axis*2+1]);
                ab[axis]=(short)((b[axis*2]<<8)|b[axis*2+1])-av;
                ac[axis]=(short)((c[axis*2]<<8)|c[axis*2+1])-av;
            }
            if (ab[0]*ac[1]==ab[1]*ac[0] && ab[0]*ac[2]==ab[2]*ac[0] && ab[1]*ac[2]==ab[2]*ac[1])
            { *why="A prop triangle becomes degenerate at native millimetre precision."; goto done; }
        }
    }
    *radius=(float)(sqrt(radiussquared)+1.0); /* Include translucent shafts in culling. */
    if (!opaque) memcpy(bounds,allbounds,sizeof(bounds));
    Put32(header,0x05000004u); /* single switch -> root group */
    Node(header+4,2,76,0,28); Node(header+28,10,104,4,52); Node(header+52,0x18,132,28,0);
    Put16(header+92,-1); Put16(header+94,-1); PutFloat(header+100,*radius);
    Put32(header+104,1);
    for (corner=0;corner<3;corner++)
    { PutFloat(header+108+corner*8,bounds[0][corner]); PutFloat(header+112+corner*8,bounds[1][corner]); }
    Put32(header+140,0x050000b0u); Put16(header+144,unique); Put16(header+156,4);
    Append(&out,header,sizeof(header)); Append(&out,native,unique*16);
    for (pass=0;pass<2;pass++)
    {
        DWORD cache[16],lastflags=0xffffffffu, lasttag=0xffffffffu, first=out.size;
        BOOL any=FALSE;
        memset(cache,0xff,sizeof(cache));
        for (face=0;face<count;face++)
        {
            unsigned char mapped[3];
            unsigned locked=0;
            if (!!(flags[face]&BG_RENDER_BLEND)!=(int)pass) continue;
            /* The renderer binds segment 3, but native model display lists
             * must load their own matrix before the first vertex command. */
            if (!any) Command(&out,0x01020040u,0x03000000u);
            any=TRUE;
            if (lasttag!=tags[face] || lastflags!=flags[face])
            {
                Material(&out,tags[face],flags[face],pass!=0);
                memset(cache,0xff,sizeof(cache)); lasttag=tags[face]; lastflags=flags[face];
            }
            for (corner=0;corner<3;corner++)
            {
                int slot;
                mapped[corner]=16;
                for (slot=0;slot<16;slot++) if (cache[slot]==indices[face*3+corner])
                { mapped[corner]=slot; locked|=1u<<slot; break; }
            }
            for (corner=0;corner<3;corner++) if (mapped[corner]==16)
            {
                int slot;
                for (slot=0;slot<16 && (locked&(1u<<slot));slot++) {}
                mapped[corner]=slot; locked|=1u<<slot; cache[slot]=indices[face*3+corner];
                Command(&out,0x04000010u|((DWORD)slot<<16),0x04000000u|cache[slot]*16);
            }
            Command(&out,0xbf000000u,((DWORD)mapped[0]*10<<16)|((DWORD)mapped[1]*10<<8)|((DWORD)mapped[2]*10));
        }
        /* Type-4 translucent rendering is gated by a non-NULL primary list,
         * even for a model that consists entirely of translucent faces. */
        if (any || !pass)
        {
            Command(&out,0xb8000000u,0);
            if (!out.failed) Put32(out.data+132+pass*4,0x05000000u|first);
        }
    }
    if (out.size&15) { unsigned char pad[16]={0}; Append(&out,pad,16-(out.size&15)); }
    if (out.failed) { *why="The compiled prop is too large or could not be allocated."; goto done; }
    *data=out.data; *size=out.size; out.data=NULL; *why=""; ok=TRUE;
done:
    free(out.data); free(native); free(indices); free(hashes); return ok;
}
