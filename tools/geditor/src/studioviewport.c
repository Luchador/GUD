/* Studio scene preview, independent of the game's display-list renderer. */
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "studioviewport.h"
#include "orbitcamera.h"
#include "studiomath.h"
#include "editorpath.h"
#include "studiogizmo.h"
#include "studiolightview.h"
#include "studioenvironment.h"
#include "studiocameraview.h"

#define STUDIO_VIEWPORT_CLASS "GEditorStudioViewport"

typedef struct StudioTexture {
    char name[MAX_PATH]; GLuint id; BOOL environment; int width,height,blur;
    TexPixel *pixels; /* Retained only for the sharp environment, shared by blur variants. */
    struct StudioTexture *next;
} StudioTexture;

typedef struct StudioViewport {
    HDC dc;
    HGLRC context;
    OrbitCamera camera;
    POINT mouse;
    POINT press;
    BOOL moved;
    unsigned buttons;
    StudioScene *scene;
    StudioGizmo gizmo;
    StudioDrag drag;
    StudioLight lightbefore;
    StudioLightIcons lighticons;
    StudioCameraModel cameramodel;
    BOOL transforming;
    int tool,hover,dragobject;
    int selected;
    StudioTexture *textures;
} StudioViewport;

static void StudioViewportEndTransform(HWND hwnd,StudioViewport *state,BOOL commit)
{
    if (!state->transforming) { return; }
    StudioTransform before=state->drag.before; StudioLight lightbefore=state->lightbefore;
    int slot=StudioSceneLightIndex(state->scene,state->dragobject);
    state->transforming=FALSE; state->buttons=0; state->hover=-1;
    if (!commit && state->scene && state->dragobject>=0 && (DWORD)state->dragobject<state->scene->count)
        state->scene->objects[state->dragobject].transform=before;
    if (!commit && state->scene && state->dragobject==STUDIO_SELECT_CAMERA) { state->scene->camera.transform=before; }
    if (!commit && slot>=0) { state->scene->lights[slot]=lightbefore; }
    if (GetCapture()==hwnd) { ReleaseCapture(); }
    SendMessage(GetParent(hwnd),slot>=0 ? STUDIO_WM_LIGHT_TRANSFORM : STUDIO_WM_TRANSFORM,
        commit ? 1 : 2,slot>=0 ? (LPARAM)&lightbefore : (LPARAM)&before);
    InvalidateRect(hwnd,NULL,FALSE);
}

BOOL StudioViewportCancelTransform(HWND viewport)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    if (!state || !state->transforming) { return FALSE; }
    StudioViewportEndTransform(viewport,state,FALSE); return TRUE;
}

void StudioViewportCommitTransform(HWND viewport)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    if (state) { StudioViewportEndTransform(viewport,state,TRUE); }
}

void StudioViewportSetTool(HWND viewport,int tool)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    if (state && tool>=STUDIO_TRANSLATE && tool<=STUDIO_SCALE)
    { StudioViewportCancelTransform(viewport); state->tool=tool; state->hover=-1; InvalidateRect(viewport,NULL,FALSE); }
}

static BOOL StudioViewportTransform(StudioViewport *state,StudioTransform *transform)
{
    if (state->scene && state->scene->filename[0] && state->selected==STUDIO_SELECT_CAMERA)
    { if (state->tool==STUDIO_SCALE) { return FALSE; } *transform=state->scene->camera.transform; return TRUE; }
    int slot=StudioSceneLightIndex(state->scene,state->selected);
    if (slot>=0)
    {
        if (!StudioLightToolAllowed(slot,state->tool)) { return FALSE; }
        StudioLightTransform(&state->scene->lights[slot],transform); return TRUE;
    }
    if (!state->scene || state->selected<0 || (DWORD)state->selected>=state->scene->count
        || !state->scene->objects[state->selected].asset) { return FALSE; }
    *transform=state->scene->objects[state->selected].transform; return TRUE;
}

static BOOL StudioViewportGizmoFrame(StudioViewport *state,int width,int height,StudioGizmoFrame *frame)
{
    StudioTransform transform;
    return StudioViewportTransform(state,&transform) && StudioGizmoPlace(&transform,&state->camera,width,height,state->tool,frame);
}

