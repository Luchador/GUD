#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "modelanimation.h"
#include "modeledits.h"
#include "modelanimationdata.h"
#include "newprops.h"

static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static unsigned Read16(const unsigned char *p) { return (unsigned)p[0]<<8 | p[1]; }
static BOOL Bits(const unsigned char *data, DWORD size, unsigned long long bit,
    unsigned width, unsigned *value)
{
    unsigned out=0;
    if (width>16 || bit+(unsigned long long)width>(unsigned long long)size*8) return FALSE;
    for (unsigned i=0;i<width;i++,bit++) out=(out<<1)|((data[bit/8]>>(7-bit%8))&1);
    *value=out;return TRUE;
}

void ModelAnimationClose(ModelAnimationPreview *p)
{
    free(p->headers);free(p->frames);free(p->model);free(p->clips);
    ZeroMemory(p,sizeof(*p));
}

static BOOL CopyBank(const RomFile *rom, DWORD kind, unsigned char **bytes, DWORD *size)
{
    for (DWORD i=0;i<rom->info.entrycount;i++)
    {
        const RomManifestEntry *e=&rom->info.entries[i];
        if (e->kind!=kind) continue;
        if (e->romstart>=e->romend || e->romend>rom->size) return FALSE;
        *size=e->romend-e->romstart;*bytes=malloc(*size);
        if (!*bytes) return FALSE;
        memcpy(*bytes,rom->data+e->romstart,*size);return TRUE;
    }
    return FALSE;
}

BOOL ModelAnimationOpen(ModelAnimationPreview *p, const char *project,
    const char *name, const char **why)
{
    RomFile rom={0};char path[MAX_PATH];BOOL ok=FALSE;
    ZeroMemory(p,sizeof(*p));*why="";
    for (size_t i=0;i<sizeof(g_AnimatedModels)/sizeof(g_AnimatedModels[0]);i++)
        if (!strcmp(name,g_AnimatedModels[i].name)) { p->channels=g_AnimatedModels[i].channels;break; }
    if (NewPropsCharacterKind(name)==CUSTOM_CHARACTER_BODY) p->channels=45;
    if (!p->channels) return TRUE;
    int length=snprintf(path,sizeof(path),"%s\\base.z64",project);
    if (length<0 || length>=MAX_PATH) { *why="The animation source path is too long.";goto done; }
    if (!RomLoad(path,&rom,why)) goto done;
    *why="This base ROM does not contain a readable animation bank.";
    if (!CopyBank(&rom,0x414e4944u,&p->headers,&p->headersize)
        || !CopyBank(&rom,0x414e4946u,&p->frames,&p->framesize)) goto done;
    p->clips=calloc(sizeof(g_AnimationNames)/sizeof(g_AnimationNames[0]),sizeof(*p->clips));
    if (!p->clips) { *why="Out of memory loading animations.";goto done; }
    for (size_t i=0;i<sizeof(g_AnimationNames)/sizeof(g_AnimationNames[0]);i++)
    {
        ModelAnimationClip clip={0};DWORD bits;
        clip.name=g_AnimationNames[i].name;clip.header=g_AnimationNames[i].offset;
        if (p->headersize<20 || clip.header>p->headersize-20) continue;
        const unsigned char *header=p->headers+clip.header;
        clip.frames=Read16(header+4);clip.width=header[6];bits=Read16(header+14);
        if (!clip.frames || !clip.width || clip.width>16 || !bits || bits%8) continue;
        clip.channels=bits/clip.width;clip.framebytes=bits/8;clip.dataoffset=Read32(header);
        if (clip.channels!=(DWORD)p->channels || bits-clip.channels*clip.width>=8
            || clip.dataoffset>p->framesize
            || clip.frames>(p->framesize-clip.dataoffset)/clip.framebytes) continue;
        /* Aircraft have a shared rig, but separate plane/helicopter clips. */
        if (p->channels==3 && ((!strcmp(name,"PplaneZ")) != (!strcmp(clip.name,"plane_runway")))) continue;
        p->clips[p->count++]=clip;
    }
    if (p->count && !ModelEditsCopyNative(project,name,&p->model,&p->modelsize,why)) goto done;
    *why="";ok=TRUE;
done:
    RomFree(&rom);
    if (!ok) ModelAnimationClose(p);
    return ok;
}

/* The root stream's Y sample supplies crouching/falling height. X/Z travel
 * and heading stay in place so long walks and cutscenes remain inspectable. */
static BOOL Height(const ModelAnimationPreview *p, const ModelAnimationClip *clip,
    DWORD frame, float *height)
{
    const unsigned char *header=p->headers+clip->header;
    DWORD descriptors=Read32(header+8),stream=Read32(header+16);
    unsigned raw,width,base;unsigned long long bit;
    if (p->headersize<12 || descriptors>p->headersize-12 || stream>p->headersize) return FALSE;
    const unsigned char *desc=p->headers+descriptors+6;
    width=desc[2];base=Read16(desc+4);
    bit=(unsigned long long)Read16(header+12)*frame+Read16(desc);
    if (!Bits(p->headers+stream,p->headersize-stream,bit,width,&raw)) return FALSE;
    if (width && width<16 && (raw&(1u<<(width-1)))) raw|=0xffffu<<width;
    raw=(raw+base)&65535u;
    *height=raw>=32768 ? (float)((int)raw-65536) : (float)raw;
    return TRUE;
}

BOOL ModelAnimationReadFrame(const ModelAnimationPreview *p,
    const ModelAnimationClip *clip, double frame, unsigned short angles[45], float *height)
{
    if (!p || !clip || !p->headers || !p->frames || !clip->frames || !isfinite(frame)
        || !clip->width || clip->width>16 || !clip->channels || clip->channels>45
        || p->headersize<20 || clip->header>p->headersize-20
        || !clip->framebytes || clip->dataoffset>p->framesize
        || clip->frames>(p->framesize-clip->dataoffset)/clip->framebytes
        || clip->channels*clip->width>clip->framebytes*8u) return FALSE;
    frame=fmax(0.0,fmin(frame,clip->frames-1.0));
    DWORD a=(DWORD)frame,b=a+1<clip->frames?a+1:a;
    double blend=frame-a;float ya,yb,y0;
    for (DWORD c=0;c<clip->channels;c++)
    {
        unsigned va,vb;
        if (!Bits(p->frames,p->framesize,((unsigned long long)clip->dataoffset+a*clip->framebytes)*8+c*clip->width,clip->width,&va)
            || !Bits(p->frames,p->framesize,((unsigned long long)clip->dataoffset+b*clip->framebytes)*8+c*clip->width,clip->width,&vb)) return FALSE;
        va<<=16-clip->width;vb<<=16-clip->width;
        int delta=(int)vb-(int)va;
        if (delta>32767) delta-=65536;
        if (delta< -32768) delta+=65536;
        angles[c]=(unsigned short)((int)va+(int)lround(delta*blend));
    }
    if (!Height(p,clip,a,&ya) || !Height(p,clip,b,&yb) || !Height(p,clip,0,&y0)) return FALSE;
    *height=ya+(yb-ya)*(float)blend-y0;
    return TRUE;
}

BgVertex *ModelAnimationPose(const ModelAnimationPreview *p, DWORD index,
    double frame, DWORD expectedcount, const char **why)
{
    unsigned short angles[45]={0};float height;
    *why="The selected animation has invalid frame data.";
    if (index>=p->count || !ModelAnimationReadFrame(p,&p->clips[index],frame,angles,&height)) return NULL;
    return ModelLoadAnimationPose(p->model,p->modelsize,angles,p->channels,height,expectedcount,why);
}
