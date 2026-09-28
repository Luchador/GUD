#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "studiolightview.h"
#define NEAR(a,b) (fabs((a)-(b))<1e-6)
#include "icons.inc"

static void Drag(void)
{
    StudioLight light={.enabled=TRUE,.position={1,2,3},.direction={0,-7,0},.intensity=2,.radius=10,.inner=20,.outer=30,.color={1,1,1}};
    StudioTransform initial,changed; StudioLight result;
    StudioLightTransform(&light,&initial);
    assert(initial.position[1]==2 && initial.rotation[2]==0 && initial.scale[2]==1);
    OrbitCamera camera={.distance=20,.yaw=30,.pitch=-20}; StudioGizmoFrame frame; StudioDrag drag;
    assert(StudioGizmoPlace(&initial,&camera,900,600,STUDIO_TRANSLATE,&frame));
    for(int slot=0;slot<3;slot++) for(int axis=0;axis<3;axis++)
    {
        double hit[3],end[3],screen[2],target[2];
        for(int k=0;k<3;k++) { hit[k]=initial.position[k]+(k==axis ? frame.length*.8 : 0); end[k]=hit[k]+(k==axis ? 1 : 0); }
        assert(StudioProject(&camera,900,600,hit,screen) && StudioProject(&camera,900,600,end,target));
        assert(StudioDragBegin(&drag,&initial,&camera,900,600,screen[0],screen[1],&frame,axis,hit));
        assert(StudioDragUpdate(&drag,target[0],target[1],FALSE,&changed));
        assert(StudioLightDragApply(&light,&changed,slot,STUDIO_TRANSLATE,&result));
        for(int k=0;k<3;k++) assert(NEAR(result.position[k],initial.position[k]+(k==axis)));
        assert(!memcmp(result.direction,light.direction,sizeof(light.direction)) && result.radius==10 && result.outer==30);
    }
    camera.yaw=camera.pitch=0; initial.position[0]=initial.position[1]=initial.position[2]=0;
    light.position[0]=light.position[1]=light.position[2]=0;
    assert(StudioGizmoPlace(&initial,&camera,900,600,STUDIO_ROTATE,&frame));
    double hit[3]={frame.length,0,0},screen[2],target[2],end[3];
    assert(StudioProject(&camera,900,600,hit,screen));
    assert(StudioDragBegin(&drag,&initial,&camera,900,600,screen[0],screen[1],&frame,2,hit));
    for(int degree=1;degree<=360;degree++)
    {
        double radians=degree*3.141592653589793/180;
        end[0]=frame.length*cos(radians); end[1]=frame.length*sin(radians); end[2]=0;
        assert(StudioProject(&camera,900,600,end,target));
        assert(StudioDragUpdate(&drag,target[0],target[1],FALSE,&changed));
        assert(StudioLightDragApply(&light,&changed,0,STUDIO_ROTATE,&result));
        assert(NEAR(result.direction[0],7*sin(radians)) && NEAR(result.direction[1],-7*cos(radians)));
        assert(result.position[0]==0 && result.position[2]==0 && result.intensity==2);
    }
    assert(StudioDragBegin(&drag,&initial,&camera,900,600,screen[0],screen[1],&frame,2,hit));
    double radians=33*3.141592653589793/180; end[0]=frame.length*cos(radians); end[1]=frame.length*sin(radians);
    assert(StudioProject(&camera,900,600,end,target));
    assert(StudioDragUpdate(&drag,target[0],target[1],TRUE,&changed));
    assert(StudioLightDragApply(&light,&changed,0,STUDIO_ROTATE,&result) && NEAR(result.direction[0],3.5));
    StudioLight before=result;
    assert(!StudioLightDragApply(&light,&changed,1,STUDIO_ROTATE,&result));
    assert(!StudioLightDragApply(&light,&changed,0,STUDIO_SCALE,&result));
    assert(!memcmp(&before,&result,sizeof(result)));
    changed.position[0]=NAN; assert(!StudioLightDragApply(&light,&changed,0,STUDIO_TRANSLATE,&result));
    puts("PASS: all light XYZ translation, full-turn spotlight rotation, direction length, Ctrl snapping and unsupported-tool isolation.");
}

int main(void)
{
    Drag();
    StudioScene scene={0}; OrbitCamera camera={.distance=10};
    scene.lights[1].enabled=scene.lights[2].enabled=TRUE;
    scene.lights[2].position[2]=2;
    double rect[4],depth;
    assert(StudioLightIconRect(&scene.lights[1],&camera,800,600,rect,&depth));
    assert(NEAR(rect[0],384) && NEAR(rect[1],284) && NEAR(rect[2],416) && NEAR(rect[3],316) && NEAR(depth,10));
    assert(StudioLightIconPick(&scene,&camera,800,600,400,300)==STUDIO_LIGHT_SELECTION(2));
    assert(StudioLightIconPick(&scene,&camera,800,600,420,300)==-1);
    scene.lights[2].position[2]=0; assert(StudioLightIconPick(&scene,&camera,800,600,400,300)==STUDIO_LIGHT_SELECTION(2));
    scene.lights[2].enabled=FALSE; assert(StudioLightIconPick(&scene,&camera,800,600,400,300)==STUDIO_LIGHT_SELECTION(1));
    for(int zoom=1;zoom<10;zoom++)
    {
        camera.distance=zoom;
        assert(StudioLightIconRect(&scene.lights[1],&camera,400,900,rect,&depth));
        assert(NEAR(rect[2]-rect[0],32) && NEAR(rect[3]-rect[1],32) && NEAR((rect[0]+rect[2])*.5,200));
        assert(StudioLightIconPick(&scene,&camera,400,900,200,450)==STUDIO_LIGHT_SELECTION(1));
    }
    scene.lights[1].position[2]=20; assert(!StudioLightIconRect(&scene.lights[1],&camera,800,600,rect,&depth));
    assert(StudioLightIconPick(&scene,&camera,800,600,400,300)==-1);
    scene.lights[1].position[2]=0; scene.lights[1].position[0]=100;
    assert(!StudioLightIconRect(&scene.lights[1],&camera,800,600,rect,&depth));
    assert(!StudioLightIconRect(&scene.lights[1],&camera,0,0,rect,&depth));
    scene.lights[2].enabled=TRUE;
    for(int k=0;k<3;k++) { scene.lights[1].position[k]=1+k; scene.lights[2].position[k]=7+k; }
    double lower[3],upper[3];
    assert(!StudioBounds(&scene,-1,lower,upper) && StudioViewBounds(&scene,-1,lower,upper));
    for(int k=0;k<3;k++) assert(lower[k]==k && upper[k]==8+k);
    assert(StudioViewBounds(&scene,STUDIO_LIGHT_SELECTION(2),lower,upper));
    for(int k=0;k<3;k++) assert(lower[k]==6+k && upper[k]==8+k);
    assert(!StudioViewBounds(&scene,-5,lower,upper));
    assert(StudioSceneLightIndex(&scene,-99999)==-1 && StudioSceneLightIndex(&scene,99999)==-1);
    puts("PASS: icon projection, constant screen size, viewport/behind-camera clipping, overlap order, selection IDs and light framing.");
    return 0;
}