static BOOL StudioViewportOverPreview(HWND hwnd,StudioViewport *state,int x,int y)
{
    RECT client; StudioPreviewRect preview; GetClientRect(hwnd,&client);
    if (!state->scene || !state->scene->filename[0] || !StudioCameraPreviewRect(client.right,client.bottom,&preview)) { return FALSE; }
    return x>=preview.left-1 && x<=preview.right && y>=preview.top-1 && y<=preview.bottom;
}

static int StudioViewportGizmoHit(HWND hwnd,StudioViewport *state,int x,int y,StudioGizmoFrame *frame,double hit[3])
{
    RECT client; double eye[3],direction[3]; GetClientRect(hwnd,&client);
    if (StudioViewportOverPreview(hwnd,state,x,y) || !StudioViewportGizmoFrame(state,client.right,client.bottom,frame)
        || !StudioRay(&state->camera,client.right,client.bottom,x,y,eye,direction)) { return -1; }
    return StudioGizmoPick(&state->gizmo,frame,eye,direction,hit);
}

static void StudioViewportTextures(StudioViewport *state)
{
    while (state->textures)
    {
        StudioTexture *texture=state->textures; state->textures=texture->next;
        if (texture->id) { glDeleteTextures(1,&texture->id); } free(texture->pixels); free(texture);
    }
}

void StudioViewportRefreshImages(HWND viewport)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    HDC dc=wglGetCurrentDC(); HGLRC context=wglGetCurrentContext();
    if (!state) { return; }
    if (wglMakeCurrent(state->dc,state->context)) { StudioViewportTextures(state); wglMakeCurrent(dc,context); }
    InvalidateRect(viewport,NULL,FALSE);
}

static void StudioViewportFrame(HWND viewport, StudioViewport *state, int selected)
{
    double lower[3],upper[3]; RECT client;
    if (!StudioViewBounds(state->scene,selected,lower,upper)) { return; }
    GetClientRect(viewport,&client);
    OrbitCameraFrame(&state->camera,lower,upper,client.bottom>0 ? (double)client.right/client.bottom : 1,STUDIO_FOV);
    InvalidateRect(viewport,NULL,FALSE);
}

void StudioViewportSetScene(HWND viewport, StudioScene *scene, BOOL frame)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    double lower[3],upper[3];
    if (!state) { return; } state->scene=scene;
    if (frame) { StudioViewportReset(viewport); StudioViewportFrame(viewport,state,-1); }
    else if (StudioViewBounds(scene,-1,lower,upper))
    {
        double squared=0;
        for (int k=0;k<3;k++) { state->camera.boundscenter[k]=(lower[k]+upper[k])*0.5; squared+=(upper[k]-lower[k])*(upper[k]-lower[k])*0.25; }
        state->camera.radius=fmax(0.001,sqrt(squared));
    }
    InvalidateRect(viewport,NULL,FALSE);
}

void StudioViewportSelect(HWND viewport, int index)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    if (state) { StudioViewportCancelTransform(viewport); state->selected=index; state->hover=-1; InvalidateRect(viewport,NULL,FALSE); }
}

BOOL StudioViewportDropPoint(HWND viewport, POINT screen, double position[3])
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    RECT client; double origin[3],direction[3],t;
    if (!state) { return FALSE; }
    ScreenToClient(viewport,&screen); GetClientRect(viewport,&client);
    if (StudioViewportOverPreview(viewport,state,screen.x,screen.y) || !PtInRect(&client,screen) || !StudioRay(&state->camera,client.right,client.bottom,screen.x,screen.y,origin,direction)) { return FALSE; }
    t=fabs(direction[1])>1e-8 ? -origin[1]/direction[1] : -1;
    if (t<0 || t>state->camera.distance*100) { t=state->camera.distance; }
    for (int k=0;k<3;k++) { position[k]=origin[k]+direction[k]*t; }
    position[1]=0; return TRUE;
}

void StudioViewportReset(HWND viewport)
{
    StudioViewport *state = (StudioViewport *)GetWindowLongPtr(viewport, GWLP_USERDATA);
    const double lower[3] = {-10, 0, -10}, upper[3] = {10, 0, 10};
    if (!state) { return; }
    StudioViewportCancelTransform(viewport);
    state->buttons = 0; state->hover=-1;
    state->selected = -1;
    if (GetCapture() == viewport) { ReleaseCapture(); }
    OrbitCameraFrame(&state->camera, lower, upper, 1, STUDIO_FOV);
    state->camera.pitch = -25;
    InvalidateRect(viewport, NULL, FALSE);
}

