#include <math.h>
#include <string.h>
#include "studiocamera.h"

BOOL StudioCameraPreviewRect(int width,int height,StudioPreviewRect *rect)
{
    const int margin=8;
    if (width<STUDIO_PREVIEW_SIZE+2*margin || height<STUDIO_PREVIEW_SIZE+2*margin) { return FALSE; }
    *rect=(StudioPreviewRect){width-margin-STUDIO_PREVIEW_SIZE,height-margin-STUDIO_PREVIEW_SIZE,width-margin,height-margin};
    return TRUE;
}

void StudioCameraImageRect(const StudioRenderSettings *settings,StudioPreviewRect *rect)
{
    int width=rect->right-rect->left,height=rect->bottom-rect->top;
    int w=width,h=height;
    if (settings->width>settings->height) { h=(int)fmax(1,round((double)width*settings->height/settings->width)); }
    else { w=(int)fmax(1,round((double)height*settings->width/settings->height)); }
    rect->left+=(width-w)/2; rect->top+=(height-h)/2;
    rect->right=rect->left+w; rect->bottom=rect->top+h;
}

void StudioCameraView(const StudioCamera *camera,double matrix[16],double view[3])
{
    Rotation rotation; RotationEuler(&rotation,camera->transform.rotation);
    memset(matrix,0,16*sizeof(*matrix)); matrix[15]=1;
    for (int row=0;row<3;row++)
    {
        view[row]=rotation.m[row][2]; /* Parallel surface-to-camera direction. */
        for (int col=0;col<3;col++)
        {
            matrix[col*4+row]=rotation.m[col][row];
            matrix[12+row]-=rotation.m[col][row]*camera->transform.position[col];
        }
    }
}

void StudioCameraClip(const StudioScene *scene,double *nearz,double *farz)
{
    double lower[3],upper[3],matrix[16],view[3],lo=0,hi=0;
    StudioCameraView(&scene->camera,matrix,view);
    if (StudioBounds(scene,-1,lower,upper)) for (int corner=0;corner<8;corner++)
    {
        double depth=0;
        for (int k=0;k<3;k++)
            depth+=(scene->camera.transform.position[k]-((corner&(1<<k)) ? upper[k] : lower[k]))*view[k];
        if (!corner || depth<lo) { lo=depth; } if (!corner || depth>hi) { hi=depth; }
    }
    double margin=fmax(.001,(hi-lo)*.01);
    *nearz=fmax(.0001,lo-margin); *farz=fmax(*nearz+1,hi+margin);
}
