#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include "gltf.h"

/* Lit embedded resources have no external textures. */
BOOL TexGetProjectImageSize(const char *project, DWORD id, int *w, int *h)
{ (void)project; (void)id; (void)w; (void)h; return FALSE; }
static void Put(unsigned char *p, DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Near(float a, float b) { assert(fabsf(a-b)<0.0001f); }
static void Reject(SetupFile *setup)
{
    SetupMarker *markers=NULL;DWORD count=0;const char *why="";
    assert(!SetupFileBuildMarkers(setup,0.5f,&markers,&count,&why));
    assert(markers==NULL&&count==0&&why[0]);
}
static void Placements(void)
{
    unsigned char data[512]={0},original[512];
    SetupPad pads[3]={0};SetupFile setup={0};SetupMarker *m=NULL;DWORD count;const char *why="";
    setup.data=data;setup.size=sizeof(data);setup.pads=pads;setup.padcount=3;strcpy(setup.name,"UsetupsevbunkerZ");
    pads[1].pos[0]=10;pads[1].pos[1]=20;pads[1].pos[2]=30;pads[1].look[0]=1;
    pads[2].look[2]=-1;
    Put(data+8,40);Put(data+12,200);
    Put(data+40,0);Put(data+44,99);Put(data+48,1); /* Demo-only invalid pad ignored. */
    Put(data+52,0);Put(data+56,1);Put(data+60,0);
    Put(data+64,0);Put(data+68,2);Put(data+72,0);
    Put(data+76,6);Put(data+80,(DWORD)-12345);Put(data+84,2500);Put(data+88,9876);
    Put(data+116,6);Put(data+132,102944);Put(data+136,51472); /* pi/2 yaw, pi/4 pitch */
    Put(data+156,9);
    Put(data+200,46);Put(data+204,12345);Put(data+208,(DWORD)-2500);Put(data+216,102944);
    Put(data+228,0x4745442e); /* Deleted character, not an outro camera. */
    Put(data+256,48);
    memcpy(original,data,sizeof(data));
    assert(SetupFileBuildMarkers(&setup,0.5f,&m,&count,&why)&&count==4);
    assert(m[0].kind==SETUP_MARKER_SPAWN&&m[0].pad==1);
    Near(m[0].position[0],20);Near(m[0].position[1],40);Near(m[0].position[2],60);Near(m[0].look[0],1);
    assert(m[1].kind==SETUP_MARKER_INTRO);Near(m[1].position[0],-123.45f);Near(m[1].position[1],25);Near(m[1].look[2],-1);
    Near(m[2].look[0],sqrtf(0.5f));Near(m[2].look[1],sqrtf(0.5f));Near(m[2].up[0],-sqrtf(0.5f));
    assert(m[3].kind==SETUP_MARKER_OUTRO);Near(m[3].position[0],123.45f);Near(m[3].position[1],-25);Near(m[3].look[0],1);
    for(DWORD i=0;i<count;i++) { float dot=0,len=0;for(int a=0;a<3;a++){dot+=m[i].look[a]*m[i].up[a];len+=m[i].look[a]*m[i].look[a];}Near(dot,0);Near(len,1); }
    free(m);assert(!memcmp(data,original,sizeof(data))&&!setup.dirty);
    strcpy(setup.name,"Ump_setupsevbZ");assert(SetupFileBuildMarkers(&setup,0.25f,&m,&count,&why)&&count==5);
    assert(m[1].kind==SETUP_MARKER_SPAWN&&m[1].pad==2);Near(m[0].position[0],40);Near(m[2].position[0],-123.45f);free(m);
    Put(data+56,999);Reject(&setup);Put(data+56,1);
    Put(data+8,510);Reject(&setup);Put(data+8,40);
    Put(data+76,100);Reject(&setup);Put(data+76,6);
    setup.size=240;Reject(&setup);setup.size=512;
    Put(data+8,0);Put(data+12,0);assert(SetupFileBuildMarkers(&setup,1,&m,&count,&why)&&!count&&!m);
    puts("PASS: SP/MP spawns, demo filtering, camera signed coordinates/angles, orthonormal frames, tombstones, malformed records and unchanged source data.");
}
static unsigned char *Read(const char *path,DWORD *size)
{
    FILE *f=fopen(path,"rb");unsigned char *data;long n;assert(f&&!fseek(f,0,SEEK_END));n=ftell(f);assert(n>0&&!fseek(f,0,SEEK_SET));
    data=malloc(n);assert(data&&fread(data,1,n,f)==(size_t)n&&!fclose(f));*size=(DWORD)n;return data;
}
static void MultiplayerResource(const char *dir)
{
    char folder[MAX_PATH],path[MAX_PATH+64];unsigned char data[156]={0};
    SetupFile setup;const char *why="";FILE *file;
    snprintf(folder,sizeof(folder),"%s/setup",dir);assert(CreateDirectory(folder,NULL));
    Put(data+8,40);Put(data+40,9);Put(data+24,44);Put(data+28,88);
    snprintf(path,sizeof(path),"%s/Ump_setuprefZ.set",folder);
    file=fopen(path,"wb");assert(file&&fwrite(data,1,sizeof(data),file)==sizeof(data)&&!fclose(file));
    assert(SetupLoadProjectFile(dir,"UsetuprefZ",&setup,&why));
    assert(!strcmp(setup.name,"Ump_setuprefZ"));
    assert(SetupSaveProjectFile(dir,&setup,&why));SetupFileFree(&setup);
    snprintf(path,sizeof(path),"%s/UsetuprefZ.set",folder);
    file=fopen(path,"wb");assert(file);fputc(0,file);fclose(file);
    assert(!SetupLoadProjectFile(dir,"UsetuprefZ",&setup,&why)); /* Do not mask corrupt solo assets. */
    snprintf(path,sizeof(path),"%s/Ump_setupdamZ.set",folder);
    file=fopen(path,"wb");assert(file&&fwrite(data,1,sizeof(data),file)==sizeof(data)&&!fclose(file));
    assert(!SetupLoadProjectFile(dir,"UsetupdamZ",&setup,&why)); /* Do not substitute an MP mission. */
    puts("PASS: multiplayer-only resource resolution and saving to the correct asset; corrupt/missing solo missions still fail.");
}
static void Model(const char *path,const char *node,DWORD expected,int color)
{
    DWORD size,count;const char *why="";unsigned char *data=Read(path,&size);BgVertex *v=GltfLoadGlbLitMesh(data,size,node,&count,&why);
    if (!v) { fprintf(stderr,"%s: %s\n",path,why); }
    assert(v&&count==expected);
    for(DWORD i=0;i<count*3;i++)
    {
        float *n=v[i].environment.normal;
        Near(n[0]*n[0]+n[1]*n[1]+n[2]*n[2],1);
        assert(isfinite(v[i].x)&&isfinite(v[i].y)&&isfinite(v[i].z));
    }
    if(color==1) { assert(v[0].b>v[0].r); }
    if(color==2) { assert(v[0].r>v[0].b); }
    if(color==3) { assert(v[0].r==255&&v[0].g==57&&v[0].b==0); }
    free(v);
    assert(!GltfLoadGlbLitMesh(data,size,"missing_node",&count,&why)&&count==0);
    free(data);
}
static void Transform(const char *path)
{
    DWORD size,count;const char *why="";unsigned char *data=Read(path,&size);BgVertex *v=GltfLoadGlbLitMesh(data,size,"marker",&count,&why);
    assert(v&&count==1);Near(v[0].x,10);Near(v[0].y,20);Near(v[0].z,26);
    Near(v[0].environment.normal[0],0);Near(v[0].environment.normal[1],0.8f);Near(v[0].environment.normal[2],-0.6f);
    assert(v[0].r==255&&v[0].g==128&&v[0].b==0&&v[0].a==255);free(v);free(data);
    puts("PASS: all four supplied GLBs, orange vertex colors, smooth normals and normals under inherited rotation/nonuniform scale.");
}

static void Swirls(void)
{
    unsigned char data[256]={0},original[256];
    SetupFile setup={0};SetupMarker spawn={0};SetupSwirlPath path={0};const char *why="";
    setup.data=data;setup.size=sizeof(data);strcpy(setup.name,"UsetupcontrolZ");
    spawn.kind=SETUP_MARKER_SPAWN;spawn.position[0]=100;spawn.position[1]=20;spawn.position[2]=300;spawn.look[0]=1;
    Put(data+8,40);Put(data+40,0); /* A non-swirl intro command before the path. */
    for(DWORD i=0;i<5;i++)
    {
        unsigned char *p=data+52+i*32;
        Put(p,3);Put(p+4,2);Put(p+8,i*10*65536);Put(p+12,(DWORD)(i==2?557056:-98304));
        Put(p+20,i==1?49152:0);Put(p+24,40*65536);Put(p+28,0xffffffff);
    }
    Put(data+212,3);Put(data+216,1);Put(data+220,0x7fffffff);Put(data+244,9);
    memcpy(original,data,sizeof(data));
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why));
    assert(path.pointcount==5&&path.curvecount==65); /* 2 travelled segments, 32 samples each. */
    Near(path.points[1].position[0],100);Near(path.points[1].position[1],193.5f);Near(path.points[1].position[2],290);
    Near(path.curve[16][0],100);Near(path.curve[16][1],199.4375f);Near(path.curve[16][2],285);
    for(int a=0;a<3;a++)
    {
        Near(path.curve[0][a],path.points[1].position[a]);
        Near(path.curve[32][a],path.points[2].position[a]);
        Near(path.curve[64][a],path.points[3].position[a]);
    }
    SetupSwirlPathFree(&path);assert(!path.points&&!path.curve&&!path.pointcount&&!path.curvecount);
    assert(!memcmp(data,original,sizeof(data))&&!setup.dirty);
    Put(data+88,0); /* Point 1 now uses world axes instead of the spawn's facing. */
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why));
    Near(path.points[1].position[0],110);Near(path.points[1].position[2],300);SetupSwirlPathFree(&path);
    Put(data+88,6); /* Look-at flag affects aim, not position. */
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why));
    Near(path.points[1].position[0],100);Near(path.points[1].position[2],290);SetupSwirlPathFree(&path);
    Put(data+216,2); /* Missing terminal bit; the following EndIntro is not another point. */
    assert(!SetupFileBuildSwirlPath(&setup,&spawn,&path,&why)&&why[0]&&!path.points&&!path.curve);
    Put(data+216,1);setup.size=220;
    assert(!SetupFileBuildSwirlPath(&setup,&spawn,&path,&why)&&!path.points&&!path.curve);setup.size=256;
    Put(data+88,1); /* Too few controls for the game's four-point interpolation. */
    assert(!SetupFileBuildSwirlPath(&setup,&spawn,&path,&why)&&!path.points&&!path.curve);Put(data+88,2);
    Put(data+8,255);assert(!SetupFileBuildSwirlPath(&setup,&spawn,&path,&why));Put(data+8,40);
    assert(SetupFileBuildSwirlPath(&setup,NULL,&path,&why)&&!path.pointcount&&!path.curvecount);
    strcpy(setup.name,"Ump_setupcontrolZ");
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why)&&!path.pointcount&&!path.curvecount);
    strcpy(setup.name,"UsetupcontrolZ");Put(data+52,9);
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why)&&!path.pointcount&&!path.curvecount);
    puts("PASS: swirl rotation/world offsets, signed fractions, eye height, curve tension/endpoints, tangent/terminal records, invalid data and unchanged source.");
}