static BOOL StudioViewportInit(HWND hwnd, StudioViewport *state)
{
    PIXELFORMATDESCRIPTOR format = {0};
    HDC previousdc = wglGetCurrentDC();
    HGLRC previous = wglGetCurrentContext();
    int index;
    state->dc = GetDC(hwnd);
    if (!state->dc) { return FALSE; }
    format.nSize = sizeof(format); format.nVersion = 1;
    format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    format.iPixelType = PFD_TYPE_RGBA; format.cColorBits = 32;
    format.cDepthBits = 24; format.iLayerType = PFD_MAIN_PLANE;
    index = ChoosePixelFormat(state->dc, &format);
    if (!index || !SetPixelFormat(state->dc, index, &format)) { return FALSE; }
    state->context = wglCreateContext(state->dc);
    if (!state->context || !wglMakeCurrent(state->dc, state->context)) { return FALSE; }
    BOOL loaded=StudioLightIconsLoad(&state->lighticons,(HINSTANCE)GetWindowLongPtr(hwnd,GWLP_HINSTANCE));
    wglMakeCurrent(previousdc, previous);
    return loaded;
}

static StudioTexture *StudioViewportTexture(StudioViewport *state, const char *filename,BOOL environment,int blur)
{
    if (!filename[0] || !state->scene) { return NULL; }
    StudioTexture *texture; char folder[MAX_PATH],path[MAX_PATH]; TexPixel *pixels=NULL; int w,h; GLint maximum;
    for (texture=state->textures;texture;texture=texture->next) if (texture->environment==environment && texture->blur==blur && !lstrcmpi(filename,texture->name)) { return texture; }
    texture=calloc(1,sizeof(*texture)); if (!texture) { return NULL; }
    texture->environment=environment; texture->blur=blur;
    lstrcpyn(texture->name,filename,sizeof(texture->name)); texture->next=state->textures; state->textures=texture;
    if (environment && blur)
    {
        StudioTexture *source=StudioViewportTexture(state,filename,TRUE,0);
        if (!source || !source->pixels || !StudioEnvironmentBlur(source->pixels,source->width,source->height,blur,&pixels,&w,&h)) { return texture; }
    }
    else
    {
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum); maximum=min(4096,maximum);
        if (!EditorPathJoin(folder,sizeof(folder),state->scene->project,"studio\\images")
            || !EditorPathJoin(path,sizeof(path),folder,filename)
            || !(environment ? TexLoadStudioEnvironment(path,maximum,&pixels,&w,&h)
                : TexLoadStudioTexture(path,maximum,&pixels,&w,&h))) { return texture; }
    }
    texture->width=w; texture->height=h;
    glGenTextures(1,&texture->id); glBindTexture(GL_TEXTURE_2D,texture->id);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,environment ? GL_CLAMP : GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    if (environment && !blur) { texture->pixels=pixels; } else { free(pixels); }
    return texture;
}

