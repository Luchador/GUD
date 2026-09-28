#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studiocamera.h"
BOOL TexGetProjectImageSize(const char *project,DWORD id,int *w,int *h) { return FALSE; }
#define NEAR(a,b) (fabs((a)-(b))<1e-6)

static void Projection(void)
{
    StudioCamera camera=g_StudioDefaultCamera;
    const double rotations[][3]={{0,0,0},{0,90,0},{90,0,0},{-20,35,17}};
    camera.transform.position[0]=4; camera.transform.position[1]=5; camera.transform.position[2]=6;
    for(unsigned i=0;i<sizeof(rotations)/sizeof(*rotations);i++)
    {
        double matrix[16],view[3]; memcpy(camera.transform.rotation,rotations[i],sizeof(rotations[i]));
        StudioCameraView(&camera,matrix,view); Rotation r; RotationEuler(&r,rotations[i]);
        /* Parallel projection: points at different depths have identical X/Y. */
        for(int depth=1;depth<=100;depth+=33)
        {
            double local[3]={2,-3,-depth},world[3],result[3]; RotationVector(&r,local,world);
            for(int k=0;k<3;k++) world[k]+=camera.transform.position[k];
            for(int k=0;k<3;k++) {
                result[k]=matrix[12+k];
                for(int j=0;j<3;j++) result[k]+=matrix[j*4+k]*world[j];
                assert(NEAR(result[k],local[k]));
            }
            assert(NEAR(view[0]*view[0]+view[1]*view[1]+view[2]*view[2],1));
        }
    }
    StudioPreviewRect rect;
    const int sizes[][2]={{800,600},{1920,1080},{300,600},{600,300},{272,272}};
    for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++) {
        assert(StudioCameraPreviewRect(sizes[i][0],sizes[i][1],&rect));
        assert(rect.right-rect.left==256 && rect.bottom-rect.top==256);
        assert(rect.right==sizes[i][0]-8 && rect.bottom==sizes[i][1]-8 && rect.left>=8 && rect.top>=8);
    }
    assert(!StudioCameraPreviewRect(271,600,&rect) && !StudioCameraPreviewRect(800,271,&rect));
    const StudioRenderSettings dimensions[]={{64,64},{128,64},{64,128},{255,1},{1,255}};
    for (unsigned i=0;i<sizeof(dimensions)/sizeof(*dimensions);i++) {
        StudioPreviewRect image={0,0,256,256}; StudioCameraImageRect(&dimensions[i],&image);
        assert(image.left>=0 && image.top>=0 && image.right<=256 && image.bottom<=256);
        assert(image.right>image.left && image.bottom>image.top);
        if (i==1) assert(image.left==0 && image.top==64 && image.right==256 && image.bottom==192);
        if (i==2) assert(image.left==64 && image.top==0 && image.right==192 && image.bottom==256);
    }
    StudioModel asset={0}; StudioInstance instance={0}; StudioScene scene={0};
    asset.lower[0]=asset.lower[1]=asset.lower[2]=-1; asset.upper[0]=asset.upper[1]=asset.upper[2]=1;
    instance.asset=&asset; instance.transform=(StudioTransform){{0},{0},{1,1,1}};
    scene.objects=&instance; scene.count=1; scene.camera=(StudioCamera){{{0,0,10},{0},{1,1,1}},4};
    double nearz,farz; StudioCameraClip(&scene,&nearz,&farz); assert(nearz<9 && nearz>0 && farz>11);
    instance.transform.position[2]=20; StudioCameraClip(&scene,&nearz,&farz); assert(nearz>0 && farz>nearz);
    scene.count=0; strcpy(scene.filename,"Camera.rnd"); double lower[3],upper[3];
    assert(!StudioViewBounds(&scene,-1,lower,upper));
    assert(StudioViewBounds(&scene,STUDIO_SELECT_CAMERA,lower,upper) && lower[2]==9 && upper[2]==11);
    puts("PASS: orthographic view orientation/roll/depth invariance, exact inset size/anchoring, narrow-window handling and front/behind clipping.");
}

