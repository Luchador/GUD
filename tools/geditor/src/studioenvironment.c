#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "studioenvironment.h"

BOOL StudioEnvironmentBlur(const TexPixel *source,int width,int height,int percent,
    TexPixel **output,int *outwidth,int *outheight)
{
    *output=NULL; *outwidth=*outheight=0;
    if (!source || width<2 || width>4096 || (width & (width-1)) || height!=width/2
        || percent<0 || percent>100) { return FALSE; }
    if (!percent)
    {
        size_t bytes=(size_t)width*height*sizeof(**output);
        *output=malloc(bytes); if (!*output) { return FALSE; }
        memcpy(*output,source,bytes); *outwidth=width; *outheight=height; return TRUE;
    }
    int w=width;
    while (w>1024 || w*percent/1600.0>8) { w/=2; }
    int h=w/2,step=width/w;
    size_t count=(size_t)w*h;
    float (*small)[3]=malloc(count*sizeof(*small)),(*horizontal)[3]=malloc(count*sizeof(*horizontal));
    TexPixel *pixels=malloc(count*sizeof(*pixels));
    if (!small || !horizontal || !pixels) { free(small); free(horizontal); free(pixels); return FALSE; }
    /* Area averaging prevents aliasing when a wide blur permits a smaller map.
     * Accumulate in floats so the two convolution passes do not round twice. */
    for (int y=0;y<h;y++) for (int x=0;x<w;x++)
    {
        unsigned r=0,g=0,b=0;
        for (int dy=0;dy<step;dy++) for (int dx=0;dx<step;dx++)
        {
            const TexPixel *p=&source[(y*step+dy)*width+x*step+dx];
            r+=p->r; g+=p->g; b+=p->b;
        }
        float divisor=(float)(step*step);
        small[y*w+x][0]=r/divisor; small[y*w+x][1]=g/divisor; small[y*w+x][2]=b/divisor;
    }
    double sigma=w*percent/1600.0,total=0;
    int radius=(int)ceil(3*sigma); float weights[49];
    for (int k=-radius;k<=radius;k++)
    { weights[k+radius]=(float)exp(-.5*k*k/(sigma*sigma)); total+=weights[k+radius]; }
    for (int k=0;k<=2*radius;k++) { weights[k]=(float)(weights[k]/total); }
    for (int y=0;y<h;y++) for (int x=0;x<w;x++)
    {
        float *out=horizontal[y*w+x]; out[0]=out[1]=out[2]=0;
        for (int k=-radius;k<=radius;k++)
        {
            const float *in=small[y*w+(x+k+w)%w]; float weight=weights[k+radius];
            for (int c=0;c<3;c++) { out[c]+=in[c]*weight; }
        }
    }
    for (int y=0;y<h;y++) for (int x=0;x<w;x++)
    {
        float out[3]={0};
        for (int k=-radius;k<=radius;k++)
        {
            int row=y+k,column=x;
            if (row<0) { row=-row-1; column=(x+w/2)%w; }
            else if (row>=h) { row=2*h-1-row; column=(x+w/2)%w; }
            const float *in=horizontal[row*w+column]; float weight=weights[k+radius];
            for (int c=0;c<3;c++) { out[c]+=in[c]*weight; }
        }
        pixels[y*w+x]=(TexPixel){(unsigned char)fmin(255,out[0]+.5f),
            (unsigned char)fmin(255,out[1]+.5f),(unsigned char)fmin(255,out[2]+.5f),255};
    }
    free(small); free(horizontal);
    *output=pixels; *outwidth=w; *outheight=h; return TRUE;
}
