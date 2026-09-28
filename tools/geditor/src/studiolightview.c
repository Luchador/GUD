/* Screen-facing studio light icons. Drawing and picking share one screen rect;
 * icons remain visible over geometry and are ordered back-to-front. */
#include <windows.h>
#include <GL/gl.h>
#include <math.h>
#include <float.h>
#include <string.h>
#include "studiolightview.h"
#include "texload.h"
#include "resource.h"

void StudioLightIconsFree(StudioLightIcons *icons)
{
    glDeleteTextures(2,icons->texture); memset(icons,0,sizeof(*icons));
}

BOOL StudioLightIconsLoad(StudioLightIcons *icons,HINSTANCE instance)
{
    const int resources[2]={IDR_STUDIO_SPOTLIGHT,IDR_STUDIO_POINT_LIGHT};
    for (int i=0;i<2;i++)
    {
        TexThumb thumb={0}; unsigned char pixels[TEX_THUMB_MAX*TEX_THUMB_MAX*4];
        if (!TexLoadResourceThumbnail(instance,resources[i],&thumb,pixels)) { goto fail; }
        for (int p=0;p<TEX_THUMB_MAX*TEX_THUMB_MAX;p++)
        { unsigned char red=pixels[p*4+2]; pixels[p*4+2]=pixels[p*4]; pixels[p*4]=red; }
        glGenTextures(1,&icons->texture[i]); if (!icons->texture[i]) { goto fail; }
        glBindTexture(GL_TEXTURE_2D,icons->texture[i]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,TEX_THUMB_MAX,TEX_THUMB_MAX,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
        if (glGetError()!=GL_NO_ERROR) { goto fail; }
    }
    return TRUE;
fail:
    StudioLightIconsFree(icons); return FALSE;
}

BOOL StudioLightIconRect(const StudioLight *light,const OrbitCamera *camera,int width,int height,double rect[4],double *depth)
{
    double screen[2],eye[3],forward[3];
    if (!light->enabled || !StudioProject(camera,width,height,light->position,screen)
        || !StudioRay(camera,width,height,width*.5,height*.5,eye,forward)) { return FALSE; }
    *depth=0;
    for (int k=0;k<3;k++) { *depth+=(light->position[k]-eye[k])*forward[k]; }
    for (int k=0;k<2;k++) { rect[k]=screen[k]-STUDIO_LIGHT_ICON_SIZE*.5; rect[k+2]=screen[k]+STUDIO_LIGHT_ICON_SIZE*.5; }
    return rect[2]>0 && rect[3]>0 && rect[0]<width && rect[1]<height;
}

int StudioLightIconPick(const StudioScene *scene,const OrbitCamera *camera,int width,int height,double x,double y)
{
    double nearest=DBL_MAX; int selected=-1;
    if (!scene || x<0 || y<0 || x>=width || y>=height) { return -1; }
    for (int i=0;i<STUDIO_LIGHT_COUNT;i++)
    {
        double rect[4],depth;
        if (StudioLightIconRect(&scene->lights[i],camera,width,height,rect,&depth)
            && x>=rect[0] && x<rect[2] && y>=rect[1] && y<rect[3] && depth<=nearest)
        { nearest=depth; selected=STUDIO_LIGHT_SELECTION(i); }
    }
    return selected;
}

static void StudioLightDirectionDraw(const StudioLight *light,const OrbitCamera *camera,int width,int height)
{
    StudioTransform transform; StudioGizmoFrame frame; Rotation basis; double end[3],up[3]={0,1,0},side[3];
    StudioLightTransform(light,&transform);
    if (!StudioGizmoPlace(&transform,camera,width,height,STUDIO_TRANSLATE,&frame)) { return; }
    double length=sqrt(light->direction[0]*light->direction[0]+light->direction[1]*light->direction[1]+light->direction[2]*light->direction[2]);
    if (length<=1e-12) { return; }
    if (fabs(light->direction[1]/length)>.95) { up[0]=1; up[1]=0; }
    if (!RotationBasis(&basis,up,light->direction)) { return; }
    for (int k=0;k<3;k++) { end[k]=light->position[k]+basis.m[k][2]*frame.length*1.2; }
    glColor3ub(242,194,70); glLineWidth(2); glBegin(GL_LINES);
    glVertex3dv(light->position); glVertex3dv(end);
    for (int sign=-1;sign<=1;sign+=2)
    {
        for (int k=0;k<3;k++) { side[k]=end[k]+frame.length*(sign*.08*basis.m[k][0]-.16*basis.m[k][2]); }
        glVertex3dv(end); glVertex3dv(side);
    }
    glEnd();
}

void StudioLightIconsDraw(const StudioLightIcons *icons,const StudioScene *scene,const OrbitCamera *camera,int width,int height,int selected)
{
    if (!scene || width<1 || height<1) { return; }
    double rects[STUDIO_LIGHT_COUNT][4],depth[STUDIO_LIGHT_COUNT]; int order[STUDIO_LIGHT_COUNT],count=0;
    for (int i=0;i<STUDIO_LIGHT_COUNT;i++)
        if (StudioLightIconRect(&scene->lights[i],camera,width,height,rects[i],&depth[i])) { order[count++]=i; }
    for (int i=1;i<count;i++) for (int j=i;j>0 && depth[order[j]]>depth[order[j-1]];j--)
    { int swap=order[j]; order[j]=order[j-1]; order[j-1]=swap; }
    glPushAttrib(GL_ENABLE_BIT|GL_CURRENT_BIT|GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_TEXTURE_BIT|GL_LINE_BIT);
    glDisable(GL_DEPTH_TEST); glDepthMask(GL_FALSE); glDisable(GL_LIGHTING); glDisable(GL_FOG); glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D); glDisable(GL_BLEND);
    if (StudioSceneLightIndex(scene,selected)==0) { StudioLightDirectionDraw(&scene->lights[0],camera,width,height); }
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glEnable(GL_TEXTURE_2D); glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    for (int n=0;n<count;n++)
    {
        int i=order[n]; const double *r=rects[i];
        if (selected==STUDIO_LIGHT_SELECTION(i)) { glColor3ub(242,194,70); } else { glColor3ub(255,255,255); }
        glBindTexture(GL_TEXTURE_2D,icons->texture[i==0 ? 0 : 1]);
        glBegin(GL_QUADS);
        glTexCoord2f(0,0); glVertex2d(r[0],r[1]); glTexCoord2f(1,0); glVertex2d(r[2],r[1]);
        glTexCoord2f(1,1); glVertex2d(r[2],r[3]); glTexCoord2f(0,1); glVertex2d(r[0],r[3]);
        glEnd();
    }
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glPopAttrib();
}
