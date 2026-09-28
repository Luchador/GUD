#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "studioscene.h"
#include "studiomath.h"
#include "gltfjson.h"

static const char *why="";
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s (%s)\n",__LINE__,#x,why); exit(1); } } while (0)
#define NEAR(a,b) (fabs((double)(a)-(b))<1e-6)
extern int test_fail_move;
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { return FALSE; }
static void Write(const char *path,const char *text)
{ FILE *f=fopen(path,"wb"); CHECK(f); CHECK(fputs(text,f)!=EOF); CHECK(!fclose(f)); }

static void Import(const char *project)
{
    char path[MAX_PATH]; GltfModelImport mesh={0}; float (*colors)[4]=NULL;
    snprintf(path,sizeof(path),"%s/studio/models/Light.gltf",project);
    CHECK(GltfReadStudioModel(path,&mesh,&colors,&why));
    CHECK(mesh.count==3 && mesh.materials.count==3 && mesh.materials.facecount==3);
    CHECK(!strcmp(mesh.materials.slots[0].name,"Housing \"blue\""));
    CHECK(!strcmp(mesh.materials.slots[1].name,"R\xc3\xa9" "flecteur"));
    for (int i=0;i<3;i++)
    {
        CHECK(mesh.materials.faces[i].slot==(DWORD)i);
        CHECK(mesh.vertices[i*3].r==255 && mesh.vertices[i*3].g==255 && mesh.vertices[i*3].b==255);
        CHECK(NEAR(mesh.materials.faces[i].uv[0],.125) && NEAR(mesh.materials.faces[i].uv[1],.25));
    }
    CHECK(NEAR(colors[0][0],.25) && NEAR(colors[0][1],.5));
    CHECK(NEAR(mesh.vertices[6].environment.normal[2],1)); /* Generated flat normal. */
    GltfFreeModelImport(&mesh); free(colors); colors=NULL;
    snprintf(path,sizeof(path),"%s/studio/models/transformed.gltf",project);
    CHECK(GltfReadStudioModel(path,&mesh,&colors,&why));
    CHECK(NEAR(mesh.vertices[0].x,10) && NEAR(mesh.vertices[0].y,20) && NEAR(mesh.vertices[0].z,30));
    CHECK(NEAR(mesh.vertices[1].x,10) && NEAR(mesh.vertices[1].y,23)); /* Mirrored winding. */
    double length=sqrt(.25+1.0/9+1.0/16);
    CHECK(NEAR(mesh.vertices[3].environment.normal[0],-.5/length));
    CHECK(NEAR(mesh.vertices[3].environment.normal[1],(1.0/3)/length));
    CHECK(NEAR(mesh.vertices[3].environment.normal[2],.25/length));
    CHECK(NEAR(mesh.vertices[6].environment.normal[2],1));
    GltfFreeModelImport(&mesh); free(colors); colors=NULL;
    snprintf(path,sizeof(path),"%s/studio/models/external.gltf",project);
    CHECK(GltfReadStudioModel(path,&mesh,&colors,&why) && mesh.count==3);
    GltfFreeModelImport(&mesh); free(colors); colors=NULL;
    snprintf(path,sizeof(path),"%s/studio/models/animated.gltf",project);
    CHECK(!GltfReadStudioModel(path,&mesh,&colors,&why));
    CHECK(!mesh.vertices && !colors);
    puts("PASS: named glTF slots, separate base colors, UVs, external buffers, authored/generated normals, transformed normals and mirrored winding.");
}

