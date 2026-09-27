#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdint.h>
#include "studiogizmo.h"
#include "resource.h"
static const char *why="";
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why); exit(1); } } while(0)
#define NEAR(a,b) (fabs((double)(a)-(b))<1e-5)
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { return FALSE; }
extern int test_fail_move;
static const StudioTransform identity={{0},{0},{1,1,1}};
static void Write(const char *path,const char *text)
{ FILE *f=fopen(path,"wb"); CHECK(f); CHECK(fputs(text,f)!=EOF); CHECK(!fclose(f)); }

/* Resource API shim: decode exactly the GLBs used by the editor. */
typedef void *HRSRC,*HGLOBAL;
#define MAKEINTRESOURCE(x) ((const char *)(uintptr_t)(x))
#define RT_RCDATA ((const char *)10)
static unsigned char *resources[3]; static DWORD sizes[3];
static HRSRC FindResource(HINSTANCE instance,const char *name,const char *type)
{
    int ids[3]={IDR_GIZMO_ARROW,IDR_GIZMO_CYLINDER,IDR_GIZMO_SCALE};
    for(int i=0;i<3;i++) if((uintptr_t)name==(uintptr_t)ids[i]) return (HRSRC)(uintptr_t)(i+1);
    return NULL;
}
static HGLOBAL LoadResource(HINSTANCE instance,HRSRC resource) { return resource; }
static void *LockResource(HGLOBAL resource) { return resources[(uintptr_t)resource-1]; }
static DWORD SizeofResource(HINSTANCE instance,HRSRC resource) { return sizes[(uintptr_t)resource-1]; }
#include "gizmo.inc"

static void Gizmos(const char *assetdir)
{
    const char *names[3]={"gizmo_arrow.glb","gizmo_cylinder.glb","gizmo_scale.glb"};
    StudioGizmo gizmo={0}; StudioTransform transform=identity; transform.rotation[1]=25;
    OrbitCamera camera={0}; camera.yaw=35; camera.pitch=-25; camera.distance=10;
    for(int i=0;i<3;i++)
    {
        char path[1024]; snprintf(path,sizeof(path),"%s/%s",assetdir,names[i]); FILE *f=fopen(path,"rb"); CHECK(f);
        CHECK(!fseek(f,0,SEEK_END)); sizes[i]=ftell(f); CHECK(!fseek(f,0,SEEK_SET));
        resources[i]=malloc(sizes[i]); CHECK(resources[i] && fread(resources[i],1,sizes[i],f)==sizes[i]); CHECK(!fclose(f));
    }
    CHECK(StudioGizmoLoad(&gizmo,NULL));
    for(int mode=0;mode<3;mode++)
    {
        StudioGizmoFrame frame; unsigned mask=0,target=mode==STUDIO_SCALE ? 15 : 7;
        CHECK(StudioGizmoPlace(&transform,&camera,900,600,mode,&frame));
        for(int axis=0;axis<(mode==STUDIO_SCALE ? 4 : 3);axis++)
            for(DWORD face=0;face<StudioGizmoCount(&gizmo,&frame,axis) && mask!=target;face++)
            {
                double center[3]={0},screen[2],eye[3],ray[3],hit[3];
                for(int corner=0;corner<3;corner++)
                { double v[3]; StudioGizmoVertex(&gizmo,&frame,axis,face*3+corner,v); for(int k=0;k<3;k++) center[k]+=v[k]/3; }
                CHECK(StudioProject(&camera,900,600,center,screen));
                CHECK(StudioRay(&camera,900,600,screen[0],screen[1],eye,ray));
                int picked=StudioGizmoPick(&gizmo,&frame,eye,ray,hit); if(picked>=0) mask|=1u<<picked;
            }
        CHECK(mask==target);
    }
    StudioGizmoFree(&gizmo); for(int i=0;i<3;i++) free(resources[i]);
    puts("PASS: actual arrow/ring/scale GLB resources; all XYZ handles and uniform-scale cube are pickable.");
}

