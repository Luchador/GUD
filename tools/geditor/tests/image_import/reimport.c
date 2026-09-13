#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imageedits.h"
#include "texrom.h"

/* The Windows WIC decoder is the only substituted part of reimport. This
 * fixture reader accepts the bottom-up 32-bit BMPs written by TexWriteBmp.
 * It reads the requested file each time, in the same native pixel order. */
BOOL TexReadImportBmp(const char *path,TexPixel **pixels,int *width,int *height,const char **why)
{
    BITMAPFILEHEADER head;BITMAPINFOHEADER info;FILE *file=fopen(path,"rb");
    TexPixel *out=NULL;int x,y;BOOL ok=FALSE;
    *pixels=NULL;*width=*height=0;*why="The selected file could not be decoded as a BMP image.";
    if(!file) { return FALSE; }
    if(fread(&head,1,sizeof(head),file)!=sizeof(head) || fread(&info,1,sizeof(info),file)!=sizeof(info)
        || head.bfType!=0x4d42 || info.biSize!=40 || info.biBitCount!=32 || info.biCompression
        || info.biWidth<1 || info.biWidth>255 || info.biHeight<1 || info.biHeight>255
        || fseek(file,head.bfOffBits,SEEK_SET)) { goto done; }
    out=malloc((size_t)info.biWidth*info.biHeight*sizeof(*out));assert(out);
    for(y=0;y<info.biHeight;y++) for(x=info.biWidth-1;x>=0;x--)
    {
        unsigned char p[4];
        if(fread(p,1,4,file)!=4) { goto done; }
        out[y*info.biWidth+x]=(TexPixel){p[2],p[1],p[0],p[3]};
    }
    *pixels=out;out=NULL;*width=info.biWidth;*height=info.biHeight;*why="";ok=TRUE;
done:
    free(out);fclose(file);return ok;
}
static DWORD FileHash(const char *path)
{
    unsigned char data[8192];FILE *file=fopen(path,"rb");size_t n;
    assert(file);n=fread(data,1,sizeof(data),file);assert(feof(file));fclose(file);
    return TexDataHash(data,(DWORD)n);
}
static void CheckImage(const char *project,DWORD id,int w,int h,const TexImportOptions *o)
{
    TexThumb *thumbs;unsigned char *pixels;const char *why;DWORD n,i;
    n=TexLoadProjectThumbnails(project,&thumbs,&pixels,&why);
    for(i=0;i<n;i++) if(strtoul(thumbs[i].label,NULL,16)==id) { break; }
    assert(i<n && thumbs[i].imagewidth==w && thumbs[i].imageheight==h);
    assert(thumbs[i].info.format==o->format && thumbs[i].info.mipmaps==o->mipmaps);
    assert(thumbs[i].info.hitsound==o->hitsound && thumbs[i].info.hittexture==o->hittexture);
    free(thumbs);free(pixels);
}
void CheckReimport(const char *project)
{
    TexPixel pixels[64*64],out[64*64];TexImportOptions o={0,2,12,6},other={10,1,3,9};
    char a[MAX_PATH],b[MAX_PATH],source[MAX_PATH],bmp[MAX_PATH],native[MAX_PATH],base[MAX_PATH];
    const char *why;DWORD id,next,bmphash,nativehash,i;int w,h;FILE *file;RomFile rom;TexRomBank bank;
    snprintf(a,sizeof(a),"%s\\source with spaces.bmp",project);
    snprintf(b,sizeof(b),"%s\\replacement.bmp",project);
    snprintf(bmp,sizeof(bmp),"%s\\images\\0010.bmp",project);
    snprintf(native,sizeof(native),"%s\\images\\native\\0010.gtex",project);
    snprintf(base,sizeof(base),"%s\\base.z64",project);
    for(i=0;i<64*64;i++) { pixels[i]=(TexPixel){(i%4)*64,32,128,(i%3) ? 255 : 0}; }
    assert(TexWriteBmp(a,pixels,8,8));
    /* Extracted ROM images and older GTI2 imports have no remembered source. */
    assert(!ImageEditsReimport(project,3,source,&why) && !source[0] && strstr(why,"Replace image"));
    assert(!ImageEditsHasUnsaved());
    assert(ImageEditsImport(project,pixels,8,8,&o,a,&id,&why) && id==16);
    pixels[0]=(TexPixel){240,64,32,0};assert(TexWriteBmp(a,pixels,8,4));
    assert(ImageEditsReimport(project,id,source,&why) && !strcmp(source,a));
    assert(ImageEditsGetPixels(project,id,out,&w,&h) && w==8 && h==4);
    assert(!memcmp(out,pixels,8*4*sizeof(*pixels)));CheckImage(project,id,8,4,&o);
    assert(ImageEditsNextId(project,&next,&why) && next==17);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    bmphash=FileHash(bmp);nativehash=FileHash(native);
    /* Reopen uses the external source, not the saved/quantized project BMP. */
    pixels[0].g=160;assert(TexWriteBmp(a,pixels,4,8));
    assert(ImageEditsReimport(project,id,source,&why) && !strcmp(source,a));
    assert(ImageEditsGetPixels(project,id,out,&w,&h) && w==4 && h==8 && out[0].g==160);
    CheckImage(project,id,4,8,&o);assert(FileHash(bmp)==bmphash && FileHash(native)==nativehash);
    ImageEditsReset();
    assert(DeleteFile(a));
    assert(!ImageEditsReimport(project,id,source,&why) && strstr(why,"missing") && !strcmp(source,a));
    assert(!ImageEditsHasUnsaved());
    file=fopen(a,"wb");assert(file);assert(fputs("broken BMP",file)>=0);fclose(file);
    assert(!ImageEditsReimport(project,id,source,&why) && strstr(why,"decoded"));
    assert(!ImageEditsHasUnsaved());
    assert(TexWriteBmp(a,pixels,64,64));
    assert(!ImageEditsReimport(project,id,source,&why) && strstr(why,"TMEM"));
    assert(!ImageEditsHasUnsaved());
    assert(TexWriteBmp(a,pixels,1,1));
    assert(!ImageEditsReimport(project,id,source,&why)); /* Saved mip count no longer fits. */
    assert(!ImageEditsHasUnsaved() && FileHash(bmp)==bmphash && FileHash(native)==nativehash);
    CheckImage(project,id,8,4,&o);
    assert(TexWriteBmp(a,pixels,8,8));assert(TexWriteBmp(b,pixels,16,8));
    /* A pending replacement supersedes both the source and the settings. A
     * failed reimport/save must preserve that edit and the older saved version. */
    assert(ImageEditsReplace(project,id,pixels,16,8,&other,b,&why));
    assert(DeleteFile(b));assert(!ImageEditsReimport(project,id,source,&why) && !strcmp(source,b));
    CheckImage(project,id,16,8,&other);assert(ImageEditsHasUnsaved());
    assert(TexWriteBmp(b,pixels,64,64));
    assert(!ImageEditsReimport(project,id,source,&why) && strstr(why,"TMEM"));
    CheckImage(project,id,16,8,&other);
    test_fail_move=3;assert(!ImageEditsSave(project,&why));
    assert(FileHash(bmp)==bmphash && FileHash(native)==nativehash);
    ImageEditsReset();assert(ImageEditsReimport(project,id,source,&why) && !strcmp(source,a));
    CheckImage(project,id,8,8,&o);ImageEditsReset();
    assert(ImageEditsReplace(project,id,pixels,16,8,&other,b,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    pixels[0]=(TexPixel){64,224,0,255};assert(TexWriteBmp(b,pixels,32,16));
    assert(ImageEditsReimport(project,id,source,&why) && !strcmp(source,b));
    CheckImage(project,id,32,16,&other);
    assert(ImageEditsGetPixels(project,id,out,&w,&h) && out[0].g==224);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(ImageEditsImport(project,pixels,8,8,&o,NULL,&next,&why) && next==17);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(!ImageEditsReimport(project,next,source,&why) && !source[0] && strstr(why,"Replace image"));
    assert(!ImageEditsHasUnsaved());
    /* Replacing a base image establishes its source; deletion removes it. */
    assert(ImageEditsReplace(project,3,pixels,8,8,&o,a,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(ImageEditsReimport(project,3,source,&why) && !strcmp(source,a));
    assert(ImageEditsDelete(project,3,&why));assert(!ImageEditsReimport(project,3,source,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(!ImageEditsReimport(project,3,source,&why) && !ImageEditsHasUnsaved());
    /* Both metadata versions export together without shifting image IDs. */
    assert(RomLoad(base,&rom,&why) && ImageEditsExportToRom(project,&rom,&why));
    assert(TexRomReadBank(&rom,&bank,&why) && bank.count==18);RomFree(&rom);
    /* Reject truncated, oversized, unterminated and damaged source metadata. */
    {
        unsigned char bytes[8192],copy[8192];size_t size;
        file=fopen(native,"rb");assert(file);size=fread(bytes,1,sizeof(bytes),file);fclose(file);
        assert(!memcmp(bytes,"GTI3",4));
        for(i=0;i<5;i++)
        {
            size_t length=size;memcpy(copy,bytes,size);
            if(i==0) { copy[32]=255; } /* Unbounded source length. */
            if(i==1) { copy[36]^=1; } /* Source hash. */
            if(i==2) { copy[40]=0; } /* Embedded NUL with otherwise valid checksum. */
            if(i==3) { copy[40+strlen(b)]='x'; }
            if(i==2 || i==3)
            {
                DWORD hash=TexDataHash(copy+40,(DWORD)strlen(b)+1);
                copy[36]=hash>>24;copy[37]=hash>>16;copy[38]=hash>>8;copy[39]=hash;
            }
            if(i==4) { length--; }
            file=fopen(native,"wb");assert(file);assert(fwrite(copy,1,length,file)==length);fclose(file);
            assert(!ImageEditsReimport(project,id,source,&why) && !ImageEditsHasUnsaved());
        }
        file=fopen(native,"wb");assert(file);assert(fwrite(bytes,1,size,file)==size);fclose(file);
    }
    puts("PASS: reimport source/settings persistence, resized pixels, unchanged IDs, legacy metadata, missing/invalid sources, TMEM/mipmap rejection, pending preservation, save rollback and source metadata validation.");
}
