#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define WINGDIAPI extern
#define APIENTRY
#include <GL/gl.h>
#include "clouds.h"
#include "../../../../src/game/skygradientmath.h"
#include "types.inc"
typedef void *HWND;
typedef struct {long long QuadPart;} LARGE_INTEGER;
typedef struct {GLuint name;int width,height;} ViewportTexture;
enum {VIEWPORT_RENDER_UNTEXTURED=3,BG_TEX_NONE=65535};
typedef struct ViewportState {
    BOOL orbit;int width,height,rendermode;
    float posx,posy,posz,yaw,pitch,backgroundcolor[3];
    RomClouds clouds;RomSkyGradient skygradient;
    ViewportTexture cloudtexture,watertexture;
    LARGE_INTEGER cloudfrequency,cloudstart;
    void *hglrc,*hdc;
} ViewportState;
static ViewportState state;
static unsigned char texel;
static void QueryPerformanceCounter(LARGE_INTEGER *out){out->QuadPart=0;}
static void QueryPerformanceFrequency(LARGE_INTEGER *out){out->QuadPart=1000;}
static ViewportState *ViewportGetState(HWND hwnd){return &state;}
static void wglMakeCurrent(void *dc,void *rc){}
static void SetTimer(HWND hwnd,int id,int ms,void *callback){}
static void KillTimer(HWND hwnd,int id){}
static void InvalidateRect(HWND hwnd,void *rect,BOOL erase){}
BOOL TexLoadProjectImage(const char *dir,DWORD id,TexPixel *pixels,int *width,int *height)
{
    *width=*height=32;
    for(int i=0;i<1024;i++)pixels[i]=(TexPixel){texel,texel,texel,0};
    return TRUE;
}
#include "logic.inc"
static void Render(unsigned char pixels[64*64*3],BOOL clouds)
{
    glClearColor(state.backgroundcolor[0],state.backgroundcolor[1],state.backgroundcolor[2],1);
    glDepthMask(GL_TRUE);glClearDepth(.7);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_TEXTURE_2D);glShadeModel(GL_FLAT);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    ViewportDrawSkyGradient(&state);if(clouds)ViewportDrawClouds(&state);
    assert(glIsEnabled(GL_DEPTH_TEST)&&glIsEnabled(GL_CULL_FACE)&&!glIsEnabled(GL_BLEND)&&!glIsEnabled(GL_TEXTURE_2D));
    GLboolean mask;GLint mode,shade;float depth;
    glGetBooleanv(GL_DEPTH_WRITEMASK,&mask);glGetIntegerv(GL_MATRIX_MODE,&mode);glGetIntegerv(GL_SHADE_MODEL,&shade);
    assert(mask&&mode==GL_MODELVIEW&&shade==GL_FLAT);
    glReadPixels(32,10,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&depth);assert(fabs(depth-.7)<1e-5);
    glReadPixels(0,0,64,64,GL_RGB,GL_UNSIGNED_BYTE,pixels);assert(glGetError()==GL_NO_ERROR);
}
void CheckSkyPixels(void)
{
    unsigned char pixels[64*64*3],other[sizeof(pixels)];
    RomSkyGradient gradient={TRUE,90,0,{16,48,96}};
    RomClouds clouds={TRUE,0x08b4,5000,{128,128,128},0};
    state=(ViewportState){.width=64,.height=64,.backgroundcolor={160/255.0f,192/255.0f,240/255.0f},.hglrc=(void *)1};
    glViewport(0,0,64,64);glPixelStorei(GL_PACK_ALIGNMENT,1);
    ViewportSetLevelSkyGradient(NULL,&gradient);Render(pixels,FALSE);
    /* Horizon and lower hemisphere retain the original color; elevation darkens. */
    for(int y=0;y<29;y++)for(int x=0;x<64;x++)assert(abs(pixels[(y*64+x)*3]-160)<=1);
    assert(pixels[(60*64+32)*3]<pixels[(40*64+32)*3]&&pixels[(40*64+32)*3]<160);
    state.posx=20000;state.posy=-2000;state.posz=-10000;Render(other,FALSE);assert(!memcmp(pixels,other,sizeof(pixels)));
    state.yaw=120;Render(other,FALSE);assert(!memcmp(pixels,other,sizeof(pixels)));
    state.pitch=90;Render(other,FALSE);assert(abs(other[(32*64+32)*3]-16)<=1);
    state.pitch=-90;Render(other,FALSE);for(int i=0;i<4096;i++)assert(abs(other[i*3]-160)<=1);
    state.pitch=0;gradient.endangle=15;ViewportSetLevelSkyGradient(NULL,&gradient);Render(other,FALSE);
    assert(abs(other[(60*64+32)*3]-16)<=1);
    gradient.endangle=90;gradient.horizonoffset=30;ViewportSetLevelSkyGradient(NULL,&gradient);Render(other,FALSE);
    assert(other[(40*64+32)*3]>pixels[(40*64+32)*3]);
    gradient.horizonoffset=0;ViewportSetLevelSkyGradient(NULL,&gradient);
    state.posx=state.posy=state.posz=0;state.yaw=0;
    texel=0;ViewportSetLevelClouds(NULL,&clouds,"project");Render(other,TRUE);
    assert(!memcmp(pixels,other,sizeof(pixels))); /* Zero-intensity clouds cannot replace gradient with a flat color. */
    texel=255;ViewportSetLevelClouds(NULL,&clouds,"project");Render(other,TRUE);
    assert(other[(60*64+32)*3]>pixels[(60*64+32)*3]);
    for(int i=0;i<4096;i++)assert(other[i*3]>=pixels[i*3]);
    if(getenv("SKY_GRADIENT_PPM"))
    {
        FILE *f=fopen(getenv("SKY_GRADIENT_PPM"),"wb");assert(f);
        fprintf(f,"P6\n64 64\n255\n");for(int y=63;y>=0;y--)fwrite(pixels+y*64*3,1,64*3,f);fclose(f);
    }
    /* Ordinary BG geometry still covers the sky; the sky has no depth writes. */
    glDisable(GL_CULL_FACE);glColor3f(1,0,0);glBegin(GL_QUADS);
    glVertex3f(-1,-1,0);glVertex3f(1,-1,0);glVertex3f(1,1,0);glVertex3f(-1,1,0);glEnd();
    unsigned char pixel[3];glReadPixels(32,32,1,1,GL_RGB,GL_UNSIGNED_BYTE,pixel);assert(pixel[0]==255&&!pixel[1]&&!pixel[2]);
    ViewportSetLevelSkyGradient(NULL,NULL);ViewportSetLevelClouds(NULL,NULL,NULL);Render(other,FALSE);
    for(int i=0;i<4096;i++)assert(abs(other[i*3]-160)<=1);
    assert(!glGetError());
    puts("PASS: real OpenGL gradient/cloud pixels, horizon, zenith, end angle, world direction, translation independence, black/bright clouds, BG occlusion and GL state/depth restoration.");
}
