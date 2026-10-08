#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WINGDIAPI extern
#define APIENTRY
#include <GL/gl.h>
#include "water.h"
#include "clouds.h"
#include "types.inc"
typedef void *HWND;
typedef struct { long long QuadPart; } LARGE_INTEGER;
typedef struct { GLuint name; int width,height; } ViewportTexture;
enum { VIEWPORT_RENDER_UNTEXTURED=3, BG_TEX_NONE=65535 };
typedef struct ViewportState {
    BOOL orbit; int width,height,rendermode;
    float posx,posy,posz,yaw,pitch,backgroundcolor[3];
    RomWater water;RomClouds clouds;RomSkyGradient skygradient;
    ViewportTexture watertexture,cloudtexture;
    LARGE_INTEGER waterfrequency,waterstart,cloudfrequency,cloudstart;
    void *hglrc,*hdc;
} ViewportState;
static ViewportState state;
static long long clockvalue;
static BOOL missing, pattern, timer;
static void QueryPerformanceCounter(LARGE_INTEGER *out) { out->QuadPart=clockvalue; }
static void QueryPerformanceFrequency(LARGE_INTEGER *out) { out->QuadPart=1000; }
static ViewportState *ViewportGetState(HWND hwnd) { return &state; }
static void wglMakeCurrent(void *dc,void *rc) {}
static void SetTimer(HWND hwnd,int id,int ms,void *callback) { timer=TRUE; }
static void KillTimer(HWND hwnd,int id) { timer=FALSE; }
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase) {}
BOOL TexLoadProjectImage(const char *dir,DWORD id,TexPixel *pixels,int *width,int *height)
{
    if(missing)return FALSE;
    *width=*height=32;
    for(int y=0;y<32;y++)for(int x=0;x<32;x++)
    {
        unsigned char c=pattern ? 110+60*sin(x*.4+y*.7)+35*cos(x*.8-y*.3) : 255;
        pixels[y*32+x]=(TexPixel){c,c,c,0}; /* Native alpha must be ignored. */
    }
    return TRUE;
}
#include "logic.inc"
static void Render(unsigned char pixels[64*64*3])
{
    glClearColor(.1f,.2f,.3f,1);glDepthMask(GL_TRUE);glClearDepth(.7);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    ViewportDrawWater(&state);
    assert(glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_TEXTURE_2D));
    assert(!glIsEnabled(GL_CLIP_PLANE0));
    GLboolean depthmask;GLint mode;float depth;
    glGetBooleanv(GL_DEPTH_WRITEMASK,&depthmask);glGetIntegerv(GL_MATRIX_MODE,&mode);
    assert(depthmask&&mode==GL_MODELVIEW);
    glReadPixels(32,10,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);assert(fabs(depth-.7)<1e-5);
    glReadPixels(0,0,64,64,GL_RGB,GL_UNSIGNED_BYTE,pixels);
    assert(glGetError()==GL_NO_ERROR);
}
static BOOL Background(const unsigned char *p)
{ return abs(p[0]-26)<=1&&abs(p[1]-51)<=1&&abs(p[2]-77)<=1; }
void CheckWaterPixels(void)
{
    unsigned char pixels[64*64*3],other[sizeof(pixels)];
    RomWater water={TRUE,0x05e4,-100,{120,100,80},0};
    state=(ViewportState){.width=64,.height=64,.backgroundcolor={.1f,.2f,.3f},.hglrc=(void *)1};
    glViewport(0,0,64,64);glPixelStorei(GL_PACK_ALIGNMENT,1);
    ViewportSetLevelWater(NULL,&water,"project");assert(state.water.enabled&&timer&&state.watertexture.name);
    /* Clearing clouds cannot stop water animation. */
    ViewportSetLevelClouds(NULL,NULL,NULL);assert(timer);
    Render(pixels);
    for(int y=33;y<64;y++)for(int x=0;x<64;x++)assert(Background(pixels+(y*64+x)*3));
    for(int y=0;y<25;y++)for(int x=0;x<64;x++)assert(!Background(pixels+(y*64+x)*3));
    /* No missing strip at either side; shifting the horizon moves its edge. */
    state.water.horizonoffset=30;Render(other);
    assert(!Background(other+(36*64+32)*3)&&Background(other+(44*64+32)*3));
    state.water.horizonoffset=0;state.pitch=60;Render(other);
    for(int i=0;i<64*64;i++)assert(Background(other+i*3));
    state.pitch=-60;Render(other);for(int i=0;i<64*64;i++)assert(!Background(other+i*3));
    state.pitch=0;state.posy=-101;Render(other);for(int i=0;i<64*64;i++)assert(Background(other+i*3));
    state.posy=0;
    /* Reloaded project texture, scrolling, camera movement and water height. */
    pattern=TRUE;ViewportSetLevelWater(NULL,&water,"project");Render(pixels);
    clockvalue=700;Render(other);assert(memcmp(pixels,other,sizeof(pixels)));
    clockvalue=0;state.posx=70;Render(other);assert(memcmp(pixels,other,sizeof(pixels)));state.posx=0;
    state.water.height=-400;Render(other);assert(memcmp(pixels,other,sizeof(pixels)));state.water.height=-100;
    /* Optional diagnostic image uses the same production drawing function. */
    if(getenv("WATER_PREVIEW_PPM"))
    {
        Render(other);FILE *f=fopen(getenv("WATER_PREVIEW_PPM"),"wb");assert(f);
        fprintf(f,"P6\n64 64\n255\n");for(int y=63;y>=0;y--)fwrite(other+y*64*3,1,64*3,f);fclose(f);
    }
    /* BG drawn afterwards covers the backdrop, regardless of water height. */
    glDisable(GL_TEXTURE_2D);glDisable(GL_CULL_FACE);glColor3f(1,0,0);
    glBegin(GL_QUADS);glVertex3f(-.5f,-.5f,-.5f);glVertex3f(.5f,-.5f,-.5f);
    glVertex3f(.5f,.5f,-.5f);glVertex3f(-.5f,.5f,-.5f);glEnd();
    unsigned char p[3];glReadPixels(32,25,1,1,GL_RGB,GL_UNSIGNED_BYTE,p);assert(p[0]==255&&p[1]==0&&p[2]==0);
    state.rendermode=VIEWPORT_RENDER_UNTEXTURED;Render(other);for(int i=0;i<64*64;i++)assert(Background(other+i*3));
    state.rendermode=0;state.orbit=TRUE;Render(other);for(int i=0;i<64*64;i++)assert(Background(other+i*3));state.orbit=FALSE;
    GLuint old=state.watertexture.name;missing=TRUE;ViewportSetLevelWater(NULL,&water,"project");
    assert(!state.water.enabled&&!state.watertexture.name&&!glIsTexture(old)&&!timer);
    missing=FALSE;ViewportSetLevelWater(NULL,&water,"project");old=state.watertexture.name;
    ViewportSetLevelWater(NULL,NULL,NULL);assert(!state.water.enabled&&!glIsTexture(old)&&!timer);
    assert(glGetError()==GL_NO_ERROR);
    puts("PASS: production water GL pixels, horizon/pitch, height/parallax/animation, BG coverage, depth/state restoration, modes, image reload and resource/timer cleanup.");
}