static void StudioViewportObjects(StudioViewport *state, const double eye[3],BOOL parallel)
{
    if (!state->scene) { return; }
    glShadeModel(GL_SMOOTH);
    StudioTexture *environment=StudioViewportTexture(state,state->scene->environment,TRUE,0);
    BOOL reflect=FALSE;
    if (environment && environment->id) for (DWORD i=0;i<state->scene->count;i++)
        for (DWORD j=0;j<state->scene->objects[i].materialcount;j++)
            if (state->scene->objects[i].materials[j].metalness>0) { reflect=TRUE; }
    enum { DIFFUSE, ADDITIVE, METALLIC, ENVIRONMENT, BASE_IMAGE };
    const int reflected[]={DIFFUSE,METALLIC,ENVIRONMENT,BASE_IMAGE,ADDITIVE};
    /* A base-image multiplication pass tints both lighting and reflections without
     * multitexturing. Emission/dielectric highlights are added afterward. With no
     * usable environment, retain the original three-pass rendering exactly. */
    for (int pass=0;pass<(reflect ? 5 : 3);pass++)
    {
        int kind=reflect ? reflected[pass] : pass;
        if (pass)
        {
            glEnable(GL_BLEND); glDepthMask(GL_FALSE); glDepthFunc(GL_EQUAL);
            glBlendFunc(kind==BASE_IMAGE ? GL_ZERO : GL_ONE,kind==BASE_IMAGE ? GL_SRC_COLOR : GL_ONE);
        }
        for (DWORD i=0;i<state->scene->count;i++)
        {
            const StudioInstance *o=&state->scene->objects[i]; if (!o->asset) { continue; }
            const GltfModelImport *mesh=&o->asset->mesh;
            StudioMatrix matrix; StudioMatrixBuild(&o->transform,&matrix);
            glPushMatrix(); glMultMatrixd(matrix.m);
            for (DWORD first=0;first<mesh->count;)
            {
                DWORD slot=mesh->materials.faces[first].slot,end=first+1;
                while (end<mesh->count && mesh->materials.faces[end].slot==slot) { end++; }
                if (slot<o->materialcount)
                {
                    const StudioMaterial *m=&o->materials[slot];
                    if ((kind==METALLIC || kind==ENVIRONMENT) && m->metalness==0) { first=end; continue; }
                    StudioTexture *texture=kind==ENVIRONMENT ? environment : kind==BASE_IMAGE || (!reflect && kind!=ADDITIVE)
                        ? StudioViewportTexture(state,m->image,FALSE,0) : NULL;
                    if (kind==ENVIRONMENT && m->environmentblur>0)
                    {
                        int blur=max(0,min(100,(int)(m->environmentblur*100+.5f)));
                        StudioTexture *blurred=StudioViewportTexture(state,state->scene->environment,TRUE,blur);
                        if (blurred && blurred->id) { texture=blurred; }
                    }
                    if (kind==BASE_IMAGE && (!texture || !texture->id)) { first=end; continue; }
                    if (texture && texture->id)
                    { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D,texture->id); glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE); }
                    else { glDisable(GL_TEXTURE_2D); }
                    glBegin(GL_TRIANGLES);
                    for (DWORD face=first;face<end;face++)
                    {
                        const BgVertex *triangle=&mesh->vertices[face*3]; double uv[3][2];
                        if (kind==ENVIRONMENT) { StudioEnvironmentCoordinatesView(triangle,&matrix,eye,parallel,uv); }
                        for (int k=0;k<3;k++)
                        {
                            const BgVertex *vertex=&triangle[k];
                            if (kind==ENVIRONMENT)
                            {
                                float strength=m->metalness/255.f;
                                glColor3f(m->base[0]*vertex->r*strength,m->base[1]*vertex->g*strength,m->base[2]*vertex->b*strength);
                                double edge=.5/texture->height;
                                glTexCoord2d(uv[k][0],fmax(edge,fmin(1-edge,uv[k][1])));
                            }
                            else
                            {
                                if (kind==BASE_IMAGE) { glColor3f(1,1,1); }
                                else
                                {
                                    float diffuse[3],additive[3],metallic[3];
                                    StudioShadeView(state->scene,m,vertex,&matrix,eye,parallel,diffuse,additive,metallic);
                                    glColor3fv(kind==METALLIC ? metallic : kind==ADDITIVE ? additive : diffuse);
                                }
                                glTexCoord2f(vertex->s,vertex->t);
                            }
                            glVertex3f(vertex->x,vertex->y,vertex->z);
                        }
                    }
                    glEnd();
                }
                first=end;
            }
            glPopMatrix();
        }
    }
    glDisable(GL_BLEND); glDisable(GL_TEXTURE_2D); glDepthMask(GL_TRUE); glDepthFunc(GL_LEQUAL);
    double lo[3],hi[3];
    if (!parallel && state->selected>=0 && StudioBounds(state->scene,state->selected,lo,hi))
    {
        glColor3ub(242,194,70); glLineWidth(1.5f); glBegin(GL_LINES);
        for (int k=0;k<3;k++) for (int edge=0;edge<4;edge++)
        {
            double a[3],b[3]; for (int j=0;j<3;j++) { a[j]=b[j]=lo[j]; }
            a[(k+1)%3]=b[(k+1)%3]=(edge&1) ? hi[(k+1)%3] : lo[(k+1)%3];
            a[(k+2)%3]=b[(k+2)%3]=(edge&2) ? hi[(k+2)%3] : lo[(k+2)%3]; b[k]=hi[k];
            glVertex3dv(a); glVertex3dv(b);
        }
        glEnd(); glLineWidth(1);
    }
}

