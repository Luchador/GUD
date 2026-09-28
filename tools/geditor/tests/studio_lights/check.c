#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "studioscene.h"
#include "studiomath.h"

static const char *why="";
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why); exit(1); } } while (0)
#define NEAR(a,b) (fabs((double)(a)-(b))<1e-6)
extern int test_fail_move;
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { return FALSE; }

static void Lighting(void)
{
    StudioScene scene={0}; strcpy(scene.filename,"Light.rnd");
    CHECK(StudioSceneAddLight(&scene,TRUE,&why)==0);
    StudioLight *spot=&scene.lights[0]; double point[3]={0},direction[3];
    spot->position[1]=10; spot->direction[1]=-7; spot->intensity=2;
    CHECK(NEAR(StudioLightSample(spot,TRUE,point,direction),2) && NEAR(direction[1],1));
    point[0]=10*tan(10*3.141592653589793/180); CHECK(NEAR(StudioLightSample(spot,TRUE,point,direction),2));
    point[0]=10*tan(25*3.141592653589793/180); double penumbra=StudioLightSample(spot,TRUE,point,direction);
    CHECK(penumbra>0 && penumbra<2);
    point[0]=10*tan(35*3.141592653589793/180); CHECK(StudioLightSample(spot,TRUE,point,direction)==0);
    spot->inner=spot->outer=30;
    point[0]=10*tan(29*3.141592653589793/180); CHECK(NEAR(StudioLightSample(spot,TRUE,point,direction),2));
    point[0]=0; point[1]=20; CHECK(StudioLightSample(spot,TRUE,point,direction)==0); /* Behind the light. */
    spot->direction[1]=7; CHECK(NEAR(StudioLightSample(spot,TRUE,point,direction),2));
    point[1]=10; CHECK(StudioLightSample(spot,TRUE,point,direction)==0); /* At its origin: finite, no direction. */
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==1);
    StudioLight *light=&scene.lights[1]; light->radius=10; light->intensity=2; light->position[1]=0;
    point[1]=5; CHECK(NEAR(StudioLightSample(light,FALSE,point,direction),.5));
    point[1]=10; CHECK(StudioLightSample(light,FALSE,point,direction)==0);
    point[1]=20; CHECK(StudioLightSample(light,FALSE,point,direction)==0);
    point[1]=0; CHECK(StudioLightSample(light,FALSE,point,direction)==0);
    light->enabled=FALSE; point[1]=1; CHECK(StudioLightSample(light,FALSE,point,direction)==0);
    puts("PASS: spotlight direction/inner cone/penumbra/outer cone, hard cones, point radius/falloff, and coincident positions.");

    memset(&scene,0,sizeof(scene)); StudioSceneDefaultLighting(&scene);
    StudioMaterial material={0}; BgVertex vertex={0}; float diffuse[3],specular[3],legacy[3],legacyspec[3];
    StudioTransform transform={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&transform,&matrix);
    double eye[3]={0,10,0}; vertex.environment.normal[1]=1; vertex.r=vertex.g=vertex.b=255;
    material.intensity=.5; material.shininess=32;
    for (int k=0;k<3;k++) { material.base[k]=.5; material.specular[k]=1; }
    StudioShade(NULL,&material,&vertex,&matrix,eye,legacy,legacyspec);
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(!memcmp(legacy,diffuse,sizeof(diffuse)) && !memcmp(legacyspec,specular,sizeof(specular)));
    strcpy(scene.filename,"Light.rnd"); CHECK(StudioSceneAddLight(&scene,FALSE,&why)==1);
    scene.directional.intensity=0;
    light=&scene.lights[1]; light->color[1]=light->color[2]=0; /* Red at half radius. */
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(NEAR(diffuse[0],.225) && NEAR(diffuse[1],.1) && NEAR(specular[0],.125) && specular[1]==0);
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==2);
    scene.lights[2].color[0]=scene.lights[2].color[1]=0; /* Independent blue contribution. */
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(NEAR(diffuse[0],.225) && NEAR(diffuse[2],.225) && NEAR(specular[2],.125));
    CHECK(StudioSceneAddLight(&scene,TRUE,&why)==0); scene.lights[0].color[0]=scene.lights[0].color[2]=0;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(NEAR(diffuse[1],.6) && NEAR(specular[1],.5)); /* All three sum with ambient. */
    material.base[0]=0; StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(diffuse[0]==0 && NEAR(specular[0],.125)); /* Base color never tints specular. */
    for (int i=0;i<3;i++) { scene.lights[i].intensity=0; }
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(NEAR(diffuse[1],.1) && specular[1]==0); /* Zero lights don't enable fallback. */
    light->intensity=10000; StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(isfinite(diffuse[0]) && specular[0]<=1);
    light->intensity=1; transform.position[0]=20; StudioMatrixBuild(&transform,&matrix);
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular);
    CHECK(NEAR(diffuse[1],.1) && specular[0]==0); /* World-space range follows transformed geometry. */
    puts("PASS: legacy preview fallback, all three colored lights, intensity, ambient-only zero lights, saturation, world transforms and independent specular.");
}

