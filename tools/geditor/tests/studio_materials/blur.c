#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "studioenvironment.h"

/* An independent, slow 2-D convolution reference checks the separable path,
 * including longitude wrapping and latitude reflection across both poles. */
static int Reference(const TexPixel *source,int w,int h,int x,int y,int percent)
{
    double sigma=w*percent/1600.0,sum=0,weight=0;
    int radius=(int)ceil(3*sigma);
    for (int dy=-radius;dy<=radius;dy++) for (int dx=-radius;dx<=radius;dx++)
    {
        int row=y+dy,column=x+dx;
        if (row<0) { row=-1-row; column+=w/2; }
        if (row>=h) { row=2*h-1-row; column+=w/2; }
        column=(column+w)%w;
        double k=exp(-(dx*dx+dy*dy)/(2*sigma*sigma));
        sum+=source[row*w+column].r*k; weight+=k;
    }
    return (int)(sum/weight+.5);
}

int main(void)
{
    TexPixel source[32*16],before[32*16],*out=NULL;
    int w,h;
    for (int i=0;i<32*16;i++) { source[i]=(TexPixel){(i*31)%256,100,230,i%256}; }
    memcpy(before,source,sizeof(source));
    assert(StudioEnvironmentBlur(source,32,16,0,&out,&w,&h));
    assert(w==32 && h==16 && !memcmp(out,source,sizeof(source))); free(out);
    for (int percent=1;percent<=100;percent+=33)
    {
        assert(StudioEnvironmentBlur(source,32,16,percent,&out,&w,&h));
        assert(w==32 && h==16);
        for (int y=0;y<h;y++) for (int x=0;x<w;x++)
        {
            assert(abs(out[y*w+x].r-Reference(source,w,h,x,y,percent))<=1);
            assert(out[y*w+x].g==100 && out[y*w+x].b==230 && out[y*w+x].a==255);
        }
        free(out);
    }
    assert(!memcmp(before,source,sizeof(source)));
    memset(source,0,sizeof(source)); source[8*32].r=255;
    int peak=256;
    for (int percent=25;percent<=100;percent+=25)
    {
        assert(StudioEnvironmentBlur(source,32,16,percent,&out,&w,&h));
        assert(out[8*32].r<peak); peak=out[8*32].r;
        assert(out[8*32+1].r==out[8*32+31].r && out[8*32+31].r>0); free(out);
    }
    memset(source,0,sizeof(source)); source[0].r=255;
    assert(StudioEnvironmentBlur(source,32,16,100,&out,&w,&h));
    assert(out[16].r>0); free(out); /* Reflection continues over the north pole. */
    TexPixel *large=malloc(2048*1024*sizeof(*large)); assert(large);
    for (int i=0;i<2048*1024;i++) { large[i]=(TexPixel){17,123,255,0}; }
    for (int percent=1;percent<=100;percent+=33)
    {
        assert(StudioEnvironmentBlur(large,2048,1024,percent,&out,&w,&h));
        assert(w<=1024 && w==2*h);
        for (int i=0;i<w*h;i++) { assert(out[i].r==17 && out[i].g==123 && out[i].b==255 && out[i].a==255); }
        free(out);
    }
    free(large);
    assert(!StudioEnvironmentBlur(source,32,16,-1,&out,&w,&h) && !out && !w && !h);
    assert(!StudioEnvironmentBlur(source,32,16,101,&out,&w,&h));
    assert(!StudioEnvironmentBlur(source,30,15,50,&out,&w,&h));
    assert(!StudioEnvironmentBlur(source,32,15,50,&out,&w,&h));
    assert(StudioEnvironmentBlur(source,2,1,100,&out,&w,&h)); free(out);
    puts("PASS: Gaussian reference, 0% identity, increasing blur, seam/pole continuity, constant-color energy, adaptive resolution and unchanged source.");
    return 0;
}