static void Math(const StudioScene *scene)
{
    double lower[3],upper[3],origin[3]={2.2,.2,10},direction[3]={0,0,-1}; int material;
    CHECK(StudioBounds(scene,-1,lower,upper) && NEAR(lower[0],0) && NEAR(upper[0],5) && NEAR(upper[2],2));
    CHECK(StudioPick(scene,origin,direction,&material)==1 && material==1);
    origin[0]=.2; CHECK(StudioPick(scene,origin,direction,&material)==1 && material==0);
    origin[0]=4.2; CHECK(StudioPick(scene,origin,direction,&material)==1 && material==2);
    origin[0]=8; CHECK(StudioPick(scene,origin,direction,&material)==-1 && material==-1);
    OrbitCamera camera={0}; camera.center[0]=3; camera.center[1]=2; camera.yaw=37; camera.pitch=-20; camera.distance=10;
    CHECK(StudioRay(&camera,800,600,400,300,origin,direction));
    for (int k=0;k<3;k++) CHECK(NEAR(origin[k]+direction[k]*10,camera.center[k]));
    CHECK(!StudioRay(&camera,0,600,0,0,origin,direction));
    StudioMaterial m={0}; BgVertex v={0}; float diffuse[3],specular[3],wide;
    StudioTransform identity={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&identity,&matrix);
    double eye[3]={-.348742916,.813733471,-.464990554};
    v.environment.normal[1]=1; v.r=v.g=v.b=255;
    for(int k=0;k<3;k++) { m.base[k]=.5; m.specular[k]=1; }
    m.intensity=.75; m.shininess=32;
    StudioShade(NULL,&m,&v,&matrix,eye,diffuse,specular,NULL);
    CHECK(NEAR(diffuse[0],.5*(.2+.8*.813733471)) && NEAR(specular[0],.75));
    m.base[0]=0; m.specular[1]=0;
    StudioShade(NULL,&m,&v,&matrix,eye,diffuse,specular,NULL);
    CHECK(diffuse[0]==0 && NEAR(specular[0],.75) && specular[1]==0); /* Independent specular color. */
    eye[0]+=.3; m.shininess=4; StudioShade(NULL,&m,&v,&matrix,eye,diffuse,specular,NULL); wide=specular[0];
    m.shininess=64; StudioShade(NULL,&m,&v,&matrix,eye,diffuse,specular,NULL); CHECK(specular[0]<wide && specular[0]>0);
    m.intensity=0; StudioShade(NULL,&m,&v,&matrix,eye,diffuse,specular,NULL); CHECK(specular[0]==0);
    puts("PASS: instance bounds, nearest face/material picking, camera rays and independent Phong color/intensity/shininess.");
}

static void EmissionMetalness(void)
{
    StudioScene scene={0}; StudioSceneDefaultLighting(&scene); scene.ambient.intensity=0;
    scene.directional.intensity=.5; scene.directional.direction[0]=scene.directional.direction[2]=0; scene.directional.direction[1]=-1;
    StudioMaterial m={0}; m.base[0]=.8f; m.base[1]=.4f; m.base[2]=.1f;
    m.intensity=.5f; m.shininess=32; for(int k=0;k<3;k++) m.specular[k]=1;
    StudioTransform identity={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&identity,&matrix);
    BgVertex v={0}; v.environment.normal[1]=1; v.r=v.g=v.b=255; double eye[3]={0,10,0};
    float diffuse[3],additive[3],metallic[3];
    for(int step=0;step<=2;step++)
    {
        m.metalness=step*.5f; StudioShade(&scene,&m,&v,&matrix,eye,diffuse,additive,metallic);
        for(int k=0;k<3;k++)
        {
            CHECK(NEAR(diffuse[k],m.base[k]*.4*(1-m.metalness)));
            CHECK(NEAR(additive[k],.25*(1-m.metalness)));
            CHECK(NEAR(metallic[k],m.base[k]*.25*m.metalness));
        }
    }
    m.specular[0]=m.specular[1]=m.specular[2]=0;
    StudioShade(&scene,&m,&v,&matrix,eye,diffuse,additive,metallic);
    CHECK(NEAR(metallic[0],.2) && NEAR(metallic[1],.1) && NEAR(metallic[2],.025));
    m.intensity=0; StudioShade(&scene,&m,&v,&matrix,eye,diffuse,additive,metallic);
    for(int k=0;k<3;k++) CHECK(metallic[k]==0);
    scene.directional.intensity=0; v.r=v.g=v.b=0; m.emission[0]=.125f; m.emission[1]=.25f; m.emission[2]=.5f;
    for(int step=0;step<2;step++)
    {
        m.metalness=(float)step; StudioShade(&scene,&m,&v,&matrix,eye,diffuse,additive,metallic);
        for(int k=0;k<3;k++) CHECK(additive[k]==m.emission[k] && diffuse[k]==0 && metallic[k]==0);
    }
    strcpy(m.name,"Validation"); CHECK(StudioMaterialValid(&m));
    m.metalness=NAN; CHECK(!StudioMaterialValid(&m)); m.metalness=1.01f; CHECK(!StudioMaterialValid(&m));
    m.metalness=-.01f; CHECK(!StudioMaterialValid(&m)); m.metalness=0;
    m.emission[1]=INFINITY; CHECK(!StudioMaterialValid(&m)); m.emission[1]=-.1f; CHECK(!StudioMaterialValid(&m));
    m.emission[1]=1.1f; CHECK(!StudioMaterialValid(&m));
    puts("PASS: emission with all lights off and black vertex colors, 0/50/100% metalness, colored metallic highlights, retained intensity control and material validation.");
}

