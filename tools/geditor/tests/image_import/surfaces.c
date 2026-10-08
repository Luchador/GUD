#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imageedits.h"
#include "texrom.h"

DWORD TestTextureRam(const unsigned char *data);
static DWORD Word(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Put(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static DWORD Offset(const RomFile *rom,const TexRomBank *bank,DWORD id)
{ DWORD at=bank->images;for(DWORD i=0;i<id;i++) at+=Word(rom->data+bank->table+i*8)&0xffffffu;return at; }
static DWORD Hash(const char *path)
{
    FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));long size=ftell(f);rewind(f);
    unsigned char *data=malloc(size);assert(data && fread(data,1,size,f)==(size_t)size);fclose(f);
    DWORD hash=TexDataHash(data,size);free(data);return hash;
}
static void WriteRom(const char *path,const RomFile *rom)
{ FILE *f=fopen(path,"wb");assert(f && fwrite(rom->data,1,rom->size,f)==rom->size && !fclose(f)); }
static void ExportSamePixels(const char *project,const RomFile *before,DWORD id,unsigned sound,unsigned bullet)
{
    char path[MAX_PATH];const char *why;RomFile rom={0};TexRomBank old,bank;
    snprintf(path,sizeof(path),"%s\\base.z64",project);
    assert(RomLoad(path,&rom,&why) && ImageEditsExportToRom(project,&rom,&why));
    assert(TexRomReadBank(&rom,&bank,&why) && TexRomReadBank(before,&old,&why));
    DWORD size=Word(before->data+old.table+id*8)&0xffffffu;
    assert((Word(rom.data+bank.table+id*8)&0xffffffu)==size);
    assert(!memcmp(rom.data+Offset(&rom,&bank,id),before->data+Offset(before,&old,id),size));
    assert(rom.data[bank.table+id*8]==(sound<<4|bullet));
    assert(Word(rom.data+bank.table+id*8+4)==Word(before->data+old.table+id*8+4));
    RomFree(&rom);
}
static void Info(const char *project,DWORD id,unsigned sound,unsigned bullet)
{
    TexThumb *thumbs=NULL;unsigned char *pixels=NULL;const char *why;char text[384];
    DWORD count=TexLoadProjectThumbnails(project,&thumbs,&pixels,&why),found=0;
    for(DWORD i=0;i<count;i++) if(strtoul(thumbs[i].label,NULL,16)==id) {
        assert(thumbs[i].info.valid && thumbs[i].info.surfacevalid && thumbs[i].info.memorybytes>=32);
        assert(thumbs[i].info.hitsound==sound && thumbs[i].info.hittexture==bullet);
        TexFormatThumbnailInfo(&thumbs[i],text,sizeof(text));
        assert(strstr(text,"Texture RAM:") && strstr(text,"bytes (") && strstr(text,"KiB)"));
        assert(strstr(text,TexInfoSurfaceName(sound)) && strstr(text,TexInfoSurfaceName(bullet)));found++;
    }
    assert(found==1);free(thumbs);free(pixels);
}
void CheckSurfaceProperties(const char *project)
{
    TexPixel pixels[256*256];unsigned char *records[16]={0},surfaces[16]={0};DWORD sizes[16]={0};
    const char *why;BOOL changed;char base[MAX_PATH],bmp[MAX_PATH],native[MAX_PATH],path[MAX_PATH];
    RomFile original={0};TexRomBank bank;
    snprintf(base,sizeof(base),"%s\\base.z64",project);
    assert(RomLoad(base,&original,&why) && TexRomReadBank(&original,&bank,&why));
    for(int i=0;i<256*256;i++) pixels[i]=(TexPixel){(i%4)*64,(i%4)*32,248,255};
    for(int i=0;i<4;i++) {
        TexImportOptions o={i==1?9:1,i==2?1:0,1,2};
        assert(TexEncodeRecord(pixels,i==1?64:4,i==1?32:4,&o,&records[i],&sizes[i],&why));
        surfaces[i]=0x12;
    }
    records[0][4]=0;records[0][5]=4; /* Generated, including native row padding. */
    records[1][4]=0;records[1][5]=7; /* Palette mip cap: base already fills 2048 bytes. */
    records[2][28]=7; /* Explicit I8 mip after RGBA16 base; equal padded byte size. */
    records[3][5]=0; /* Special explicit zero-LOD bank. */
    for(int i=0;i<4;i++) {
        TexInfoRecord info;assert(TexInfoReadRecord(records[i],sizes[i],&info));
        assert(info.info.memorybytes==TestTextureRam(records[i]));
        if(i==0) assert(info.info.memorybytes==32+16+8+8+24 && info.info.generatedmipmaps);
        if(i==1) assert(info.info.memorybytes==2048+8+24 && !info.info.mipmaps);
        if(i==3) assert(info.info.memorybytes==32+24);
    }
    assert(TexRomUpdateImages(&original,&bank,(const unsigned char *const *)records,sizes,surfaces,16,&why));
    assert(TexRomReadBank(&original,&bank,&why));
    for(int i=0;i<4;i++) {
        int w,h;Put(original.data+bank.table+i*8+4,0x38d20000u+i);
        assert(TexDecodeRecord(records[i],sizes[i],pixels,&w,&h));
        snprintf(path,sizeof(path),"%s\\images\\%04X.bmp",project,i);assert(TexWriteBmp(path,pixels,w,h));free(records[i]);
    }
    WriteRom(base,&original);DWORD basehash=Hash(base);
    for(DWORD id=0;id<4;id++) {
        snprintf(bmp,sizeof(bmp),"%s\\images\\%04lX.bmp",project,(unsigned long)id);DWORD bmphash=Hash(bmp);
        assert(ImageEditsSetSurface(project,id,TRUE,1,&changed,&why) && !changed && !ImageEditsHasUnsaved());
        assert(!ImageEditsSetSurface(project,id,TRUE,13,&changed,&why) && !changed && !ImageEditsHasUnsaved());
        assert(ImageEditsSetSurface(project,id,TRUE,3,&changed,&why) && changed);
        assert(ImageEditsSetSurface(project,id,FALSE,4,&changed,&why) && changed);Info(project,id,3,4);
        ExportSamePixels(project,&original,id,1,2); /* Export uses saved values. */
        assert(Hash(bmp)==bmphash);ImageEditsReset();Info(project,id,1,2);
        assert(ImageEditsSetSurface(project,id,TRUE,3,&changed,&why));
        assert(ImageEditsSetSurface(project,id,FALSE,4,&changed,&why));
        test_fail_move=1;assert(!ImageEditsSave(project,&why) && ImageEditsHasUnsaved() && Hash(bmp)==bmphash);
        assert(ImageEditsSave(project,&why));ImageEditsReset();Info(project,id,3,4);
        assert(Hash(bmp)==bmphash && Hash(base)==basehash);ExportSamePixels(project,&original,id,3,4);
    }
    /* Every supported category is independent and repeat clicks are no-ops. */
    for(unsigned type=0;type<=12;type++) {
        assert(ImageEditsSetSurface(project,0,TRUE,type,&changed,&why) && changed);Info(project,0,type,4);
        assert(ImageEditsSetSurface(project,0,TRUE,type,&changed,&why) && !changed);
    }
    ImageEditsReset();
    /* Preserve external BMP edits both in the preview and on disk. */
    snprintf(bmp,sizeof(bmp),"%s\\images\\0005.bmp",project);
    assert(TexLoadSavedProjectImage(project,5,pixels,(int[]){0},(int[]){0}));pixels[0].r=248;
    assert(TexWriteBmp(bmp,pixels,4,4));DWORD bmphash=Hash(bmp);
    assert(ImageEditsSetSurface(project,5,FALSE,9,&changed,&why));
    TexPixel preview[16];int w,h;assert(TexLoadProjectImage(project,5,preview,&w,&h) && !memcmp(pixels,preview,sizeof(preview)));
    assert(ImageEditsSave(project,&why));ImageEditsReset();assert(Hash(bmp)==bmphash);
    RomFile output={0};assert(RomLoad(base,&output,&why) && ImageEditsExportToRom(project,&output,&why));
    assert(TexRomReadBank(&output,&bank,&why));
    assert(output.data[bank.table+5*8]==0x19);
    assert(TexDecodeRecord(output.data+Offset(&output,&bank,5),output.size-Offset(&output,&bank,5),preview,&w,&h));
    assert(!memcmp(pixels,preview,sizeof(preview)));RomFree(&output);
    /* Pending imports retain their pixels/source; a saved imported asset can
     * then receive a metadata-only update without changing its BMP. */
    DWORD id;TexImportOptions options={1,1,1,2};
    snprintf(path,sizeof(path),"%s\\original source.bmp",project);assert(TexWriteBmp(path,pixels,4,4));
    assert(ImageEditsImport(project,pixels,4,4,&options,path,&id,&why) && id==16);
    assert(ImageEditsSetSurface(project,id,TRUE,11,&changed,&why));Info(project,id,11,2);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    snprintf(bmp,sizeof(bmp),"%s\\images\\0010.bmp",project);bmphash=Hash(bmp);
    assert(ImageEditsSetSurface(project,id,FALSE,12,&changed,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();Info(project,id,11,12);assert(Hash(bmp)==bmphash);
    char source[MAX_PATH];assert(ImageEditsReimport(project,id,source,&why) && !strcmp(source,path));Info(project,id,11,12);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(ImageEditsSetSurface(project,id,TRUE,2,&changed,&why));
    assert(ImageEditsFlip(project,id,TRUE,&why));Info(project,id,2,12);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(ImageEditsDelete(project,id,&why));assert(!ImageEditsSetSurface(project,id,TRUE,1,&changed,&why));ImageEditsReset();
    assert(!ImageEditsSetSurface(project,4000,FALSE,0,&changed,&why));
    /* Metadata-only overrides survive a conflicting base rebase, including
     * raw mips and detail flags, under Keep Project. */
    assert(RomLoad(base,&output,&why) && TexRomReadBank(&output,&bank,&why));output.data[bank.table]=0x56;
    assert(!ImageEditsRebase(project,&original,&output,PROJECT_REBASE_STOP,NULL,FALSE,&why));
    assert(ImageEditsRebase(project,&original,&output,PROJECT_REBASE_KEEP_PROJECT,NULL,TRUE,&why));
    WriteRom(base,&output);RomFree(&output);
    ExportSamePixels(project,&original,0,3,4);Info(project,0,3,4);
    /* A later metadata save failure leaves both old metadata and pixels. */
    snprintf(native,sizeof(native),"%s\\images\\native\\0000.gtex",project);DWORD nativehash=Hash(native);
    assert(ImageEditsSetSurface(project,0,FALSE,10,&changed,&why));test_fail_move=1;
    assert(!ImageEditsSave(project,&why) && Hash(native)==nativehash);Info(project,0,3,10);
    ImageEditsReset();Info(project,0,3,4);RomFree(&original);
    TexThumb unavailable={0};char tooltip[384];TexFormatThumbnailInfo(&unavailable,tooltip,sizeof(tooltip));
    assert(strstr(tooltip,"Texture RAM: Unavailable"));
    puts("PASS: texture RAM agrees with runtime allocation; generated/capped/mixed/zero-LOD records retain exact bytes, details and BMPs through surface edits, save/reopen/export and rebase.");
    puts("PASS: independent surface categories, no-op/discard, saved-only export, failure/retry, external BMPs, pending/saved imports, source retention, reimport/flip and invalid/deleted IDs.");
}