static void RealSwirl(const char *file)
{
    SetupFile setup={0};SetupMarker spawn={0};SetupSwirlPath path={0};const char *why="";
    setup.data=Read(file,&setup.size);strcpy(setup.name,"UsetupfixtureZ");
    spawn.kind=SETUP_MARKER_SPAWN;spawn.look[2]=1;
    assert(SetupFileBuildSwirlPath(&setup,&spawn,&path,&why));
    assert(path.pointcount==(setup.size-44)/32-1);
    for(DWORD i=0;i<path.curvecount;i++)for(int a=0;a<3;a++)assert(isfinite(path.curve[i][a]));
    for(DWORD i=1;i+1<path.pointcount;i++)for(int a=0;a<3;a++)Near(path.curve[(i-1)*32][a],path.points[i].position[a]);
    SetupSwirlPathFree(&path);free(setup.data);
}
int main(int argc,char **argv)
{
    assert(argc>=7);Placements();MultiplayerResource(argv[6]);
    Model(argv[1],NULL,1762,0);Model(argv[2],NULL,3430,1);Model(argv[3],NULL,3430,2);Model(argv[4],NULL,12,3);
    Transform(argv[5]);Swirls();
    for(int i=7;i<argc;i++)RealSwirl(argv[i]);
    printf("PASS: %d authored mission swirl paths, control counts and every travelled knot.\n",argc-7);
    return 0;
}