static void Dragging(void)
{
    OrbitCamera camera={0}; camera.distance=10; camera.yaw=30; camera.pitch=-20;
    StudioTransform transform=identity,result; StudioGizmoFrame frame; StudioDrag drag;
    double hit[3],end[3],screen[2],screenend[2];
    CHECK(StudioGizmoPlace(&transform,&camera,900,600,STUDIO_TRANSLATE,&frame));
    for(int axis=0;axis<3;axis++)
    {
        for(int k=0;k<3;k++) { hit[k]=k==axis ? frame.length*.8 : 0; end[k]=hit[k]+(k==axis ? 1 : 0); }
        CHECK(StudioProject(&camera,900,600,hit,screen) && StudioProject(&camera,900,600,end,screenend));
        CHECK(StudioDragBegin(&drag,&transform,&camera,900,600,screen[0],screen[1],&frame,axis,hit));
        CHECK(StudioDragUpdate(&drag,screenend[0],screenend[1],FALSE,&result));
        for(int k=0;k<3;k++) CHECK(NEAR(result.position[k],k==axis ? 1 : 0));
    }
    transform.rotation[2]=90; CHECK(StudioGizmoPlace(&transform,&camera,900,600,STUDIO_SCALE,&frame));
    for(int k=0;k<3;k++) { hit[k]=frame.axes.m[k][0]*frame.length*.8; end[k]=hit[k]+frame.axes.m[k][0]*frame.length*.5; }
    CHECK(StudioProject(&camera,900,600,hit,screen) && StudioProject(&camera,900,600,end,screenend));
    CHECK(StudioDragBegin(&drag,&transform,&camera,900,600,screen[0],screen[1],&frame,0,hit));
    CHECK(StudioDragUpdate(&drag,screenend[0],screenend[1],FALSE,&result));
    CHECK(NEAR(result.scale[0],1.5) && NEAR(result.scale[1],1) && NEAR(result.rotation[2],90));
    transform.scale[0]=2; transform.scale[1]=3; transform.scale[2]=4;
    CHECK(StudioDragBegin(&drag,&transform,&camera,900,600,400,300,&frame,3,hit));
    CHECK(StudioDragUpdate(&drag,400+log(2)*100,300,FALSE,&result));
    CHECK(NEAR(result.scale[0],4) && NEAR(result.scale[1],6) && NEAR(result.scale[2],8));
    CHECK(StudioDragUpdate(&drag,-1e6,1e6,FALSE,&result) && StudioTransformValid(&result));
    CHECK(StudioDragUpdate(&drag,1e6,-1e6,FALSE,&result) && StudioTransformValid(&result));
    camera.yaw=camera.pitch=0; transform=identity;
    CHECK(StudioGizmoPlace(&transform,&camera,900,600,STUDIO_ROTATE,&frame));
    hit[0]=frame.length; hit[1]=hit[2]=0;
    CHECK(StudioProject(&camera,900,600,hit,screen));
    CHECK(StudioDragBegin(&drag,&transform,&camera,900,600,screen[0],screen[1],&frame,2,hit));
    const double angles[]={33,170,190};
    for(int i=0;i<3;i++)
    {
        double radians=angles[i]*3.14159265358979323846/180;
        end[0]=frame.length*cos(radians); end[1]=frame.length*sin(radians); end[2]=0;
        CHECK(StudioProject(&camera,900,600,end,screenend));
        CHECK(StudioDragUpdate(&drag,screenend[0],screenend[1],i==0,&result));
        CHECK(NEAR(result.rotation[2],i==0 ? 30 : i==1 ? 170 : -170));
    }
    CHECK(StudioGizmoPlace(&transform,&camera,900,600,STUDIO_TRANSLATE,&frame));
    hit[0]=hit[1]=0; hit[2]=frame.length;
    CHECK(StudioDragBegin(&drag,&transform,&camera,900,600,450,300,&frame,2,hit) && drag.fallback);
    CHECK(StudioDragUpdate(&drag,450,210,FALSE,&result) && NEAR(result.position[2],frame.length));
    puts("PASS: XYZ translation, rotated local-axis scaling, uniform scale ratios/limits, world rotation, Ctrl snap and angle wrap.");
}

