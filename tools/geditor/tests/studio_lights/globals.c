/* Permanent lighting, legacy appearance, and atomic scene persistence. */
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

static void Lighting(void)
{
    StudioScene scene={0}; StudioSceneDefaultLighting(&scene); scene.camera=g_StudioDefaultCamera; scene.render=g_StudioDefaultRender; strcpy(scene.filename,"Global.rnd");
    StudioMaterial material={0}; BgVertex vertex={0}; float diffuse[3],specular[3],lit[3],highlight[3];
    StudioTransform transform={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&transform,&matrix);
    double eye[3]={0,10,0}; vertex.environment.normal[1]=1; vertex.r=vertex.g=vertex.b=255;
    for (int k=0;k<3;k++) { material.base[k]=.5f; material.specular[k]=1; }
    material.intensity=.25f; material.shininess=32;
    scene.directional.intensity=0; scene.ambient.intensity=.5;
    scene.ambient.color[0]=.2f; scene.ambient.color[1]=.4f; scene.ambient.color[2]=.8f;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    CHECK(NEAR(diffuse[0],.05) && NEAR(diffuse[1],.1) && NEAR(diffuse[2],.2));
    CHECK(specular[0]==0 && specular[1]==0 && specular[2]==0);
    scene.ambient.intensity=0;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    for (int k=0;k<3;k++) CHECK(diffuse[k]==0 && specular[k]==0);
    scene.directional.intensity=.5;
    scene.directional.direction[0]=scene.directional.direction[2]=0; scene.directional.direction[1]=-7;
    scene.directional.color[0]=1; scene.directional.color[1]=.5f; scene.directional.color[2]=0;
    StudioShade(&scene,&material,&vertex,&matrix,eye,lit,highlight,NULL);
    CHECK(NEAR(lit[0],.2) && NEAR(lit[1],.1) && lit[2]==0);
    CHECK(NEAR(highlight[0],.125) && NEAR(highlight[1],.0625) && highlight[2]==0);
    scene.directional.direction[1]=-1;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    CHECK(!memcmp(lit,diffuse,sizeof(lit)) && !memcmp(highlight,specular,sizeof(highlight)));
    scene.directional.direction[1]=1;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    for (int k=0;k<3;k++) CHECK(diffuse[k]==0 && specular[k]==0);
    scene.directional.direction[1]=-1;
    transform.rotation[0]=180; StudioMatrixBuild(&transform,&matrix);
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    for (int k=0;k<3;k++) CHECK(NEAR(diffuse[k],0) && NEAR(specular[k],0));
    transform.rotation[0]=0; StudioMatrixBuild(&transform,&matrix);
    material.base[0]=0;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    CHECK(diffuse[0]==0 && NEAR(specular[0],.125)); material.base[0]=.5f;
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==1);
    scene.lights[1].color[0]=scene.lights[1].color[1]=0;
    scene.ambient.intensity=.5;
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    CHECK(NEAR(diffuse[0],.25) && NEAR(diffuse[1],.2) && NEAR(diffuse[2],.325));
    CHECK(NEAR(specular[0],.125) && NEAR(specular[2],.0625));
    scene.lights[1].intensity=0;
    StudioShade(&scene,&material,&vertex,&matrix,eye,lit,highlight,NULL);
    memset(scene.lights,0,sizeof(scene.lights));
    StudioShade(&scene,&material,&vertex,&matrix,eye,diffuse,specular,NULL);
    CHECK(!memcmp(lit,diffuse,sizeof(lit)) && !memcmp(highlight,specular,sizeof(highlight)));
    CHECK(StudioSceneLightIndex(&scene,STUDIO_SELECT_AMBIENT)==-1 && StudioSceneLightIndex(&scene,STUDIO_SELECT_DIRECTIONAL)==-1);
    puts("PASS: colored ambient/directional terms, zero intensity, normalized travel direction, transformed normals, independent specular, additive local lights and no automatic fallback.");
}

