/* Deterministic GUTX encoder. Palettes cover the base image and every mip;
 * rows are left unswapped, as expected by texLoadRaw's explicit-LOD path. */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "texencode.h"

static void Write16(unsigned char *p, unsigned int v) { p[0] = v >> 8; p[1] = v; }
static void Write32(unsigned char *p, DWORD v) { p[0]=v>>24; p[1]=v>>16; p[2]=v>>8; p[3]=v; }
DWORD TexImportRowBytes(int format, int width)
{
    if (width < 1 || width > 255) { return 0; }
    if (format == 0 || format == 2) { return ((width + 3) & ~3) * 4; }
    if (format == 1 || format == 3 || format == 4) { return ((width + 3) & ~3) * 2; }
    if (format == 5 || format == 7 || format == 9 || format == 11) { return (width + 7) & ~7; }
    if (format == 6 || format == 8 || format == 10 || format == 12) { return ((width + 15) & ~15) / 2; }
    return 0;
}
DWORD TexImportTmemLimit(int format) { return format >= 9 ? 2048 : 4096; }
int TexImportMaxMipmaps(int width, int height)
{
    int count = 0;
    while ((width > 1 || height > 1) && count < 6)
    { width = (width + 1) / 2; height = (height + 1) / 2; count++; }
    return count;
}
DWORD TexImportTmemBytes(int width, int height, int format, int mipmaps)
{
    DWORD size = 0;
    int i;
    if (height < 1 || height > 255 || !TexImportRowBytes(format, width)
        || mipmaps < 0 || mipmaps > TexImportMaxMipmaps(width, height)) { return 0; }
    for (i = 0; i <= mipmaps; i++)
    {
        size += TexImportRowBytes(format, width) * height;
        width = (width + 1) / 2; height = (height + 1) / 2;
    }
    return size;
}
static unsigned int Intensity(TexPixel p) { return (77u*p.r + 150u*p.g + 29u*p.b + 128) >> 8; }
static unsigned short PaletteColor(TexPixel p, int format)
{
    return format >= 11 ? (Intensity(p) << 8) | p.a
        : (((unsigned int)p.r >> 3) << 11) | ((p.g >> 3) << 6) | ((p.b >> 3) << 1) | (p.a >= 128);
}
static TexPixel PalettePixel(unsigned int color, int format)
{
    TexPixel p;
    if (format >= 11) { p.r = p.g = p.b = color >> 8; p.a = color; }
    else { p.r = ((color >> 11) & 31) << 3; p.g = ((color >> 6) & 31) << 3; p.b = ((color >> 1) & 31) << 3; p.a = (color & 1) ? 255 : 0; }
    return p;
}

typedef struct ColorCount { unsigned short color; DWORD count; TexPixel pixel; } ColorCount;
typedef struct ColorBox { int first, count; DWORD weight; unsigned int range; int axis; } ColorBox;
static int g_PaletteAxis;
static unsigned int Channel(TexPixel p, int axis) { return axis==0 ? p.r : axis==1 ? p.g : axis==2 ? p.b : p.a; }
static int CompareColor(const void *a, const void *b)
{
    const ColorCount *x=a, *y=b;
    int d=(int)Channel(x->pixel,g_PaletteAxis)-(int)Channel(y->pixel,g_PaletteAxis);
    return d ? d : (int)x->color-(int)y->color;
}
static void MeasureBox(ColorBox *box, ColorCount *colors)
{
    unsigned int min[4]={255,255,255,255}, max[4]={0,0,0,0};
    int i, axis;
    box->weight=0; box->range=0; box->axis=0;
    for (i=box->first;i<box->first+box->count;i++)
    {
        box->weight += colors[i].count;
        for (axis=0;axis<4;axis++)
        {
            unsigned int c=Channel(colors[i].pixel,axis);
            if(c<min[axis]) { min[axis]=c; } if(c>max[axis]) { max[axis]=c; }
        }
    }
    for(axis=0;axis<4;axis++)
    {
        unsigned int range=max[axis]-min[axis];
        if(range>box->range) { box->range=range;box->axis=axis; }
    }
}
/* Weighted median-cut, followed by nearest-color lookup. Small palettes and
 * colors already representable in the selected format are preserved exactly. */
