/* BMP alpha compatibility, independent of the Windows imaging codec. The
 * production restore step receives top-down RGB pixels with WIC-style opaque
 * alpha; tests ensure only the intended alpha changes, in the right rows. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texencode.h"
BOOL TestRestoreImportBmpAlpha(const char *, TexPixel *, DWORD, DWORD);
static void Put32(unsigned char *p, DWORD v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
static DWORD Get32(const unsigned char *p) { return (DWORD)p[0]|(DWORD)p[1]<<8|(DWORD)p[2]<<16|(DWORD)p[3]<<24; }
static void Write(const char *path, const unsigned char *data, DWORD size)
{ FILE *file=fopen(path,"wb"); assert(file && fwrite(data,1,size,file)==size && !fclose(file)); }
static void Opaque(TexPixel *p, DWORD count)
{ for (DWORD i=0;i<count;i++) { p[i]=(TexPixel){i*11,i*7,i*3,255}; } }
static void SameRGB(const TexPixel *a, const TexPixel *b, DWORD count)
{ for (DWORD i=0;i<count;i++) { assert(a[i].r==b[i].r && a[i].g==b[i].g && a[i].b==b[i].b); } }
static DWORD Fixture(unsigned char *data, DWORD header, BOOL topdown, DWORD compression, DWORD mask)
{
    DWORD offset=14+header+(header==40 && compression==6 ? 16 : 0);
    const unsigned char alpha[]={0,3,255,18,128,255};
    memset(data,0,256); data[0]='B';data[1]='M';
    Put32(data+2,offset+24); Put32(data+10,offset); Put32(data+14,header);
    Put32(data+18,3); Put32(data+22,topdown?(DWORD)-2:2);
    data[26]=1; data[28]=32; Put32(data+30,compression); Put32(data+34,26); /* Photoshop includes trailing padding. */
    if (header>=56 || compression==6)
    {
        Put32(data+54,0x00ff0000);Put32(data+58,0x0000ff00);Put32(data+62,0x000000ff);Put32(data+66,mask);
    }
    for (DWORD y=0;y<2;y++) for (DWORD x=0;x<3;x++)
    { DWORD i=y*3+x, j=(topdown?y:1-y)*3+x; data[offset+j*4+3]=alpha[i]; }
    return offset+26;
}
void CheckBmpAlpha(const char *directory, const char *photoshop)
{
    unsigned char data[256]; TexPixel pixels[6],before[6]; char path[MAX_PATH]; DWORD size,offset;
    snprintf(path,sizeof(path),"%s/alpha-test.bmp",directory);
    const DWORD headers[]={40,56,108,124};
    const unsigned char expected[]={0,3,255,18,128,255};
    for (DWORD h=0;h<4;h++) for (int topdown=0;topdown<2;topdown++)
    {
        size=Fixture(data,headers[h],topdown,h?3:0,0xff000000); Write(path,data,size);
        Opaque(pixels,6);memcpy(before,pixels,sizeof(pixels));
        assert(TestRestoreImportBmpAlpha(path,pixels,3,2));SameRGB(before,pixels,6);
        for (DWORD i=0;i<6;i++) { assert(pixels[i].a==expected[i]); }
        /* Unmarked zero bytes mean unused XRGB; an explicit mask means zero alpha. */
        offset=Get32(data+10); memset(data+offset,0,24);Write(path,data,size);Opaque(pixels,6);
        assert(TestRestoreImportBmpAlpha(path,pixels,3,2));
        for (DWORD i=0;i<6;i++) { assert(pixels[i].a==(h?0:255)); }
    }
    size=Fixture(data,40,FALSE,6,0xff000000);Write(path,data,size);Opaque(pixels,6);
    assert(TestRestoreImportBmpAlpha(path,pixels,3,2));
    for (DWORD i=0;i<6;i++) { assert(pixels[i].a==expected[i]); }
    /* A4 alpha scales to eight bits, without reading RGB or the unused nibble. */
    size=Fixture(data,108,TRUE,3,0xf0000000); offset=Get32(data+10);
    for (DWORD i=0;i<6;i++) { data[offset+i*4+3]=(i*3)<<4; }
    Write(path,data,size);Opaque(pixels,6);assert(TestRestoreImportBmpAlpha(path,pixels,3,2));
    for (DWORD i=0;i<6;i++) { assert(pixels[i].a==i*51); }
    /* 24-bit and RGB-only bitfield files leave the codec result untouched. */
    size=Fixture(data,40,FALSE,0,0);data[28]=24;Write(path,data,size);Opaque(pixels,6);memcpy(before,pixels,sizeof(pixels));
    assert(TestRestoreImportBmpAlpha(path,pixels,3,2) && !memcmp(before,pixels,sizeof(pixels)));
    size=Fixture(data,108,FALSE,3,0);Write(path,data,size);
    assert(TestRestoreImportBmpAlpha(path,pixels,3,2) && !memcmp(before,pixels,sizeof(pixels)));
    /* Reject truncated, overlapping, overflowing, and mismatched pixel data
       atomically; no caller pixel is changed on failure. */
    for (int bad=0;bad<8;bad++)
    {
        size=Fixture(data,108,FALSE,3,0xff000000);
        if (bad==0) { size-=3; }
        if (bad==1) { Put32(data+10,54); }
        if (bad==2) { Put32(data+10,0xfffffff0u); }
        if (bad==3) { Put32(data+22,0x80000000u); }
        if (bad==4) { Put32(data+18,256); }
        if (bad==5) { Put32(data+66,0xffff0000u); }
        if (bad==6) { Put32(data+66,0xf5000000u); }
        if (bad==7) { size=80; }
        Write(path,data,size);Opaque(pixels,6);memcpy(before,pixels,sizeof(pixels));
        assert(!TestRestoreImportBmpAlpha(path,pixels,3,2) && !memcmp(before,pixels,sizeof(pixels)));
    }
    assert(DeleteFile(path));
    puts("PASS: Photoshop/legacy BMP alpha, V3/V4/V5 and external masks, top-down/bottom-up rows, opaque padding, explicit transparent images, RGB preservation and malformed-file rejection.");
    if (photoshop)
    {
        FILE *f=fopen(photoshop,"rb"); assert(f); fseek(f,0,SEEK_END); long length=ftell(f); rewind(f);
        unsigned char *bmp=malloc(length); assert(bmp && fread(bmp,1,length,f)==(size_t)length && !fclose(f));
        DWORD w=Get32(bmp+18),h=Get32(bmp+22),start=Get32(bmp+10),count=w*h;
        assert(w==64 && h==64 && Get32(bmp+14)==40 && bmp[28]==32 && Get32(bmp+30)==0);
        assert(start+(size_t)count*4<=(size_t)length);
        TexPixel *image=malloc(count*sizeof(*image)),*decoded=malloc(count*sizeof(*decoded)); assert(image && decoded);
        for (DWORD y=0;y<h;y++) for (DWORD x=0;x<w;x++)
        { unsigned char *p=bmp+start+((h-1-y)*w+x)*4;image[y*w+x]=(TexPixel){p[2],p[1],p[0],255}; }
        assert(TestRestoreImportBmpAlpha(photoshop,image,w,h));
        DWORD transparent=0,near=0;
        for (DWORD y=0;y<h;y++) for (DWORD x=0;x<w;x++)
        {
            unsigned char *p=bmp+start+((h-1-y)*w+x)*4;TexPixel q=image[y*w+x];
            assert(q.r==p[2] && q.g==p[1] && q.b==p[0] && q.a==p[3]);
            transparent+=q.a==0;near+=q.a>0 && q.a<128;
        }
        assert(transparent==138 && near==74);
        const char *why="";TexImportOptions options={10,0,0,0};unsigned char *record=NULL;DWORD recordsize;int rw,rh;
        assert(TexEncodeRecord(image,w,h,&options,&record,&recordsize,&why));
        assert(TexDecodeRecord(record,recordsize,decoded,&rw,&rh));
        assert(rw==64 && rh==64);
        for (DWORD i=0;i<count;i++) { assert(decoded[i].a==(image[i].a>=128?255:0)); }
        printf("PASS: supplied jungle_tree_wall.bmp retains all RGBA values; CI4 encoding preserves all %u cutout pixels.\n",transparent+near);
        free(bmp);free(image);free(decoded);free(record);
    }
}