int main(int argc,char **argv)
{
    CHECK(argc==3); Lighting(); StudioScene scene={0},loaded={0}; StudioSceneDefaultLighting(&scene); scene.camera=g_StudioDefaultCamera; scene.render=g_StudioDefaultRender;
    lstrcpyn(scene.project,argv[1],sizeof(scene.project)); strcpy(scene.filename,"Globals.rnd");
    scene.ambient.color[0]=.125f; scene.ambient.color[1]=.25f; scene.ambient.intensity=0;
    scene.directional.color[2]=.75f; scene.directional.intensity=2.5;
    scene.directional.direction[0]=7; scene.directional.direction[1]=-3; scene.directional.direction[2]=.125;
    CHECK(StudioSceneAddLight(&scene,FALSE,&why)==1);
    CHECK(StudioSceneSave(&scene,&why) && StudioSceneLoad(argv[1],"Globals.rnd",&loaded,&why));
    CHECK(!memcmp(&scene.ambient,&loaded.ambient,sizeof(scene.ambient)));
    CHECK(!memcmp(&scene.directional,&loaded.directional,sizeof(scene.directional)) && loaded.lights[1].enabled);
    StudioGlobalLight ambient=scene.ambient,directional=scene.directional;
    scene.directional.intensity=9; test_fail_move=1; CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(argv[1],"Globals.rnd",&loaded,&why) && loaded.directional.intensity==2.5);
    scene.directional=directional;
    for (int i=0;i<7;i++)
    {
        switch(i)
        {
        case 0: scene.ambient.color[0]=NAN; break;
        case 1: scene.ambient.intensity=-1; break;
        case 2: scene.directional.intensity=INFINITY; break;
        case 3: scene.directional.intensity=10001; break;
        case 4: memset(scene.directional.direction,0,sizeof(scene.directional.direction)); break;
        case 5: scene.directional.direction[0]=1e10; break;
        case 6: scene.directional.color[2]=1.1f; break;
        }
        CHECK(!StudioSceneSave(&scene,&why));
        CHECK(StudioSceneLoad(argv[1],"Globals.rnd",&loaded,&why));
        CHECK(!memcmp(&ambient,&loaded.ambient,sizeof(ambient)) && !memcmp(&directional,&loaded.directional,sizeof(directional)));
        scene.ambient=ambient; scene.directional=directional;
    }
    for (int i=0;i<atoi(argv[2]);i++)
    {
        char name[64]; snprintf(name,sizeof(name),"BadGlobal%d.rnd",i); StudioScene snapshot=loaded;
        CHECK(!StudioSceneLoad(argv[1],name,&loaded,&why) && !memcmp(&loaded,&snapshot,sizeof(loaded)));
    }
    for (int i=0;i<5;i++)
    {
        char name[64]; snprintf(name,sizeof(name),"GlobalLegacy%d.rnd",i);
        CHECK(StudioSceneLoad(argv[1],name,&loaded,&why));
        CHECK(!memcmp(&loaded.ambient,&g_StudioDefaultAmbientLight,sizeof(loaded.ambient)));
        CHECK(loaded.directional.intensity==(i<3 ? 1 : 0));
        CHECK(!memcmp(loaded.directional.direction,g_StudioDefaultDirectionalLight.direction,sizeof(loaded.directional.direction)));
        CHECK(StudioSceneSave(&loaded,&why));
    }
    CHECK(StudioSceneLoad(argv[1],"Globals.rnd",&loaded,&why) && loaded.directional.intensity==2.5 && loaded.ambient.intensity==0);
    loaded.directional.intensity=0; strcpy(loaded.filename,"GlobalDark.rnd"); CHECK(StudioSceneSave(&loaded,&why));
    CHECK(StudioSceneLoad(argv[1],"GlobalDark.rnd",&loaded,&why) && loaded.directional.intensity==0 && loaded.ambient.intensity==0);
    StudioSceneFree(&scene); StudioSceneFree(&loaded);
    puts("PASS: global roundtrip with local lights, persistent zero intensity, validation, atomic-save/load rollback and v1-v3 appearance migration including zero-power local lights.");
    return 0;
}