static void ParallelShading(void)
{
    StudioScene scene={0}; StudioSceneDefaultLighting(&scene); scene.ambient.intensity=0;
    scene.directional.direction[0]=scene.directional.direction[1]=0; scene.directional.direction[2]=-1;
    StudioMaterial m={.base={1,1,1},.specular={1,1,1},.intensity=1,.shininess=32};
    StudioTransform t={{0},{0},{1,1,1}}; StudioMatrix matrix; StudioMatrixBuild(&t,&matrix);
    BgVertex vertices[3]={0}; const double eye[3]={0,0,10},view[3]={0,0,1}; double uv[3][2];
    for(int i=0;i<3;i++) {
        vertices[i].x=(i-1)*5; vertices[i].z=i; vertices[i].r=vertices[i].g=vertices[i].b=255;
        vertices[i].environment.normal[2]=1; float diffuse[3],additive[3],metallic[3];
        StudioShadeView(&scene,&m,&vertices[i],&matrix,view,TRUE,diffuse,additive,metallic);
        assert(NEAR(additive[0],1) && NEAR(diffuse[0],.8));
        StudioShadeView(&scene,&m,&vertices[i],&matrix,eye,FALSE,diffuse,additive,metallic);
        if(i!=1) assert(additive[0]<.1);
    }
    StudioEnvironmentCoordinatesView(vertices,&matrix,view,TRUE,uv);
    for(int i=0;i<3;i++) assert(NEAR(uv[i][0],.5) && NEAR(uv[i][1],.5));
    StudioEnvironmentCoordinatesView(vertices,&matrix,eye,FALSE,uv); assert(fabs(uv[0][0]-uv[2][0])>.1);
    puts("PASS: parallel specular highlights and reflection directions remain constant across a flat plane; perspective behavior retained.");
}

int main(int argc,char **argv)
{
    assert(argc==3); Projection(); ParallelShading();
    StudioScene scene={0},loaded={0}; StudioSceneDefaultLighting(&scene); scene.camera=g_StudioDefaultCamera; scene.render=g_StudioDefaultRender; const char *why="";
    snprintf(scene.project,sizeof(scene.project),"%s",argv[1]); strcpy(scene.filename,"Camera.rnd");
    scene.camera=(StudioCamera){{{1.25,-2.5,8},{-12,35,17},{1,1,1}},3.125};
    scene.render=(StudioRenderSettings){127,63};
    assert(StudioSceneSave(&scene,&why) && StudioSceneLoad(argv[1],"Camera.rnd",&loaded,&why));
    assert(!memcmp(&scene.camera,&loaded.camera,sizeof(StudioCamera)));
    assert(loaded.render.width==127 && loaded.render.height==63);
    const StudioRenderSettings invalidsizes[]={{0,64},{64,0},{256,1},{1,256},{-1,64}};
    for(unsigned i=0;i<sizeof(invalidsizes)/sizeof(*invalidsizes);i++) {
        scene.render=invalidsizes[i]; assert(!StudioSceneSave(&scene,&why));
    }
    scene.render=(StudioRenderSettings){127,63};
    StudioCamera retained=scene.camera;
    double invalid[]={0,-1,1e10,NAN,INFINITY};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
        scene.camera.size=invalid[i]; assert(!StudioSceneSave(&scene,&why)); scene.camera=retained;
    }
    scene.camera.transform.scale[1]=2; assert(!StudioSceneSave(&scene,&why)); scene.camera=retained;
    for(int i=0;i<atoi(argv[2]);i++) {
        char file[64]; snprintf(file,sizeof(file),"BadCamera%d.rnd",i); StudioScene before=loaded;
        assert(!StudioSceneLoad(argv[1],file,&loaded,&why) && !memcmp(&before,&loaded,sizeof(loaded)));
    }
    for(int version=1;version<=8;version++) {
        char file[64]; snprintf(file,sizeof(file),"Legacy%d.rnd",version);
        assert(StudioSceneLoad(argv[1],file,&loaded,&why));
        assert(!memcmp(&loaded.camera,&g_StudioDefaultCamera,sizeof(StudioCamera)));
        assert(loaded.render.width==64 && loaded.render.height==64);
        assert(StudioSceneSave(&loaded,&why));
    }
    StudioSceneFree(&loaded); StudioSceneFree(&scene);
    puts("PASS: camera scene roundtrip, invalid setting/load rollback, and v1-v8 camera/render migration, output dimensions and malformed settings.");
}
