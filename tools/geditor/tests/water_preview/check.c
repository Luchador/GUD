#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "water.h"
#include "environment.h"

static void Put32(unsigned char *p, DWORD v)
{ p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
static void Float(unsigned char *p, float v)
{ DWORD bits; memcpy(&bits,&v,4); Put32(p,bits); }
static void Metadata(DWORD stride)
{
    unsigned char data[3*112]={0},copy[sizeof(data)];
    RomFile rom={0}; RomWater water;
    rom.data=data; rom.size=3*stride; rom.info.entrycount=2;
    rom.info.entries[0]=(RomManifestEntry){0x454e5654,0,0,stride};
    rom.info.entries[1]=(RomManifestEntry){0x434d4150,0,rom.size,0};
    Put32(data,0xffffffffu);Put32(data+stride,23);
    unsigned char *sky=data+stride+(stride==104?44:28);
    sky[3]=sky[24]=1;sky[33]=1;Float(sky+28,-500);Float(sky+36,120);Float(sky+40,90);Float(sky+44,60);Float(sky+48,25);
    memcpy(copy,data,sizeof(data));
    assert(RomGetLevelWater(&rom,23,&water)&&water.enabled&&water.textureid==0x05e4);
    assert(water.height==-500&&water.color[0]==120&&water.color[2]==60&&water.horizonoffset==25);
    assert(RomGetLevelWater(&rom,99,&water)&&!water.enabled);
    assert(!memcmp(copy,data,sizeof(data)));
    EnvironmentTable table;const char *why="";
    assert(EnvironmentReadRom(&rom,&table,NULL,&why));
    EnvironmentPreviewWater(EnvironmentFind(&table,23),&water);
    assert(water.enabled&&water.height==-500&&water.textureid==0x05e4&&water.color[2]==60);
    sky[24]=0;assert(RomGetLevelWater(&rom,23,&water)&&!water.enabled);
    sky[24]=1;sky[3]=0;assert(RomGetLevelWater(&rom,23,&water)&&!water.enabled);
    sky[3]=1;sky[33]=3;assert(!RomGetLevelWater(&rom,23,&water)&&!water.enabled);
    sky[33]=2;assert(RomGetLevelWater(&rom,23,&water)&&water.textureid==0x05e5);
    Float(sky+28,NAN);assert(!RomGetLevelWater(&rom,23,&water)&&!water.enabled);
    Float(sky+28,-500);Float(sky+40,256);assert(!RomGetLevelWater(&rom,23,&water)&&!water.enabled);
    Float(sky+40,90);Float(sky+48,INFINITY);assert(!RomGetLevelWater(&rom,23,&water)&&!water.enabled);
}
static void Sampling(void)
{
    RomWater water={TRUE,0x05e4,-100,{255,128,0},0};
    double eye[3]={0},down[3]={0,-1,0},ray[3]={1,-.25,-1};float bg[3]={.1f,.2f,.3f};
    WaterSample a=WaterSamplePlane(&water,eye,down,bg,0,32,32),b;
    assert(a.q==1&&a.s==0&&a.t==0);
    assert(fabs(a.color[0]-1)<1e-6&&fabs(a.color[1]-(.2+.8*128/255))<1e-6&&fabs(a.color[2]-.3)<1e-6);
    b=WaterSamplePlane(&water,eye,down,bg,32.0/60,32,32);assert(fabs(b.t/b.q-1.0/32)<1e-6);
    eye[0]=32;b=WaterSamplePlane(&water,eye,down,bg,0,32,32);assert(fabs(b.s/b.q-1.0/32)<1e-6);eye[0]=0;
    a=WaterSamplePlane(&water,eye,ray,bg,0,32,32);
    assert(fabs(a.s/a.q-400.0/1024)<1e-6&&fabs(a.t/a.q+400.0/1024)<1e-6);
    ray[1]=0;a=WaterSamplePlane(&water,eye,ray,bg,0,32,32);
    assert(a.q>0&&isfinite(a.s/a.q)&&fabs(a.color[0]-.1)<1e-6);
    assert(hypot(a.s/a.q*1024,a.t/a.q*1024)<=300001);
    eye[1]=-101;assert(WaterSamplePlane(&water,eye,down,bg,0,32,32).q==0);
    eye[1]=0;water.enabled=FALSE;assert(WaterSamplePlane(&water,eye,down,bg,0,32,32).q==0);
    assert(fabs(WaterBlend(0)-128.0/255)<1e-6);
    assert(WaterBlend(3.1415901/2/2.4)>.999&&WaterBlend(3*3.1415901/2/2.4)<.0041);
    puts("PASS: water metadata/legacy records, Clouds gate, malformed values, world UVs, horizon, tint, animation and below-plane safety.");
}
int main(void) { Metadata(88);Metadata(104);Metadata(112);Sampling();return 0; }
