/* Exercise the production drag completion/cancellation functions with reentrant
 * capture-loss notifications, as Win32 sends when capture is released. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "studiogizmo.h"
typedef intptr_t HWND,LPARAM,LONG_PTR;
typedef uintptr_t WPARAM;
#define GWLP_USERDATA 0
#define STUDIO_WM_TRANSFORM 141
#define STUDIO_WM_LIGHT_TRANSFORM 142
static HWND capture=2;
typedef struct {
    BOOL transforming;
    unsigned buttons;
    int hover,dragobject;
    StudioDrag drag;
    StudioLight lightbefore;
    StudioScene *scene;
} StudioViewport;
static StudioViewport state;
static int notifications,lastphase;
static BOOL cancel_save;
BOOL StudioViewportCancelTransform(HWND hwnd);
static LONG_PTR GetWindowLongPtr(HWND hwnd,int id) { return hwnd ? (LONG_PTR)&state : 0; }
static HWND GetParent(HWND hwnd) { return 1; }
static HWND GetCapture(void) { return capture; }
static void ReleaseCapture(void)
{ capture=0; assert(!StudioViewportCancelTransform(2)); }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
static void SendMessage(HWND hwnd,int message,WPARAM phase,LPARAM previous)
{
    assert(!state.transforming && !state.buttons && capture!=2);
    assert((message==STUDIO_WM_TRANSFORM || message==STUDIO_WM_LIGHT_TRANSFORM) && previous);
    lastphase=phase; notifications++;
    if (phase==1 && cancel_save)
    {
        if (message==STUDIO_WM_LIGHT_TRANSFORM) { state.scene->lights[-2-state.dragobject]=*(StudioLight *)previous; }
        else { state.scene->objects[state.dragobject].transform=*(StudioTransform *)previous; }
    }
}
#include "transaction.inc"
static void Begin(void)
{
    state.transforming=TRUE; state.buttons=1; state.hover=2; capture=2;
    state.drag.before=state.scene->objects[0].transform;
    state.scene->objects[0].transform.position[0]+=10;
    state.scene->objects[0].transform.rotation[1]+=30; state.scene->objects[0].transform.scale[2]*=2;
}
int main(void)
{
    StudioInstance object={0}; object.transform=(StudioTransform){{1,2,3},{10,20,30},{1,2,3}};
    StudioScene scene={0}; scene.objects=&object; scene.count=1; state.scene=&scene;
    Begin(); assert(StudioViewportCancelTransform(2)); assert(lastphase==2 && notifications==1 && object.transform.position[0]==1);
    assert(!memcmp(&object.transform,&state.drag.before,sizeof(object.transform)));
    assert(!StudioViewportCancelTransform(2) && notifications==1);
    Begin(); StudioViewportCommitTransform(2); assert(lastphase==1 && notifications==2 && object.transform.position[0]==11);
    Begin(); cancel_save=TRUE; StudioViewportCommitTransform(2); assert(notifications==3 && object.transform.position[0]==11);
    Begin(); capture=9; assert(StudioViewportCancelTransform(2)); assert(capture==9 && object.transform.position[0]==11);
    assert(!memcmp(&object.transform,&state.drag.before,sizeof(object.transform)));
    assert(!StudioViewportCancelTransform(0));
    for (int slot=0;slot<STUDIO_LIGHT_COUNT;slot++)
    {
        scene.lights[slot]=(StudioLight){.enabled=TRUE,.position={1,2,3},.direction={0,-7,0},.intensity=2,.radius=5};
        state.dragobject=STUDIO_LIGHT_SELECTION(slot); state.lightbefore=scene.lights[slot];
        scene.lights[slot].position[0]=99; scene.lights[slot].direction[2]=4;
        state.transforming=TRUE; state.buttons=1; capture=2;
        assert(StudioViewportCancelTransform(2) && !memcmp(&scene.lights[slot],&state.lightbefore,sizeof(StudioLight)));
        state.transforming=TRUE; state.buttons=1; capture=2; scene.lights[slot].position[0]=99;
        cancel_save=TRUE; StudioViewportCommitTransform(2);
        assert(!memcmp(&scene.lights[slot],&state.lightbefore,sizeof(StudioLight)));
        state.transforming=TRUE; state.buttons=1; capture=2; scene.lights[slot].position[0]=42;
        cancel_save=FALSE; StudioViewportCommitTransform(2); assert(scene.lights[slot].position[0]==42);
    }
    puts("PASS: model/light drag commit, full-transform cancellation, failed-save rollback and reentrant capture release.");
    return 0;
}