/* Shared camera pass: identical framing, materials and lighting for preview
 * and output. The output aspect determines horizontal span; size is vertical. */
static void StudioViewportCamera(StudioViewport *state,int x,int y,int width,int height)
{
    double viewmatrix[16],view[3],nearz,farz,half=state->scene->camera.size*.5;
    double aspect=(double)state->scene->render.width/state->scene->render.height;
    StudioCameraView(&state->scene->camera,viewmatrix,view); StudioCameraClip(state->scene,&nearz,&farz);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_SCISSOR_TEST); glScissor(x,y,width,height); glViewport(x,y,width,height);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE); glDepthMask(GL_TRUE); glClearDepth(1);
    glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDisable(GL_BLEND);
    glDisable(GL_LIGHTING); glDisable(GL_TEXTURE_2D); glDisable(GL_CULL_FACE); glDisable(GL_FOG);
    glDisable(GL_ALPHA_TEST); glDisable(GL_STENCIL_TEST); glDisable(GL_DITHER);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(-half*aspect,half*aspect,-half,half,nearz,farz);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadMatrixd(viewmatrix);
    StudioViewportObjects(state,view,TRUE);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}

static void StudioViewportPreview(StudioViewport *state,int width,int height)
{
    StudioPreviewRect rect;
    if (!state->scene || !state->scene->filename[0] || !StudioCameraValid(&state->scene->camera)
        || !StudioRenderSettingsValid(&state->scene->render) || !StudioCameraPreviewRect(width,height,&rect)) { return; }
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    glEnable(GL_SCISSOR_TEST);
    glScissor(rect.left-1,height-rect.bottom-1,STUDIO_PREVIEW_SIZE+2,STUDIO_PREVIEW_SIZE+2);
    if (state->selected==STUDIO_SELECT_CAMERA) { glClearColor(.95f,.76f,.27f,1); }
    else { glClearColor(.45f,.47f,.5f,1); }
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(rect.left,height-rect.bottom,STUDIO_PREVIEW_SIZE,STUDIO_PREVIEW_SIZE);
    glClearColor(.13f,.14f,.16f,1); glClear(GL_COLOR_BUFFER_BIT);
    StudioCameraImageRect(&state->scene->render,&rect);
    StudioViewportCamera(state,rect.left,height-rect.bottom,rect.right-rect.left,rect.bottom-rect.top);
    glPopAttrib();
}

/* Alpha comes from geometry coverage, never RGB or the window pixel format's
 * optional alpha bits. Even black models are opaque; an empty scene stays clear. */
static BOOL StudioViewportReadCamera(StudioViewport *state,TexPixel *pixels,float *depth)
{
    int width=state->scene->render.width,height=state->scene->render.height;
    glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
    glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT,1); glPixelStorei(GL_PACK_ROW_LENGTH,0);
    glPixelStorei(GL_PACK_SKIP_ROWS,0); glPixelStorei(GL_PACK_SKIP_PIXELS,0); glPixelStorei(GL_PACK_SWAP_BYTES,GL_FALSE);
    while (glGetError()!=GL_NO_ERROR) { }
    StudioViewportCamera(state,0,0,width,height);
    glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    glReadPixels(0,0,width,height,GL_DEPTH_COMPONENT,GL_FLOAT,depth);
    BOOL ok=glGetError()==GL_NO_ERROR;
    glPopClientAttrib(); glPopAttrib();
    if (ok) for (int i=0;i<width*height;i++)
    {
        pixels[i].a=depth[i]<1.0f ? 255 : 0;
        if (!pixels[i].a) { pixels[i].r=pixels[i].g=pixels[i].b=0; }
    }
    return ok;
}

