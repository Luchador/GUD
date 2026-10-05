#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imageedits.h"
#include "texrom.h"

static DWORD Word(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static void Level(const unsigned char *data,DWORD size,int level,TexPixel *pixels,int *w,int *h)
{
    unsigned char *copy=malloc(size);assert(copy);memcpy(copy,data,size);
    const unsigned char *desc=data+16+level*12;
    DWORD header=(DWORD)data[10]<<8|data[11],offset=Word(desc+4),bytes=Word(desc+8);
    memcpy(copy+16,desc,12);memmove(copy+header,data+offset,bytes);
    copy[4]=0;copy[5]=copy[6]=1;Put(copy+20,header);Put(copy+12,(header+bytes+15)&~15u);
    assert(TexDecodeRecord(copy,Word(copy+12),pixels,w,h));free(copy);
}
static void CompareFlipAt(const TexPixel *before,const TexPixel *after,int w,int h,BOOL horizontal,int line)
{
    for(int y=0;y<h;y++) for(int x=0;x<w;x++) {
        int from=horizontal ? y*w+w-1-x : (h-1-y)*w+x;
        if(memcmp(before+from,after+y*w+x,sizeof(*before))) {
            fprintf(stderr,"Flip comparison at line %d, %dx%d, axis %d, pixel %d,%d failed.\n",line,w,h,horizontal,x,y);abort();
        }
    }
}
#define CompareFlip(a,b,w,h,axis) CompareFlipAt(a,b,w,h,axis,__LINE__)
static void NativeFlips(void)
{
    TexPixel input[64],before[64],after[64];const char *why;
    for(int i=0;i<64;i++) input[i]=(TexPixel){i*31,i*7,i*13,i*17};
    for(int format=0;format<13;format++) for(int axis=0;axis<2;axis++) {
        TexImportOptions options={format,2,5,11};unsigned char *data;DWORD size;
        assert(TexEncodeRecord(input,7,5,&options,&data,&size,&why));
        unsigned char *original=malloc(size);assert(original);memcpy(original,data,size);
        assert(TexFlipRecord(data,size,axis,&why));
        /* Existing mip images and indexed colors are reflected exactly. */
        for(int level=0;level<3;level++) {
            int w,h,fw,fh;Level(original,size,level,before,&w,&h);Level(data,size,level,after,&fw,&fh);
            assert(w==fw && h==fh);CompareFlip(before,after,w,h,axis);
        }
        assert(!memcmp(data,original,(DWORD)data[10]<<8|data[11])); /* Header and palette. */
        assert(TexFlipRecord(data,size,axis,&why) && !memcmp(data,original,size));
        data[16]=255;memcpy(original,data,size);
        assert(!TexFlipRecord(data,size,axis,&why) && !memcmp(data,original,size));
        free(original);free(data);
    }
    puts("PASS: both flip axes in all 13 formats, odd dimensions, mip pixels/alpha, exact palettes and double-flip byte restoration.");
}
static DWORD ImageOffset(const RomFile *rom,const TexRomBank *bank,DWORD id)
{
    DWORD offset=bank->images;
    for(DWORD i=0;i<id;i++) offset+=Word(rom->data+bank->table+i*8)&0xffffffu;
    return offset;
}
static void Current(const char *project,DWORD id,TexPixel *out,int w,int h)
{ int width,height;assert(TexLoadProjectImage(project,id,out,&width,&height) && width==w && height==h); }
static void Exported(const char *project,DWORD id,const TexPixel *expected,int w,int h,const TexImportOptions *options)
{
    char base[MAX_PATH];RomFile rom={0};TexRomBank bank;TexInfoRecord info;
    TexPixel pixels[256*256];const char *why;int width,height;
    snprintf(base,sizeof(base),"%s\\base.z64",project);
    assert(RomLoad(base,&rom,&why) && ImageEditsExportToRom(project,&rom,&why) && TexRomReadBank(&rom,&bank,&why));
    DWORD offset=ImageOffset(&rom,&bank,id);
    assert(TexInfoReadRecord(rom.data+offset,rom.size-offset,&info));
    assert(info.info.format==options->format && info.info.mipmaps==options->mipmaps);
    assert(rom.data[bank.table+id*8]==(options->hitsound<<4|options->hittexture));
    assert(TexDecodeRecord(rom.data+offset,info.size,pixels,&width,&height));
    assert(width==w && height==h && !memcmp(pixels,expected,w*h*sizeof(*pixels)));
    RomFree(&rom);
}
void CheckFlips(const char *project)
{
    TexPixel original[64],horizontal[64],both[64],out[64],saved[64];
    char path[MAX_PATH],base[MAX_PATH],source[MAX_PATH],remembered[MAX_PATH];const char *why;
    TexImportOptions stock={1,0,1,2},custom={10,2,5,11};DWORD id,next;int w,h;
    NativeFlips();
    snprintf(base,sizeof(base),"%s\\base.z64",project);RomFile rom={0};TexRomBank bank;
    assert(RomLoad(base,&rom,&why) && TexRomReadBank(&rom,&bank,&why));
    DWORD basehash=TexDataHash(rom.data,rom.size),detail=Word(rom.data+bank.table+4);
    RomFree(&rom);
    Current(project,0,original,4,4);
    assert(ImageEditsFlip(project,0,TRUE,&why) && ImageEditsHasUnsaved());
    Current(project,0,horizontal,4,4);CompareFlip(original,horizontal,4,4,TRUE);
    assert(TexLoadSavedProjectImage(project,0,out,&w,&h) && !memcmp(original,out,16*sizeof(*out)));
    assert(ImageEditsFlip(project,0,FALSE,&why));Current(project,0,both,4,4);CompareFlip(horizontal,both,4,4,FALSE);
    /* Thumbnail pixels use display orientation; horizontal still means left/right. */
    TexThumb *thumbs;unsigned char *thumbpixels;DWORD count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);
    assert(count==16 && !strcmp(thumbs[0].label,"0000") && thumbs[0].w==4 && thumbs[0].h==4);
    for(int y=0;y<4;y++) for(int x=0;x<4;x++) {
        TexPixel p=both[(3-y)*4+x];const unsigned char *actual=thumbpixels+thumbs[0].pixeloffset+(y*TEX_THUMB_MAX+x)*4;
        assert(actual[0]==p.b && actual[1]==p.g && actual[2]==p.r && actual[3]==p.a);
    }
    free(thumbs);free(thumbpixels);
    assert(ImageEditsSave(project,&why));ImageEditsReset();Current(project,0,out,4,4);
    assert(!memcmp(both,out,16*sizeof(*out)));Exported(project,0,both,4,4,&stock);
    assert(ImageEditsNextId(project,&next,&why) && next==16);
    assert(ImageEditsFlip(project,0,TRUE,&why) && ImageEditsFlip(project,0,FALSE,&why));
    Current(project,0,out,4,4);assert(!memcmp(original,out,16*sizeof(*out)));
    test_fail_move=3;assert(!ImageEditsSave(project,&why) && ImageEditsHasUnsaved());
    assert(TexLoadSavedProjectImage(project,0,saved,&w,&h) && !memcmp(both,saved,16*sizeof(*out)));
    assert(ImageEditsSave(project,&why));ImageEditsReset();Exported(project,0,original,4,4,&stock);
    /* Pending imported CI4 image: preserve source and settings, then reopen. */
    for(int i=0;i<35;i++) original[i]=(TexPixel){i*17,i*11,i*3,i*31};
    snprintf(source,sizeof(source),"%s\\head source.bmp",project);assert(TexWriteBmp(source,original,7,5));
    assert(ImageEditsImport(project,original,7,5,&custom,source,&id,&why) && id==16);
    Current(project,id,original,7,5);
    assert(ImageEditsFlip(project,id,TRUE,&why));Current(project,id,horizontal,7,5);CompareFlip(original,horizontal,7,5,TRUE);
    assert(ImageEditsSave(project,&why));ImageEditsReset();Current(project,id,out,7,5);
    assert(!memcmp(horizontal,out,35*sizeof(*out)));Exported(project,id,horizontal,7,5,&custom);
    assert(ImageEditsFlip(project,id,FALSE,&why));Current(project,id,both,7,5);CompareFlip(horizontal,both,7,5,FALSE);
    assert(ImageEditsSave(project,&why));ImageEditsReset();Exported(project,id,both,7,5,&custom);
    assert(ImageEditsReimport(project,id,remembered,&why) && !strcmp(remembered,source));
    Current(project,id,out,7,5);assert(!memcmp(original,out,35*sizeof(*out)));ImageEditsReset();
    /* External edits to a saved BMP must be flipped using its remembered settings. */
    snprintf(path,sizeof(path),"%s\\images\\0010.bmp",project);
    assert(TexWriteBmp(path,original,7,5));assert(ImageEditsFlip(project,id,FALSE,&why));
    unsigned char *converted;DWORD convertedsize;
    assert(TexEncodeRecord(original,7,5,&custom,&converted,&convertedsize,&why));
    assert(TexDecodeRecord(converted,convertedsize,saved,&w,&h));free(converted);
    Current(project,id,out,7,5);CompareFlip(saved,out,7,5,FALSE);
    assert(ImageEditsSave(project,&why));ImageEditsReset();Exported(project,id,out,7,5,&custom);
    assert(ImageEditsDelete(project,1,&why) && !ImageEditsFlip(project,1,TRUE,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(!ImageEditsFlip(project,1,FALSE,&why) && !ImageEditsFlip(project,4095,TRUE,&why) && !ImageEditsHasUnsaved());
    snprintf(path,sizeof(path),"%s\\images\\0002.bmp",project);assert(DeleteFile(path));
    assert(!ImageEditsFlip(project,2,FALSE,&why) && !ImageEditsHasUnsaved());
    assert(RomLoad(base,&rom,&why) && TexDataHash(rom.data,rom.size)==basehash);
    assert(ImageEditsExportToRom(project,&rom,&why) && TexRomReadBank(&rom,&bank,&why));
    assert(Word(rom.data+bank.table+4)==detail);
    /* Matching pixels AND native detail flags must rebase without conflicts. */
    RomFile oldrom={0};assert(RomLoad(base,&oldrom,&why));
    assert(ImageEditsRebase(project,&oldrom,&rom,PROJECT_REBASE_STOP,NULL,FALSE,&why));
    assert(ImageEditsRebase(project,&oldrom,&rom,PROJECT_REBASE_STOP,NULL,TRUE,&why));
    FILE *file=fopen(base,"wb");assert(file && fwrite(rom.data,1,rom.size,file)==rom.size && !fclose(file));
    RomFree(&oldrom);RomFree(&rom);
    assert(RomLoad(base,&rom,&why) && ImageEditsExportToRom(project,&rom,&why) && TexRomReadBank(&rom,&bank,&why));
    assert(Word(rom.data+bank.table+4)==detail);RomFree(&rom);
    puts("PASS: flip pending/base/saved images, thumbnails, alpha, external edits, unchanged IDs/settings/source, save/reopen/ROM export, save rollback and invalid/deleted-image rejection.");
}

void CheckGeneratedMipFlip(const char *project)
{
    char base[MAX_PATH];RomFile rom={0};TexRomBank bank;const char *why;
    TexPixel before[16],after[16];TexImportOptions options={1,2,1,2};
    snprintf(base,sizeof(base),"%s\\base.z64",project);
    assert(RomLoad(base,&rom,&why) && TexRomReadBank(&rom,&bank,&why));
    rom.data[bank.images+4]=0;rom.data[bank.images+5]=3;rom.data[bank.images+6]=1;
    FILE *file=fopen(base,"wb");assert(file && fwrite(rom.data,1,rom.size,file)==rom.size && !fclose(file));RomFree(&rom);
    Current(project,0,before,4,4);assert(ImageEditsFlip(project,0,TRUE,&why));
    Current(project,0,after,4,4);CompareFlip(before,after,4,4,TRUE);
    assert(ImageEditsSave(project,&why));ImageEditsReset();Exported(project,0,after,4,4,&options);
    puts("PASS: stock runtime-generated mipmaps retain their effective count and format after flip/save/export.");
}
