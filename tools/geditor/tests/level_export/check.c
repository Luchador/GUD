#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "levelexport.h"
#include "gltf.h"
#include "texload.h"
#include "png.inc"

static const char *why = "";
static BOOL missing;
extern int test_fail_move;
#define OK(x) do { if (!(x)) { fprintf(stderr, "%d: %s: %s\n", __LINE__, #x, why); abort(); } } while (0)
BOOL TexGetProjectImageSize(const char *project, DWORD id, int *w, int *h)
{ *w = *h = 2; return !missing; }
BOOL TexLoadProjectImage(const char *project, DWORD id, TexPixel *out, int *w, int *h)
{
    if (missing) { return FALSE; }
    assert(project && id != BG_TEX_NONE);
    *w = *h = 2;
    for (int i = 0; i < 4; i++) { out[i] = (TexPixel){32,64,128,(unsigned char)(i ? 255 : 64)}; }
    return TRUE;
}
BOOL TexEncodePng(const TexPixel *p, int w, int h, unsigned char **data, DWORD *size)
{
    assert(w == 2 && h == 2);
    *size = p[0].a == 255 ? sizeof(opaque_png) : sizeof(alpha_png);
    *data = malloc(*size); assert(*data);
    memcpy(*data, p[0].a == 255 ? opaque_png : alpha_png, *size);
    return TRUE;
}
static void Put(unsigned char *p, DWORD value)
{ p[0]=value>>24; p[1]=value>>16; p[2]=value>>8; p[3]=value; }
static void Sentinel(const char *path)
{ FILE *f=fopen(path,"wb");assert(f);assert(fwrite("keep",1,4,f)==4);fclose(f); }
static void Unchanged(const char *path)
{ char text[5]={0};FILE *f=fopen(path,"rb");assert(f);assert(fread(text,1,5,f)==4&&!strcmp(text,"keep"));fclose(f); }
static unsigned char *Read(const char *path,DWORD *size)
{
    FILE *f=fopen(path,"rb");assert(f);fseek(f,0,SEEK_END);*size=ftell(f);rewind(f);
    unsigned char *p=malloc(*size);assert(p&&fread(p,1,*size,f)==*size);fclose(f);return p;
}
int main(int argc,char **argv)
{
    assert(argc == 2 || argc == 4);
    char path[MAX_PATH];
    BgDocumentRoom rooms[3]={0};
    BgDocumentVertex vertices[3]={
        {.x=0,.y=0,.z=0,.s=64,.t=32,.r=128,.g=64,.b=32,.a=64},
        {.x=100,.y=0,.z=0,.s=0,.t=0,.r=128,.g=64,.b=32,.a=128},
        {.x=0,.y=100,.z=0,.s=32,.t=64,.r=128,.g=64,.b=32,.a=255}};
    BgDocumentFace faces[4]={0};
    unsigned char commands[3][16]={{0}};
    const DWORD modes[3]={0x00552078,0x00553078,0x005049d8};
    BgDocumentDrawGroup groups[3]={0};
    for (int i=0;i<3;i++) {
        Put(commands[i],0xb7000000);Put(commands[i]+4,1);
        Put(commands[i]+8,0xb900031d);Put(commands[i]+12,modes[i]);
        groups[i]=(BgDocumentDrawGroup){.commands=commands[i],.commandsize=16};
    }
    for (int i=0;i<4;i++) {
        faces[i]=(BgDocumentFace){.id=i+1,.room=i==3?2:1,.layer=i==2,
            .textureid=i==1?BG_TEX_NONE:1,.vertexindices={0,1,2},.cullbackfaces=i!=2};
        BgMaterialInit(&faces[i].material);BgMaterialSetTexture(&faces[i].material,faces[i].textureid);
        faces[i].material.alphasource=i==0?BG_ALPHA_VERTEX:BG_ALPHA_TEXTURE_VERTEX;
        BgMaterialSetWrap(&faces[i].material,FALSE,BG_TEXTURE_CLAMP);
        BgMaterialSetWrap(&faces[i].material,TRUE,BG_TEXTURE_MIRROR);
    }
    for (int i=1;i<=2;i++) {
        rooms[i]=(BgDocumentRoom){.origin={100,200,-300},.vertices=vertices,.vertexcount=3,
            .faces=faces+(i==2?3:0),.facecount=i==2?1:3};
        rooms[i].layers[0]=(BgDocumentLayerData){.groups=groups+(i==2?2:0),.groupcount=1};
        rooms[i].layers[1]=(BgDocumentLayerData){.groups=groups+1,.groupcount=1};
    }
    BgDocument doc={.rooms=rooms,.roomcount=2,.facecount=4,.levelscale=.5f,.dirty=TRUE};
    vertices[0].x=25; /* An unsaved edit must be exported. */
    BgDocumentVertex saved[3]; memcpy(saved,vertices,sizeof(saved));
    snprintf(path,sizeof(path),"%s/background.glb",argv[1]);
    OK(LevelExportBackground(path,"project",&doc,&why));
    assert(doc.dirty&&!memcmp(vertices,saved,sizeof(saved)));
    DWORD size,count;unsigned char *data=Read(path,&size);
    BgVertex *loaded=GltfLoadGlbMesh(data,size,&count,&why);
    OK(loaded&&count==4);assert(fabsf(loaded[0].x-2.5f)<.0001f);free(loaded);free(data);
    /* The existing JSON model writer keeps its one-mesh behavior. */
    BgVertex tri[3]={{.x=0,.y=0,.z=0,.r=255,.g=255,.b=255,.a=255},
        {.x=1,.y=0,.z=0,.r=255,.g=255,.b=255,.a=255},
        {.x=0,.y=1,.z=0,.r=255,.g=255,.b=255,.a=255}};
    snprintf(path,sizeof(path),"%s/model.gltf",argv[1]);
    OK(GltfWriteModel(path,NULL,tri,NULL,NULL,1,&why));
    StanTile tiles[2]={
        {.room=1,.red=17,.green=34,.blue=51,.pointcount=4,
            .points={{250,400,-600,0},{400,400,-600,0},{400,600,-600,0},{250,600,-600,0}}},
        {.room=2,.special=STAN_TYPE_LADDER,.red=68,.green=85,.blue=102,.pointcount=3,
            .points={{0,0,0,0},{0,100,0,0},{0,100,100,0}}}};
    StanFile stan={.tiles=tiles,.tilecount=2,.levelscale=.5f,.dirty=TRUE};
    StanTile original[2];memcpy(original,tiles,sizeof(tiles));
    snprintf(path,sizeof(path),"%s/stans.glb",argv[1]);OK(LevelExportStans(path,&stan,&why));
    assert(stan.dirty&&!memcmp(original,tiles,sizeof(tiles)));
    snprintf(path,sizeof(path),"%s/protected.glb",argv[1]);Sentinel(path);
    missing=TRUE;assert(!LevelExportBackground(path,"project",&doc,&why));Unchanged(path);missing=FALSE;
    test_fail_move=1;assert(!LevelExportStans(path,&stan,&why));Unchanged(path);
    tiles[0].pointcount=2;assert(!LevelExportStans(path,&stan,&why));Unchanged(path);tiles[0].pointcount=4;
    tiles[0].points[0].x=NAN;assert(!LevelExportStans(path,&stan,&why));Unchanged(path);
    assert(!LevelExportBackground(path,"project",NULL,&why));
    assert(!LevelExportStans(path,NULL,&why));
    if(argc==4) {
        data=Read(argv[2],&size);BgDocument native={0};OK(BgDocumentLoad(data,size,1,&native,&why));free(data);
        snprintf(path,sizeof(path),"%s/native-bg.glb",argv[1]);OK(LevelExportBackground(path,"project",&native,&why));
        printf("Native background: %u faces\n",native.facecount);BgDocumentFree(&native);
        data=Read(argv[3],&size);StanFile ns={0};OK(StanLoadNative(data,size,1,&ns,&why));free(data);
        snprintf(path,sizeof(path),"%s/native-stans.glb",argv[1]);OK(LevelExportStans(path,&ns,&why));
        printf("Native stans: %u tiles\n",ns.tilecount);StanFileFree(&ns);
    }
    puts("PASS: live BG/STAN export, GLB reload, source immutability and failed-output preservation.");
    return 0;
}
