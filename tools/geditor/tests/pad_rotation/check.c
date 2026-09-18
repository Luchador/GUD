#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "setupload.h"
static const char *why="";
static DWORD Get(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Bits(float f) { union {float f;DWORD u;}v={.f=f};return v.u; }
static void Save(const char *path,const void *data,DWORD size)
{ FILE *f=fopen(path,"wb");assert(f && fwrite(data,1,size,f)==size);assert(!fclose(f)); }
static void Native(const unsigned char *data,DWORD root,const unsigned char *before)
{
    const unsigned char *p=data+Get(data+root*4);
    assert(!memcmp(p,before,12)); /* position */
    assert(Get(p+12)==0 && Get(p+16)==Bits(1) && Get(p+20)==0);
    assert(Get(p+24)==0xbf800002 && Get(p+28)==0 && Get(p+32)==0);
    assert(Get(p+40)==Get(before+40)); /* editor marker */
    if(root==7) assert(!memcmp(p+44,before+44,24)); /* placement bounds */
}
static void Directions(const SetupFile *s,SetupPadRef ref)
{
    const SetupPad *p=ref.bound?&s->boundpads[ref.index].pad:&s->pads[ref.index];
    DWORD at=Get(s->data+(ref.bound?28:24))+ref.index*(ref.bound?68:44);
    for(int i=0;i<3;i++)
    {
        assert(isfinite(p->up[i]) && isfinite(p->look[i]));
        assert(p->up[i]==0 || fabsf(p->up[i])>=1e-12f);
        assert(p->look[i]==0 || fabsf(p->look[i])>=1e-12f);
        assert(Bits(p->up[i])==Get(s->data+at+12+i*4));
        assert(Bits(p->look[i])==Get(s->data+at+24+i*4));
    }
}
int main(int argc,char **argv)
{
    unsigned char original[448]={0},snapshot[448],*packed=NULL,*again=NULL;
    DWORD size,againSize;SetupFile s={0},loaded={0};char path[1024];assert(argc==2);
    Put(original+24,64);Put(original+28,160);
    for(int r=0;r<2;r++)
    {
        unsigned char *p=original+(r?160:64);
        Put(p,Bits(100));Put(p+4,Bits(200));Put(p+8,Bits(300));
        /* Exact orientation words from Depot lamp 152, bound pad 60. */
        const DWORD direction[]={0x18fcd544,0x3f800000,0x80040000,0xbf800002,0x17cf38a4,0x248d3002};
        for(int i=0;i<6;i++)Put(p+12+i*4,direction[i]);
        Put(p+36,384);Put(p+40,0x47455050);
        if(r)for(int i=0;i<6;i++)Put(p+44+i*4,Bits((float)i-3));
    }
    memcpy(original+384,"p745a",6);memcpy(snapshot,original,sizeof(original));
    assert(SetupCompactNative(original,sizeof(original),&packed,&size,&why));
    assert(!memcmp(original,snapshot,sizeof(original)));
    Native(packed,6,original+64);Native(packed,7,original+160);
    assert(SetupCompactNative(packed,size,&again,&againSize,&why));
    assert(size==againSize && !memcmp(packed,again,size));free(again);free(packed);
    snprintf(path,sizeof(path),"%s/setup/UsetuprotateZ.set",argv[1]);Save(path,original,sizeof(original));
    assert(SetupLoadProjectFile(argv[1],"UsetuprotateZ",&s,&why));
    assert(s.padcount==1 && s.boundpadcount==1);
    Native(s.data,6,original+64);Native(s.data,7,original+160);
    /* Every axis, positive/negative cardinal angles, repeated native writes. */
    for(int bound=0;bound<2;bound++)
    {
        SetupPadRef ref={.bound=bound,.index=0};Directions(&s,ref);
        for(int i=0;i<1000;i++)
        {
            Rotation r;BOOL changed;RotationAxis(&r,i%3,(i%2?1:-1)*90);
            assert(SetupFileRotatePad(&s,&ref,&r,&changed,&why));Directions(&s,ref);
        }
        /* A small intentional tilt remains intact. */
        Rotation r;BOOL changed;RotationAxis(&r,2,.00001);
        assert(SetupFileRotatePad(&s,&ref,&r,&changed,&why) && changed);Directions(&s,ref);
    }
    /* Compaction also synchronizes the live cache when old values are present. */
    s.pads[0].up[0]=1e-24f;Put(s.data+Get(s.data+24)+12,Bits(s.pads[0].up[0]));
    assert(SetupFileCompact(&s,&why));Directions(&s,(SetupPadRef){.index=0});
    assert(SetupSaveProjectFile(argv[1],&s,&why));
    assert(SetupLoadProjectFile(argv[1],s.name,&loaded,&why));
    assert(s.size==loaded.size && !memcmp(s.data,loaded.data,s.size));
    SetupFileFree(&s);SetupFileFree(&loaded);
    puts("PASS: Depot 152 orientation repair, source preservation, ordinary/bound pads, repeated rotation, live cache, save/reload, and idempotence.");
}