static int MakePalette(TexPixel **levels, const int *widths, const int *heights, int count,
                       int format, unsigned short palette[256], unsigned char **lookup)
{
    DWORD *histogram=calloc(65536,sizeof(*histogram));
    ColorCount *colors=NULL;
    ColorBox boxes[256];
    int maximum=(format==10 || format==12) ? 16 : 256, total=0, used=1, level,i,b;
    if(!histogram) { return 0; }
    for(level=0;level<count;level++)
    { for(i=0;i<widths[level]*heights[level];i++) { histogram[PaletteColor(levels[level][i],format)]++; } }
    for(i=0;i<65536;i++) { if(histogram[i]) { total++; } }
    colors=malloc((size_t)total*sizeof(*colors)); *lookup=malloc(65536);
    if(!colors || !*lookup) { free(colors);free(histogram);free(*lookup);*lookup=NULL;return 0; }
    total=0;
    for(i=0;i<65536;i++)
    { if(histogram[i]) { colors[total++]=(ColorCount){(unsigned short)i,histogram[i],PalettePixel(i,format)}; } }
    free(histogram);
    boxes[0]=(ColorBox){0,total,0,0,0}; MeasureBox(&boxes[0],colors);
    while(used<maximum)
    {
        int best=-1, split, end;
        DWORD weight=0;
        ULONGLONG score=0;
        for(b=0;b<used;b++)
        {
            ULONGLONG candidate=(ULONGLONG)boxes[b].range*boxes[b].weight;
            if(boxes[b].count>1 && candidate>score) { best=b;score=candidate; }
        }
        if(best<0) { break; }
        g_PaletteAxis=boxes[best].axis;
        qsort(colors+boxes[best].first,boxes[best].count,sizeof(*colors),CompareColor);
        end=boxes[best].first+boxes[best].count;
        for(split=boxes[best].first;split<end-1;)
        { weight+=colors[split++].count; if(weight>=boxes[best].weight/2) { break; } }
        boxes[used]=(ColorBox){split,end-split,0,0,0};
        boxes[best].count=split-boxes[best].first;
        MeasureBox(&boxes[best],colors); MeasureBox(&boxes[used++],colors);
    }
    for(b=0;b<used;b++)
    {
        ULONGLONG sums[4]={0,0,0,0}; TexPixel p; int axis;
        for(i=boxes[b].first;i<boxes[b].first+boxes[b].count;i++)
        { for(axis=0;axis<4;axis++) { sums[axis]+=(ULONGLONG)Channel(colors[i].pixel,axis)*colors[i].count; } }
        p.r=sums[0]/boxes[b].weight;p.g=sums[1]/boxes[b].weight;
        p.b=sums[2]/boxes[b].weight;p.a=sums[3]/boxes[b].weight;
        palette[b]=PaletteColor(p,format);
    }
    for(i=0;i<total;i++)
    {
        unsigned int best=UINT_MAX;int index=0;
        for(b=0;b<used;b++)
        {
            TexPixel p=PalettePixel(palette[b],format),q=colors[i].pixel;
            int r=(int)p.r-q.r,g=(int)p.g-q.g,blue=(int)p.b-q.b,a=(int)p.a-q.a;
            unsigned int distance=r*r+g*g+blue*blue+2*a*a;
            if(distance<best) { best=distance;index=b; }
        }
        (*lookup)[colors[i].color]=index;
    }
    free(colors);return used;
}