static void Json(void)
{
    const char text[]="{\"strings\":[\"a\\n\\u00e9\\ud83d\\udca1\",\"\\ud800\",\"\\u0000\"]}";
    GltfJsonToken *tokens=NULL; int count; char *decoded;
    CHECK(GltfJsonParse(text,strlen(text),&tokens,&count,&why));
    decoded=GltfJsonCopyString(text,&tokens[3]); CHECK(decoded && !strcmp(decoded,"a\n\xc3\xa9\xf0\x9f\x92\xa1")); free(decoded);
    CHECK(!GltfJsonCopyString(text,&tokens[4]) && !GltfJsonCopyString(text,&tokens[5])); free(tokens);
}

int main(int argc,char **argv)
{
    CHECK(argc==3); const char *project=argv[1]; StudioScene scene={0},loaded={0}; StudioSceneDefaultLighting(&scene);
    double position[3]={0}; char path[MAX_PATH],asset[MAX_PATH],source[MAX_PATH];
    Import(project); Json(); EmissionMetalness();
    lstrcpyn(scene.project,project,sizeof(scene.project)); lstrcpyn(scene.filename,"Main.rnd",sizeof(scene.filename));
    CHECK(StudioSceneAddModel(&scene,"Light.gltf",position,&why));
    position[2]=2; CHECK(StudioSceneAddModel(&scene,"Light.gltf",position,&why));
    CHECK(scene.objects[0].asset==scene.objects[1].asset && scene.objects[0].materials!=scene.objects[1].materials);
    StudioMaterial *m=&scene.objects[0].materials[1];
    CHECK(m->metalness==0 && m->emission[0]==0 && m->emission[1]==0 && m->emission[2]==0);
    m->base[0]=.125; m->base[1]=.625; m->specular[0]=.375; m->intensity=.875; m->shininess=87; m->emission[0]=.125f; m->emission[1]=.25f; m->emission[2]=.5f; m->metalness=.625f;
    lstrcpyn(m->image,"Paint.bmp",sizeof(m->image)); CHECK(StudioSceneSave(&scene,&why));
    Math(&scene);
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why) && !why[0] && loaded.count==2);
    CHECK(!memcmp(m,&loaded.objects[0].materials[1],sizeof(*m)));
    CHECK(loaded.objects[1].materials[1].base[0]==1 && NEAR(loaded.objects[1].transform.position[2],2));
    scene.objects[0].materials[1].shininess=9; test_fail_move=1;
    CHECK(!StudioSceneSave(&scene,&why));
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why) && loaded.objects[0].materials[1].shininess==87);
    scene.objects[0].materials[1].shininess=87;
    CHECK(!StudioSceneAddModel(&scene,"../Light.gltf",position,&why) && scene.count==2);
    m->intensity=NAN; CHECK(!StudioSceneSave(&scene,&why)); m->intensity=.875;
    snprintf(path,sizeof(path),"%s/studio/scenes/Bad.rnd",project);
    const char *bad[]={"{\"format\":\"GEditor Render Studio\",\"version\":6,\"objects\":[]}",
        "{\"format\":\"GEditor Render Studio\",\"version\":1,\"objects\":[{\"model\":\"../Light.gltf\"}]}",
        "{\"format\":\"GEditor Render Studio\",\"version\":1,\"objects\":[{\"model\":\"Light.gltf\",\"position\":[NaN,0,0],\"materials\":[]}]}",
        "{\"format\":\"GEditor Render Studio\",\"version\":1,\"objects\":[{\"model\":\"Light.gltf\",\"position\":[0,0,0],\"materials\":[{}]}]}"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++)
    {
        Write(path,bad[i]); StudioInstance *before=loaded.objects;
        CHECK(!StudioSceneLoad(project,"Bad.rnd",&loaded,&why) && loaded.objects==before && loaded.count==2);
    }
    for(int i=0;i<atoi(argv[2]);i++)
    {
        char name[64]; snprintf(name,sizeof(name),"BadMaterial%d.rnd",i); StudioScene before=loaded;
        CHECK(!StudioSceneLoad(project,name,&loaded,&why) && !memcmp(&before,&loaded,sizeof(loaded)));
    }
    for(int version=1;version<=4;version++)
    {
        char name[64]; snprintf(name,sizeof(name),"Legacy%d.rnd",version);
        CHECK(StudioSceneLoad(project,name,&loaded,&why) && loaded.count==1);
        StudioMaterial *legacy=&loaded.objects[0].materials[0];
        CHECK(legacy->metalness==0 && legacy->emission[0]==0 && legacy->emission[1]==0 && legacy->emission[2]==0);
        CHECK(legacy->base[0]==.25f && legacy->shininess==64 && StudioSceneSave(&loaded,&why));
    }
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why));
    StudioMaterial retained=*m; m->emission[0]=NAN; CHECK(!StudioSceneSave(&scene,&why)); *m=retained;
    m->metalness=2; CHECK(!StudioSceneSave(&scene,&why)); *m=retained;
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why) && !memcmp(m,&loaded.objects[0].materials[1],sizeof(*m)));
    snprintf(asset,sizeof(asset),"%s/studio/models/Light.gltf",project);
    CHECK(DeleteFile(asset));
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why) && why[0] && loaded.count==2 && !loaded.objects[0].asset);
    CHECK(!memcmp(m,&loaded.objects[0].materials[1],sizeof(*m)) && StudioSceneSave(&loaded,&why));
    snprintf(source,sizeof(source),"%s/studio/models/reordered.gltf",project); CHECK(CopyFile(source,asset,FALSE));
    CHECK(StudioSceneLoad(project,"Main.rnd",&loaded,&why) && loaded.objects[0].asset && !why[0]);
    CHECK(!strcmp(loaded.objects[0].materials[0].name,"Glass"));
    CHECK(!memcmp(m,&loaded.objects[0].materials[2],sizeof(*m)));
    snprintf(source,sizeof(source),"%s/studio/models/original.gltf",project); CHECK(CopyFile(source,asset,FALSE));
    Write(path,"{\"format\":\"GEditor Render Studio\",\"version\":1,\"objects\":[]}");
    CHECK(StudioSceneLoad(project,"Bad.rnd",&loaded,&why) && loaded.count==0);
    StudioSceneRemove(&scene,0); CHECK(scene.count==1 && NEAR(scene.objects[0].transform.position[2],2));
    StudioSceneFree(&scene); StudioSceneFree(&loaded);
    puts("PASS: independent materials, scene roundtrip, atomic-save failure, invalid-load rollback, missing assets, reordered slots and legacy empty scenes.");
    return 0;
}