int main(int argc,char **argv)
{
    CHECK(argc==3); StudioScene scene={0},loaded={0}; double position[3]={0}; char path[MAX_PATH];
    lstrcpyn(scene.project,argv[1],sizeof(scene.project)); lstrcpyn(scene.filename,"Main.rnd",sizeof(scene.filename));
    CHECK(StudioSceneAddModel(&scene,"Light.gltf",position,&why)); CHECK(StudioSceneAddModel(&scene,"Light.gltf",position,&why));
    StudioTransform transform={{10,20,30},{0,0,90},{2,3,4}}; scene.objects[0].transform=transform;
    scene.objects[1].transform=transform; scene.objects[1].transform.position[2]=32; scene.objects[1].transform.scale[2]=.01;
    StudioMatrix matrix; StudioMatrixBuild(&transform,&matrix); double point[3]={1,2,3}; StudioPoint(&matrix,point,point);
    CHECK(NEAR(point[0],4) && NEAR(point[1],22) && NEAR(point[2],42));
    double lo[3],hi[3]; CHECK(StudioBounds(&scene,0,lo,hi));
    CHECK(NEAR(lo[0],7) && NEAR(hi[0],10) && NEAR(lo[1],20) && NEAR(hi[1],30) && NEAR(lo[2],30));
    double eye[3]={9.4,24.4,40},ray[3]={0,0,-1}; int material;
    CHECK(StudioPick(&scene,eye,ray,&material)==1 && material==1); /* World distances despite very different scales. */
    StudioMaterial m={0}; BgVertex v={0}; float diffuse[3],specular[3];
    m.base[0]=m.base[1]=m.base[2]=1; m.shininess=32; v.r=v.g=v.b=255; v.environment.normal[0]=v.environment.normal[1]=1;
    StudioShade(&m,&v,&matrix,eye,diffuse,specular);
    double nl=(-.348742916/3+.813733471/2)/sqrt(1.0/9+.25);
    CHECK(NEAR(diffuse[0],.2+.8*nl));
    CHECK(StudioSceneSave(&scene,&why) && StudioSceneLoad(argv[1],"Main.rnd",&loaded,&why));
    CHECK(!memcmp(&loaded.objects[0].transform,&transform,sizeof(transform)));
    CHECK(loaded.objects[1].transform.scale[2]==.01 && loaded.objects[0].asset==loaded.objects[1].asset);
    scene.objects[0].transform.rotation[1]=57; test_fail_move=1; CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(argv[1],"Main.rnd",&loaded,&why) && loaded.objects[0].transform.rotation[1]==0);
    snprintf(path,sizeof(path),"%s/studio/scenes/Legacy.rnd",argv[1]);
    Write(path,"{\"format\":\"GEditor Render Studio\",\"version\":1,\"objects\":[{\"model\":\"Light.gltf\",\"position\":[4,5,6],\"materials\":[]}]}");
    CHECK(StudioSceneLoad(argv[1],"Legacy.rnd",&loaded,&why));
    CHECK(loaded.objects[0].transform.position[0]==4 && loaded.objects[0].transform.scale[2]==1 && loaded.objects[0].transform.rotation[1]==0);
    Write(path,"{\"format\":\"GEditor Render Studio\",\"version\":2,\"objects\":[{\"model\":\"Light.gltf\",\"position\":[4,5,6],\"rotation\":[0,0,0],\"scale\":[1,0,1],\"materials\":[]}]}");
    CHECK(!StudioSceneLoad(argv[1],"Legacy.rnd",&loaded,&why) && loaded.objects[0].transform.scale[1]==1);
    scene.objects[0].transform.scale[0]=NAN; CHECK(!StudioSceneSave(&scene,&why));
    StudioSceneFree(&scene); StudioSceneFree(&loaded);
    puts("PASS: transformed bounds/picking, inverse-transpose lighting, v2 save/reload, v1 defaults and failed-save/invalid-load rollback.");
    Dragging(); Gizmos(argv[2]); return 0;
}