BOOL TexEncodeRecord(const TexPixel *pixels, int width, int height,
    const TexImportOptions *options, unsigned char **data, DWORD *size, const char **reasonout)
{
    TexPixel *levels[7]={0}; int widths[7],heights[7],level,x,y,colors=0;
    unsigned short palette[256]; unsigned char *lookup=NULL,*result=NULL;
    DWORD bytes,header,offset,total; BOOL ok=FALSE;
    *data=NULL;*size=0;*reasonout="Invalid image dimensions, texture type, mipmaps, or surface settings.";
    if(!pixels || !options || options->hitsound>12 || options->hittexture>12
        || !(bytes=TexImportTmemBytes(width,height,options->format,options->mipmaps))) { return FALSE; }
    if(bytes>TexImportTmemLimit(options->format))
    { *reasonout="This image and its mipmaps do not fit in TMEM. Use a smaller format, fewer mipmaps, or resize the BMP.";return FALSE; }
    *reasonout="Out of memory encoding the texture.";
    widths[0]=width;heights[0]=height;levels[0]=malloc((size_t)width*height*sizeof(TexPixel));
    if(!levels[0]) { goto done; } memcpy(levels[0],pixels,(size_t)width*height*sizeof(TexPixel));
    if(options->format==2 || options->format==3 || options->format==7 || options->format==8)
    { for(x=0;x<width*height;x++) { levels[0][x].a=255; } }
    for(level=1;level<=options->mipmaps;level++)
    {
        int sw=widths[level-1],sh=heights[level-1];
        widths[level]=(sw+1)/2;heights[level]=(sh+1)/2;
        levels[level]=malloc((size_t)widths[level]*heights[level]*sizeof(TexPixel));
        if(!levels[level]) { goto done; }
        for(y=0;y<heights[level];y++) for(x=0;x<widths[level];x++)
        {
            unsigned int r=0,g=0,b=0,a=0,n=0; int dx,dy;
            for(dy=0;dy<2 && y*2+dy<sh;dy++) for(dx=0;dx<2 && x*2+dx<sw;dx++)
            { TexPixel p=levels[level-1][(y*2+dy)*sw+x*2+dx];r+=p.r*p.a;g+=p.g*p.a;b+=p.b*p.a;a+=p.a;n++; }
            levels[level][y*widths[level]+x]=(TexPixel){a ? r/a : 0,a ? g/a : 0,a ? b/a : 0,(a+n/2)/n};
        }
    }
    if(options->format>=9)
    { colors=MakePalette(levels,widths,heights,options->mipmaps+1,options->format,palette,&lookup);if(!colors) { goto done; } }
    header=(100+colors*2+15)&~15u;total=(header+bytes+15)&~15u;
    result=calloc(total,1);if(!result) { goto done; }
    memcpy(result,"GUTX",4);result[4]=1;result[5]=result[6]=options->mipmaps+1;
    Write16(result+8,colors);Write16(result+10,header);Write32(result+12,total);
    for(x=0;x<colors;x++) { Write16(result+100+x*2,palette[x]); }
    offset=header;
    for(level=0;level<=options->mipmaps;level++)
    {
        int format=options->format;DWORD rowbytes=TexImportRowBytes(format,widths[level]);
        unsigned char *desc=result+16+level*12;
        desc[0]=format;desc[1]=widths[level];desc[2]=heights[level];
        Write32(desc+4,offset);Write32(desc+8,rowbytes*heights[level]);
        for(y=0;y<heights[level];y++) for(x=0;x<widths[level];x++)
        {
            TexPixel p=levels[level][y*widths[level]+x];unsigned int intensity=Intensity(p),v=0;
            unsigned char *row=result+offset+y*rowbytes;
            switch(format)
            {
            case 0:case 2: row[x*4]=p.r;row[x*4+1]=p.g;row[x*4+2]=p.b;row[x*4+3]=format==0 ? p.a : 255;break;
            case 1:case 3: Write16(row+x*2,PaletteColor(p,9) | (format==3));break;
            case 4: Write16(row+x*2,(intensity<<8)|p.a);break;
            case 5: row[x]=(intensity&0xf0)|(p.a>>4);break;
            case 6: v=((intensity>>5)<<1)|(p.a>=128);break;
            case 7: row[x]=intensity;break;
            case 8: v=intensity>>4;break;
            case 9:case 11: row[x]=lookup[PaletteColor(p,format)];break;
            case 10:case 12: v=lookup[PaletteColor(p,format)];break;
            }
            if(format==6 || format==8 || format==10 || format==12)
            { if(x&1) { row[x/2]|=v; } else { row[x/2]=v<<4; } }
        }
        offset+=rowbytes*heights[level];
    }
    *data=result;result=NULL;*size=total;*reasonout="";ok=TRUE;
done:
    for(level=0;level<7;level++) { free(levels[level]); } free(lookup);free(result);return ok;
}
