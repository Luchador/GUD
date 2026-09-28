#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studiomath.h"
static const char *why="";
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why); exit(1); } } while (0)
#define NEAR(a,b) (fabs((double)(a)-(b))<1e-6)
extern int test_fail_move;
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { return FALSE; }

static void NormalForReflection(BgVertex *v,double x,double y,double z)
{
    double length=sqrt(x*x+y*y+(z+1)*(z+1));
    if (length<1e-12) { v->environment.normal[0]=1; v->environment.normal[1]=v->environment.normal[2]=0; }
    else { v->environment.normal[0]=x/length; v->environment.normal[1]=y/length; v->environment.normal[2]=(z+1)/length; }
}
static void Mapping(void)
{
    StudioTransform transform={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&transform,&matrix);
    BgVertex v[3]={{0}}; double eye[3]={0,0,10},uv[3][2];
    const double axes[6][3]={{0,0,1},{1,0,0},{-1,0,0},{0,0,-1},{0,1,0},{0,-1,0}};
    const double expected[6][2]={{.5,.5},{.75,.5},{.25,.5},{0,.5},{0,0},{0,1}};
    for (int i=0;i<6;i++)
    {
        for (int k=0;k<3;k++) NormalForReflection(v+k,axes[i][0],axes[i][1],axes[i][2]);
        StudioEnvironmentCoordinates(v,&matrix,eye,uv);
        for (int k=0;k<3;k++) { CHECK(NEAR(uv[k][1],expected[i][1])); if (i<4) CHECK(NEAR(fmod(uv[k][0],1),expected[i][0])); }
    }
    for (int k=0;k<3;k++) NormalForReflection(v+k,k==1 ? -.01 : .01,0,-sqrt(1-.0001));
    StudioEnvironmentCoordinates(v,&matrix,eye,uv);
    CHECK(fabs(uv[0][0]-uv[1][0])<.01 && fabs(uv[1][0]-uv[2][0])<.01);
    NormalForReflection(v+2,0,1,0); StudioEnvironmentCoordinates(v,&matrix,eye,uv);
    CHECK(NEAR(uv[2][1],0) && NEAR(uv[2][0],(uv[0][0]+uv[1][0])/2));
    for (int k=0;k<3;k++) NormalForReflection(v+k,0,0,1);
    eye[0]=10; eye[2]=0; StudioEnvironmentCoordinates(v,&matrix,eye,uv); CHECK(NEAR(uv[0][0],.25));
    transform.rotation[1]=90; StudioMatrixBuild(&transform,&matrix);
    StudioEnvironmentCoordinates(v,&matrix,eye,uv); CHECK(NEAR(uv[0][0],.75));
    transform.rotation[1]=0; transform.position[0]=3; transform.position[1]=2;
    eye[0]=3; eye[1]=2; eye[2]=10; StudioMatrixBuild(&transform,&matrix);
    StudioEnvironmentCoordinates(v,&matrix,eye,uv); CHECK(NEAR(uv[0][0],.5) && NEAR(uv[0][1],.5));
    transform.scale[0]=2; StudioMatrixBuild(&transform,&matrix);
    for (int k=0;k<3;k++) { v[k].environment.normal[0]=v[k].environment.normal[2]=1; }
    StudioEnvironmentCoordinates(v,&matrix,eye,uv);
    CHECK(NEAR(uv[0][0],.5+atan2(.8,.6)/(2*3.14159265358979323846)));
    puts("PASS: six panorama directions, top-down orientation, seam unwrapping, pole longitude, camera movement, translation, rotation and inverse-transpose scaled normals.");
}
int main(int argc,char **argv)
{
    CHECK(argc==3); Mapping(); StudioScene scene={0},loaded={0}; StudioSceneDefaultLighting(&scene); scene.camera=g_StudioDefaultCamera; scene.render=g_StudioDefaultRender;
    lstrcpyn(scene.project,argv[1],sizeof(scene.project)); strcpy(scene.filename,"Environment.rnd");
    strcpy(scene.environment,"Studio 360.bmp"); CHECK(StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(argv[1],scene.filename,&loaded,&why) && !strcmp(loaded.environment,scene.environment));
    strcpy(scene.environment,"Other.bmp"); test_fail_move=1; CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(argv[1],scene.filename,&loaded,&why) && !strcmp(loaded.environment,"Studio 360.bmp"));
    strcpy(scene.environment,"../Escape.bmp"); CHECK(!StudioSceneSave(&scene,&why));
    for (int i=0;i<atoi(argv[2]);i++)
    {
        char name[64]; snprintf(name,sizeof(name),"BadEnvironment%d.rnd",i); StudioScene before=loaded;
        CHECK(!StudioSceneLoad(argv[1],name,&loaded,&why) && !memcmp(&before,&loaded,sizeof(before)));
    }
    for (int version=1;version<=5;version++)
    {
        char name[64]; snprintf(name,sizeof(name),"EnvironmentLegacy%d.rnd",version);
        CHECK(StudioSceneLoad(argv[1],name,&loaded,&why) && !loaded.environment[0]);
        CHECK(StudioSceneSave(&loaded,&why));
    }
    CHECK(StudioSceneLoad(argv[1],"Environment.rnd",&loaded,&why) && !strcmp(loaded.environment,"Studio 360.bmp"));
    strcpy(loaded.filename,"EnvironmentNone.rnd"); loaded.environment[0]=0; CHECK(StudioSceneSave(&loaded,&why));
    CHECK(StudioSceneLoad(argv[1],loaded.filename,&loaded,&why) && !loaded.environment[0]);
    StudioSceneFree(&scene); StudioSceneFree(&loaded);
    puts("PASS: environment/None persistence, missing image retained, invalid filename/documents, failed-save/load rollback and v1-v5 migration.");
    return 0;
}
