#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "clouds.h"

static void Put32(unsigned char *p, DWORD v)
{ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void Float(unsigned char *p, float v)
{ DWORD bits; memcpy(&bits,&v,4); Put32(p,bits); }
static void Environment(unsigned char *p, DWORD id, BOOL enabled, float red, DWORD stride)
{
    Put32(p,id); Float(p+8,10); Float(p+12,3000);
    p += 28 + stride - 88;
    Put32(p-8,996); Put32(p-4,1000);
    p[0]=16; p[1]=48; p[2]=96; p[3]=enabled;
    Float(p+4,5000); Float(p+12,red); Float(p+16,199); Float(p+20,186); Float(p+48,25);
}
static void Metadata(DWORD stride)
{
    unsigned char data[5*104]={0},copy[sizeof(data)],rgb[3];
    DWORD sky=28+stride-88;
    RomFile rom={0}; RomClouds clouds; RomFog fog;
    rom.data=data; rom.size=5*stride; rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x454e5654,0,0,stride};
    rom.info.entries[1]=(RomManifestEntry){0x434d4150,0,rom.size,0};
    Environment(data,0xffffffffu,TRUE,10,stride);
    Environment(data+stride,209,TRUE,20,stride);
    Environment(data+2*stride,9,TRUE,30,stride);
    Environment(data+3*stride,21,FALSE,40,stride);
    memcpy(copy,data,sizeof(data));
    assert(RomGetLevelClouds(&rom,9,&clouds) && clouds.enabled);
    assert(clouds.textureid==0x08b4 && clouds.height==5000 && clouds.color[0]==30 && clouds.horizonoffset==25);
    assert(RomGetLevelEnvironment(&rom,9,rgb,&fog) && !fog.enabled);
    assert(fog.nearclip==10 && fog.farclip==3000 && fog.start==996 && fog.end==1000);
    assert(rgb[0]==16 && rgb[1]==48 && rgb[2]==96); /* Clouds independent of fog. */
    assert(RomGetLevelClouds(&rom,21,&clouds) && !clouds.enabled);
    assert(RomGetLevelClouds(&rom,99,&clouds) && clouds.color[0]==10);
    assert(!memcmp(copy,data,sizeof(data))); /* Reading never changes the ROM. */
    Put32(data+2*stride,10);
    assert(RomGetLevelClouds(&rom,9,&clouds) && clouds.color[0]==20); /* MP fallback. */
    data[stride+sky+9]=1;
    assert(RomGetLevelClouds(&rom,9,&clouds) && clouds.textureid==0x05e4);
    data[stride+sky+9]=2;
    assert(RomGetLevelClouds(&rom,9,&clouds) && clouds.textureid==0x05e5);
    data[stride+sky+9]=3;
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    data[stride+sky+9]=0; Float(data+stride+sky+4,NAN);
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    Float(data+stride+sky+4,5000); Float(data+stride+sky+12,256);
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    Float(data+stride+sky+12,20); Float(data+stride+sky+48,INFINITY);
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    Float(data+stride+sky+48,25);
    rom.info.entries[1].romend=rom.size-1;
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled); /* Truncated terminator. */
    rom.info.entries[1].romend=rom.size; Put32(data+4*stride,99);
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    Put32(data+4*stride,0); rom.info.entries[0].flags=100;
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    rom.info.entrycount=0;
    assert(!RomGetLevelClouds(&rom,9,&clouds) && !clouds.enabled);
    puts("PASS: per-level clouds, fog independence, solo/MP/default precedence, sky image bank and malformed metadata.");
}
static void ProjectionAndColor(void)
{
    RomClouds clouds={TRUE,0x08b4,5000,{255,128,0},0};
    double eye[3]={0},up[3]={0,1,0},ray[3]={0,0.25,-1};
    float background[3]={16/255.0f,48/255.0f,96/255.0f};
    TexPixel pixels[]={{0,0,0,0},{255,255,255,0},{128,128,128,40}};
    CloudSample sample=CloudsSample(&clouds,eye,up,0,64,32),later;
    assert(sample.opacity==1 && sample.s==0 && sample.t==0);
    later=CloudsSample(&clouds,eye,up,32.0/60,64,32);
    assert(fabs(later.t-1.0/32)<1e-6); /* One texel per 32 game ticks. */
    later=CloudsSample(&clouds,eye,up,32.0*32/60,64,32);
    assert(fabs(later.t)<1e-6); /* Continuous texture repeat. */
    eye[0]=640; sample=CloudsSample(&clouds,eye,up,0,64,32);
    assert(fabs(sample.s-1.0/32)<1e-6); /* Camera translation moves the plane. */
    eye[0]=0; sample=CloudsSample(&clouds,eye,ray,0,64,32);
    assert(sample.opacity==0.5f && sample.t<0);
    ray[1]=0; sample=CloudsSample(&clouds,eye,ray,0,64,32);
    assert(sample.opacity==0 && isfinite(sample.t));
    ray[1]=-1; sample=CloudsSample(&clouds,eye,ray,0,64,32);
    assert(sample.opacity==0 && isfinite(sample.t));
    eye[1]=50000; ray[1]=0.25; sample=CloudsSample(&clouds,eye,ray,0,64,32);
    assert(sample.opacity==0.5f && isfinite(sample.t));
    clouds.enabled=FALSE; assert(CloudsSample(&clouds,eye,up,0,64,32).opacity==0);
    clouds.enabled=TRUE; assert(CloudsSample(&clouds,eye,up,0,0,32).opacity==0);
    CloudsTint(pixels,3,&clouds,background);
    assert(pixels[0].r==16 && pixels[0].g==48 && pixels[0].b==96 && pixels[0].a==255);
    assert(pixels[1].r==255 && pixels[1].g==152 && pixels[1].b==96 && pixels[1].a==255);
    assert(pixels[2].r>pixels[0].r && pixels[2].r<pixels[1].r && pixels[2].a==255);
    puts("PASS: cloud-plane UVs, drift, horizon fade, camera movement, above-sky flight and native sky tint.");
}
int main(void) { Metadata(104); Metadata(88); ProjectionAndColor(); return 0; }