BOOL StudioViewportRender(HWND viewport,TexPixel **pixels,const char **why)
{
    StudioViewport *state=(StudioViewport *)GetWindowLongPtr(viewport,GWLP_USERDATA);
    RECT client; *pixels=NULL; *why="";
    if (!state || !state->context || !state->scene || !state->scene->filename[0]
        || !StudioCameraValid(&state->scene->camera) || !StudioRenderSettingsValid(&state->scene->render))
    { *why="Open a valid scene before rendering."; return FALSE; }
    GetClientRect(viewport,&client);
    if (client.right<state->scene->render.width || client.bottom<state->scene->render.height)
    { *why="Enlarge the Render Studio viewport before rendering."; return FALSE; }
    size_t count=(size_t)state->scene->render.width*state->scene->render.height;
    TexPixel *rgba=malloc(count*sizeof(*rgba)); float *depth=malloc(count*sizeof(*depth));
    if (!rgba || !depth) { free(rgba); free(depth); *why="Not enough memory to render the image."; return FALSE; }
    HDC previousdc=wglGetCurrentDC(); HGLRC previous=wglGetCurrentContext(); BOOL ok=FALSE;
    if (wglMakeCurrent(state->dc,state->context))
    { ok=StudioViewportReadCamera(state,rgba,depth); wglMakeCurrent(previousdc,previous); }
    free(depth); InvalidateRect(viewport,NULL,FALSE);
    if (!ok) { free(rgba); *why="OpenGL could not render the output image."; return FALSE; }
    *pixels=rgba; return TRUE;
}

/* The grid is a background guide, not part of the model's depth bounds.
 * A separate projection keeps both its near and distant lines visible without
 * sacrificing model depth precision (especially for the additive passes). */
static void StudioViewportGrid(const double eye[3],int width,int height)
{
    double distance=sqrt(eye[0]*eye[0]+eye[1]*eye[1]+eye[2]*eye[2]);
    double nearz=.001,farz=distance+18;
    double halfheight=tan(STUDIO_FOV*3.14159265358979323846/360.0)*nearz;
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
    glFrustum(-halfheight*width/height,halfheight*width/height,-halfheight,halfheight,nearz,farz);
    glMatrixMode(GL_MODELVIEW); glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE);
    glLineWidth(1); glBegin(GL_LINES);
    for (int i = -10; i <= 10; i++)
    {
        if (!i) { continue; }
        if (i % 5) { glColor3ub(65, 69, 77); }
        else { glColor3ub(89, 94, 103); }
        glVertex3i(i, 0, -10); glVertex3i(i, 0, 10);
        glVertex3i(-10, 0, i); glVertex3i(10, 0, i);
    }
    glEnd();
    glLineWidth(2); glBegin(GL_LINES);
    glColor3ub(190, 83, 83); glVertex3i(-10, 0, 0); glVertex3i(10, 0, 0);
    glColor3ub(90, 175, 111); glVertex3i(0, 0, 0); glVertex3i(0, 3, 0);
    glColor3ub(88, 130, 205); glVertex3i(0, 0, -10); glVertex3i(0, 0, 10);
    glEnd(); glLineWidth(1);
    glDepthMask(GL_TRUE); glEnable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

static void StudioViewportDraw(StudioViewport *state, int width, int height)
{
    double nearz, farz, eye[3], halfheight;
    if (width < 1 || height < 1) { return; }
    OrbitCameraClip(&state->camera, &nearz, &farz);
    halfheight = tan(STUDIO_FOV * 3.14159265358979323846 / 360.0) * nearz;
    glViewport(0, 0, width, height);
    glClearColor(0.13f, 0.14f, 0.16f, 1);
    glDepthMask(GL_TRUE); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glDisable(GL_TEXTURE_2D); glDisable(GL_LIGHTING); glDisable(GL_FOG);
    glDisable(GL_BLEND); glDisable(GL_CULL_FACE);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glFrustum(-halfheight * width / height, halfheight * width / height,
        -halfheight, halfheight, nearz, farz);
    OrbitCameraPosition(&state->camera, eye);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glRotated(-state->camera.pitch, 1, 0, 0);
    glRotated(-state->camera.yaw, 0, 1, 0);
    glTranslated(-eye[0], -eye[1], -eye[2]);

    StudioViewportGrid(eye,width,height);
    StudioViewportObjects(state,eye,FALSE);
    if (state->scene && state->scene->filename[0])
        StudioCameraModelDraw(&state->cameramodel,&state->scene->camera,state->selected==STUDIO_SELECT_CAMERA);
    StudioLightIconsDraw(&state->lighticons,state->scene,&state->camera,width,height,state->selected);
    StudioGizmoFrame frame;
    if (StudioViewportGizmoFrame(state,width,height,&frame))
        StudioGizmoDraw(&state->gizmo,&frame,state->transforming ? state->drag.axis : state->hover);
    StudioViewportPreview(state,width,height);
}

