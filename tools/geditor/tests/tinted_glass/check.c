#include <assert.h>
#include <math.h>
#include <stdio.h>
/* Also accept the public-domain MinGW GL header on Linux. */
#define WINGDIAPI extern
#define APIENTRY
#include <GL/gl.h>
#include "bgrender.h"
#include "types.inc"
typedef struct { ViewportRenderMode rendermode; } ViewportState;
typedef struct { GLuint gltex; BgRenderFlags renderflags; } SceneBatch;
#include "logic.inc"

static float vertices[]={-1,-1,0, 3,-1,0, -1,3,0};
static unsigned char colors[12];
static const float uv[]={.5,.5,.5,.5,.5,.5};
static const float background[]={.1f,.2f,.3f};
static void Pixel(const float expected[3])
{
    unsigned char pixel[4];
    glReadPixels(32,32,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    for(int a=0;a<3;a++)
    {
        if(fabs(pixel[a]-expected[a]*255)>2.1)
            fprintf(stderr,"Channel %d: got %u, expected %.3f\n",a,pixel[a],expected[a]*255);
        assert(fabs(pixel[a]-expected[a]*255)<=2.1);
    }
    assert(glGetError()==GL_NO_ERROR);
}
static void Clear(void)
{
    glDepthMask(GL_TRUE); glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(background[0],background[1],background[2],1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
}
static void Texture(int alpha)
{
    unsigned char pixel[]={128,192,255,(unsigned char)alpha};
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
}
static void Colors(int alpha)
{
    for(int v=0;v<3;v++)
    { colors[v*4]=204; colors[v*4+1]=153; colors[v*4+2]=102; colors[v*4+3]=alpha; }
}
void CheckGlassPixels(void)
{
    GLuint texture; GLint alphaBits;
    glGetIntegerv(GL_ALPHA_BITS,&alphaBits); assert(alphaBits>=8);
    glViewport(0,0,64,64); glDisable(GL_DITHER);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3,GL_FLOAT,0,vertices); glColorPointer(4,GL_UNSIGNED_BYTE,0,colors);
    glTexCoordPointer(2,GL_FLOAT,0,uv);
    glGenTextures(1,&texture); glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    ViewportState state={VIEWPORT_RENDER_NORMAL};
    SceneBatch batch={texture,BG_RENDER_DEPTH_TEST|BG_RENDER_BLEND};
    const int textureAlphas[]={0,80,255}, shadeAlphas[]={0,96,255}, tints[]={0,64,200,255};
    for(int mode=0;mode<3;mode++) for(int t=0;t<3;t++) for(int s=0;s<3;s++) for(int n=0;n<4;n++)
    {
        state.rendermode=mode==2 ? VIEWPORT_RENDER_UNTEXTURED : VIEWPORT_RENDER_NORMAL;
        batch.renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_BLEND|(mode==1 ? BG_RENDER_ALPHA_TEST : 0);
        Clear(); Texture(textureAlphas[t]); Colors(shadeAlphas[s]);
        ViewportApplyRenderFlags(batch.renderflags);
        ViewportDrawGlassTriangle(&state,&batch,0,tints[n]);
        float alpha=fminf(1,shadeAlphas[s]/255.f*(mode==2 ? 1 : textureAlphas[t]/255.f)+tints[n]/255.f);
        if(alpha<=(mode==1 ? VIEWPORT_CUTOUT_ALPHA_THRESHOLD : VIEWPORT_BLEND_ALPHA_THRESHOLD)) alpha=0;
        float expected[3]; const float tex[]={128/255.f,192/255.f,1};
        for(int a=0;a<3;a++) expected[a]=colors[a]/255.f*(mode==2 ? 1 : tex[a])*alpha+background[a]*(1-alpha);
        Pixel(expected);
    }
    /* Two overlapping panes must each use their own saturated alpha. */
    Clear(); state.rendermode=VIEWPORT_RENDER_NORMAL;
    batch.renderflags=BG_RENDER_DEPTH_TEST|BG_RENDER_BLEND;
    Texture(0); Colors(128); ViewportApplyRenderFlags(batch.renderflags);
    ViewportDrawGlassTriangle(&state,&batch,0,64);
    ViewportDrawGlassTriangle(&state,&batch,0,128);
    float expected[3], tex[]={128/255.f,192/255.f,1};
    for(int a=0;a<3;a++)
    {
        float rgb=colors[a]/255.f*tex[a];
        expected[a]=(rgb*(64/255.f)+background[a]*(1-64/255.f))*(1-128/255.f)+rgb*(128/255.f);
    }
    Pixel(expected);
    /* Gradient shade alpha is interpolated before the saturated tint is added. */
    Clear(); Texture(255); Colors(0); colors[7]=255; colors[11]=128;
    ViewportApplyRenderFlags(batch.renderflags); ViewportDrawGlassTriangle(&state,&batch,0,64);
    float gradient=(32.5f/128)*(1+128/255.f)+64/255.f;
    for(int a=0;a<3;a++) expected[a]=colors[a]/255.f*tex[a]*gradient+background[a]*(1-gradient);
    Pixel(expected);
    /* A nearer opaque triangle must block all three glass passes. */
    Clear(); glDisable(GL_TEXTURE_2D); Colors(255);
    ViewportApplyRenderFlags(BG_RENDER_DEPTH_TEST|BG_RENDER_DEPTH_WRITE);
    glDrawArrays(GL_TRIANGLES,0,3);
    for(int v=0;v<3;v++) vertices[v*3+2]=.5;
    Colors(0); Texture(0); ViewportApplyRenderFlags(batch.renderflags);
    ViewportDrawGlassTriangle(&state,&batch,0,255);
    for(int a=0;a<3;a++) expected[a]=colors[a]/255.f;
    Pixel(expected);
    glDeleteTextures(1,&texture);
    puts("PASS: real GL pixels for transparent texels, shade alpha, tint saturation, cutout, untextured view, overlapping panes, gradients and occlusion.");
}