static void Validation(void)
{
    StudioScene scene={0}; strcpy(scene.filename,"Light.rnd"); CHECK(StudioSceneAddLight(&scene,TRUE,&why)==0);
    StudioLight valid=scene.lights[0],bad=valid;
    CHECK(StudioLightValid(&valid,TRUE)); bad.direction[1]=0; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.inner=31; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.outer=91; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.inner=-1; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.inner=bad.outer=0; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.position[0]=NAN; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.direction[0]=INFINITY; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.color[1]=1.1; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.intensity=-1; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.intensity=INFINITY; CHECK(!StudioLightValid(&bad,TRUE));
    bad=valid; bad.radius=0; CHECK(!StudioLightValid(&bad,FALSE));
    bad.radius=NAN; CHECK(!StudioLightValid(&bad,FALSE));
    bad.radius=10; CHECK(StudioLightValid(&bad,FALSE));
}

int main(int argc,char **argv)
{
    CHECK(argc==3); const char *project=argv[1]; StudioScene scene={0},loaded={0}; StudioSceneDefaultLighting(&scene);
    Lighting(); Validation();
    CHECK(StudioSceneAddLight(&scene,TRUE,&why)==-1 && !scene.lights[0].enabled);
    lstrcpyn(scene.project,project,sizeof(scene.project)); strcpy(scene.filename,"Lights.rnd");
    CHECK(StudioSceneAddLight(&scene,TRUE,&why)==0 && StudioSceneLightSlot(&scene,TRUE)==-1);
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==1 && StudioSceneLightSlot(&scene,FALSE)==2);
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==2 && StudioSceneLightSlot(&scene,FALSE)==-1);
    StudioLight before[3]; memcpy(before,scene.lights,sizeof(before));
    CHECK(StudioSceneAddLight(&scene,TRUE,&why)==-1 && StudioSceneAddLight(&scene,FALSE,&why)==-1);
    CHECK(!memcmp(before,scene.lights,sizeof(before)));
    double position[3]={1,2,3}; CHECK(StudioSceneAddModel(&scene,"Light.gltf",position,&why));
    scene.objects[0].transform.rotation[1]=23; scene.objects[0].transform.scale[2]=2.5;
    scene.objects[0].materials[1].shininess=91;
    for (int i=0;i<3;i++)
    {
        scene.lights[i].position[0]=.1+i; scene.lights[i].position[2]=-3-i;
        scene.lights[i].color[0]=.125f*(i+1); scene.lights[i].intensity=2.75+i;
    }
    scene.lights[0].direction[0]=1; scene.lights[0].inner=12.75; scene.lights[0].outer=62.25;
    scene.lights[1].radius=37.125; scene.lights[2].radius=1.0/3;
    CHECK(StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(project,"Lights.rnd",&loaded,&why) && !why[0]);
    CHECK(!memcmp(scene.lights,loaded.lights,sizeof(scene.lights)) && loaded.count==1);
    CHECK(!memcmp(&scene.objects[0].transform,&loaded.objects[0].transform,sizeof(StudioTransform)));
    CHECK(loaded.objects[0].materials[1].shininess==91);
    scene.lights[0].intensity=99; test_fail_move=1; CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(project,"Lights.rnd",&loaded,&why) && loaded.lights[0].intensity==2.75);
    scene.lights[0].intensity=2.75; scene.lights[1].radius=-1; CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(project,"Lights.rnd",&loaded,&why) && loaded.lights[1].radius==37.125);
    for (int i=0;i<atoi(argv[2]);i++)
    {
        char name[64]; snprintf(name,sizeof(name),"Bad%d.rnd",i);
        StudioScene snapshot=loaded;
        CHECK(!StudioSceneLoad(project,name,&loaded,&why));
        CHECK(!memcmp(&snapshot,&loaded,sizeof(snapshot))); /* Including original object ownership. */
    }
    for (int i=1;i<=2;i++)
    {
        char name[64]; snprintf(name,sizeof(name),"Legacy%d.rnd",i);
        CHECK(StudioSceneLoad(project,name,&loaded,&why));
        for (int k=0;k<3;k++) { CHECK(!loaded.lights[k].enabled); }
        CHECK(loaded.count==1 && loaded.objects[0].transform.position[1]==2);
        CHECK(StudioSceneSave(&loaded,&why));
    }
    /* Deleting a lower point-light slot must not renumber the surviving light. */
    strcpy(scene.filename,"Sparse.rnd"); memset(&scene.lights[1],0,sizeof(scene.lights[1]));
    CHECK(StudioSceneSave(&scene,&why) && StudioSceneLoad(project,"Sparse.rnd",&loaded,&why));
    CHECK(!loaded.lights[1].enabled && loaded.lights[2].enabled && loaded.lights[2].radius==1.0/3);
    CHECK(StudioSceneLightSlot(&loaded,FALSE)==1 && StudioSceneAddLight(&loaded,FALSE,&why)==1);
    CHECK(loaded.lights[2].intensity==4.75 && loaded.lights[1].intensity==1);
    strcpy(loaded.filename,"Reused.rnd"); CHECK(StudioSceneSave(&loaded,&why));
    memset(loaded.lights,0,sizeof(loaded.lights)); strcpy(loaded.filename,"Empty.rnd");
    CHECK(StudioSceneSave(&loaded,&why));
    CHECK(StudioSceneLoad(project,"Empty.rnd",&loaded,&why) && StudioSceneLightSlot(&loaded,TRUE)==0);
    StudioSceneFree(&scene); StudioSceneFree(&loaded);
    puts("PASS: one spotlight/two point limits, every light property persisted alongside models/materials, validation, failed-save/load rollback and v1/v2 migration.");
    return 0;
}