static LRESULT CALLBACK StudioViewportProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    StudioViewport *state = (StudioViewport *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (message)
    {
    case WM_CREATE:
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        if (!StudioViewportInit(hwnd, state)
            || !StudioGizmoLoad(&state->gizmo,(HINSTANCE)GetWindowLongPtr(hwnd,GWLP_HINSTANCE))
            || !StudioCameraModelLoad(&state->cameramodel,(HINSTANCE)GetWindowLongPtr(hwnd,GWLP_HINSTANCE))) { return -1; }
        StudioViewportReset(hwnd);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: StudioViewportCancelTransform(hwnd); InvalidateRect(hwnd, NULL, FALSE); return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT paint; RECT client;
        HDC previousdc = wglGetCurrentDC();
        HGLRC previous = wglGetCurrentContext();
        BeginPaint(hwnd, &paint); GetClientRect(hwnd, &client);
        if (state && state->context && wglMakeCurrent(state->dc, state->context))
        {
            StudioViewportDraw(state, client.right, client.bottom);
            SwapBuffers(state->dc);
            wglMakeCurrent(previousdc, previous);
        }
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        if (!state) { break; }
        if (state->transforming) { return 0; }
        SetFocus(hwnd);
        if (StudioViewportOverPreview(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)))
        {
            state->moved=TRUE;
            if (message==WM_LBUTTONDOWN) { SendMessage(GetParent(hwnd),STUDIO_WM_SELECT,(WPARAM)(INT_PTR)STUDIO_SELECT_CAMERA,-1); }
            return 0;
        }
        if (message==WM_LBUTTONDOWN)
        {
            StudioGizmoFrame frame; StudioTransform transform; double hit[3]; RECT client;
            int axis=StudioViewportGizmoHit(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),&frame,hit);
            GetClientRect(hwnd,&client);
            if (axis>=0 && StudioViewportTransform(state,&transform) && StudioDragBegin(&state->drag,&transform,
                &state->camera,client.right,client.bottom,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),&frame,axis,hit))
            {
                int slot=StudioSceneLightIndex(state->scene,state->selected);
                if (slot>=0) { state->lightbefore=state->scene->lights[slot]; }
                state->transforming=TRUE; state->dragobject=state->selected; state->moved=TRUE;
                if (GetCapture()!=hwnd) { SetCapture(hwnd); }
                InvalidateRect(hwnd,NULL,FALSE); return 0;
            }
        }
        if (GetCapture()!=hwnd) { SetCapture(hwnd); }
        state->buttons |= message == WM_LBUTTONDOWN ? MK_LBUTTON : message == WM_MBUTTONDOWN ? MK_MBUTTON : MK_RBUTTON;
        state->mouse = (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        state->press=state->mouse; state->moved=FALSE;
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        if (!state) { break; }
        if (state->transforming)
        { if (message==WM_LBUTTONUP) { StudioViewportEndTransform(hwnd,state,TRUE); } return 0; }
        state->buttons &= ~(message == WM_LBUTTONUP ? MK_LBUTTON : message == WM_MBUTTONUP ? MK_MBUTTON : MK_RBUTTON);
        if (!state->buttons && GetCapture() == hwnd) { ReleaseCapture(); }
        if (message==WM_LBUTTONUP && !state->moved && !StudioViewportOverPreview(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)))
        {
            RECT client; double origin[3],direction[3]; int material=-1,index=-1;
            GetClientRect(hwnd,&client);
            index=StudioLightIconPick(state->scene,&state->camera,client.right,client.bottom,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam));
            if (index==-1 && StudioRay(&state->camera,client.right,client.bottom,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),origin,direction))
            {
                double distance,cameradistance;
                index=StudioPickDistance(state->scene,origin,direction,&material,&distance);
                if (state->scene && state->scene->filename[0]
                    && StudioCameraModelPick(&state->cameramodel,&state->scene->camera,origin,direction,&cameradistance) && cameradistance<distance)
                { index=STUDIO_SELECT_CAMERA; material=-1; }
            }
            SendMessage(GetParent(hwnd),STUDIO_WM_SELECT,(WPARAM)(INT_PTR)index,(LPARAM)material);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (state && state->transforming)
        {
            StudioTransform transform;
            if (StudioDragUpdate(&state->drag,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),(GetKeyState(VK_CONTROL)&0x8000)!=0,&transform))
            {
                int slot=StudioSceneLightIndex(state->scene,state->dragobject); BOOL applied=FALSE;
                if (slot>=0)
                    applied=StudioLightDragApply(&state->lightbefore,&transform,slot,state->drag.mode,&state->scene->lights[slot]);
                else if (state->scene && state->dragobject==STUDIO_SELECT_CAMERA)
                { state->scene->camera.transform=transform; applied=TRUE; }
                else if (state->scene && state->dragobject>=0 && (DWORD)state->dragobject<state->scene->count)
                { state->scene->objects[state->dragobject].transform=transform; applied=TRUE; }
                if (applied)
                { SendMessage(GetParent(hwnd),slot>=0 ? STUDIO_WM_LIGHT_TRANSFORM : STUDIO_WM_TRANSFORM,0,0); InvalidateRect(hwnd,NULL,FALSE); }
            }
            return 0;
        }
        if (state && !state->buttons)
        {
            StudioGizmoFrame frame; double hit[3]; int hover=StudioViewportGizmoHit(hwnd,state,GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam),&frame,hit);
            if (hover!=state->hover) { state->hover=hover; InvalidateRect(hwnd,NULL,FALSE); }
        }
        if (state && state->buttons)
        {
            RECT client;
            POINT mouse = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            GetClientRect(hwnd, &client);
            if (abs(mouse.x-state->press.x)>GetSystemMetrics(SM_CXDRAG) || abs(mouse.y-state->press.y)>GetSystemMetrics(SM_CYDRAG)) { state->moved=TRUE; }
            if (state->buttons & MK_MBUTTON)
                OrbitCameraPan(&state->camera, mouse.x - state->mouse.x, mouse.y - state->mouse.y, client.bottom, STUDIO_FOV);
            else
                OrbitCameraRotate(&state->camera, mouse.x - state->mouse.x, mouse.y - state->mouse.y);
            state->mouse = mouse; InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state && !state->transforming)
        {
            POINT point={GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}; ScreenToClient(hwnd,&point);
            if (StudioViewportOverPreview(hwnd,state,point.x,point.y)) { return 0; }
            OrbitCameraDolly(&state->camera, (double)GET_WHEEL_DELTA_WPARAM(wparam) / WHEEL_DELTA);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (wparam==VK_ESCAPE && StudioViewportCancelTransform(hwnd)) { return 0; }
        if (state && !state->transforming && wparam=='Z') { StudioViewportFrame(hwnd,state,state->selected); return 0; }
        break;
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    case WM_CAPTURECHANGED:
        StudioViewportCancelTransform(hwnd);
        if (state) { state->buttons = 0; state->hover=-1; }
        if (GetCapture() == hwnd) { ReleaseCapture(); }
        return 0;
    case WM_NCDESTROY:
        if (state)
        {
            if (GetCapture() == hwnd) { ReleaseCapture(); }
            if (state->context)
            {
                HDC dc=wglGetCurrentDC(); HGLRC context=wglGetCurrentContext();
                if (wglMakeCurrent(state->dc,state->context))
                { StudioViewportTextures(state); StudioLightIconsFree(&state->lighticons); wglMakeCurrent(dc,context); }
                if (wglGetCurrentContext() == state->context) { wglMakeCurrent(NULL, NULL); }
                wglDeleteContext(state->context);
            }
            if (state->dc) { ReleaseDC(hwnd, state->dc); }
            StudioGizmoFree(&state->gizmo); StudioCameraModelFree(&state->cameramodel);
            free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        }
        break;
    }
    return DefWindowProc(hwnd, message, wparam, lparam);
}

HWND StudioViewportCreate(HWND parent, HINSTANCE instance)
{
    WNDCLASS window = {0};
    window.style = CS_OWNDC;
    window.lpfnWndProc = StudioViewportProc;
    window.hInstance = instance;
    window.hCursor = LoadCursor(NULL, IDC_ARROW);
    window.lpszClassName = STUDIO_VIEWPORT_CLASS;
    if (!RegisterClass(&window) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { return NULL; }
    return CreateWindowEx(WS_EX_CLIENTEDGE, STUDIO_VIEWPORT_CLASS, "Studio viewport",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS,
        0, 0, 16, 16, parent, NULL, instance, NULL);
}
