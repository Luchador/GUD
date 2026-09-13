#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "texrom.h"
#include "texencode.h"
#include "imageedits.h"
#include "gltf.h"
#define BASE_COUNT 16u
#define MANIFEST 0x110020u
#define CONFIG 0x110010u
#define TABLE 0x111000u
#define IMAGES 0x102000u
#define CMAP_END (TABLE+(TEX_IMAGE_CAPACITY+1)*8)
static DWORD Read32(const unsigned char *p) {return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3];}
static void Write32(unsigned char *p,DWORD v) {p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
/* Isolate the image pipeline from the independently tested level-table parser. */
BOOL RomLoad(const char *path,RomFile *rom,const char **why)
{
    FILE *f=fopen(path,"rb");long size;DWORD i;
    memset(rom,0,sizeof(*rom));*why="test base unavailable";if(!f) return FALSE;
    assert(!fseek(f,0,SEEK_END));size=ftell(f);rewind(f);rom->data=malloc(size);rom->size=rom->info.size=size;
    assert(fread(rom->data,1,size,f)==(size_t)size);fclose(f);
    rom->info.manifestoffset=MANIFEST;rom->info.entrycount=Read32(rom->data+MANIFEST+20);
    for(i=0;i<rom->info.entrycount;i++) {const unsigned char *p=rom->data+MANIFEST+24+i*16;rom->info.entries[i]=(RomManifestEntry){Read32(p),Read32(p+4),Read32(p+8),Read32(p+12)};}
    *why="";return TRUE;
}
void RomFree(RomFile *rom) {free(rom->data);memset(rom,0,sizeof(*rom));}
static TexPixel source[256*256],decoded[256*256];
static TexPixel Expected(TexPixel p,int format)
{
    unsigned int intensity=(77u*p.r+150u*p.g+29u*p.b+128)>>8;
    if(format==1 || format==3 || format==9 || format==10) {p.r&=248;p.g&=248;p.b&=248;p.a=p.a>=128 ? 255 : 0;}
    if(format==4 || format==11 || format==12) {p.r=p.g=p.b=intensity;}
    if(format==5) {p.r=p.g=p.b=(intensity>>4)*17;p.a=(p.a>>4)*17;}
    if(format==6) {p.r=p.g=p.b=((intensity>>5)*255)/7;p.a=p.a>=128 ? 255 : 0;}
    if(format==7) {p.r=p.g=p.b=intensity;}
    if(format==8) {p.r=p.g=p.b=(intensity>>4)*17;}
    if(format==2 || format==3 || format==7 || format==8) p.a=255;
    return p;
}
static void Encoders(void)
{
    const int widths[]={1,3,4,7,8,9,15,16,17,31,32,63,64,127,255};
    const TexPixel colors[]={{0,64,128,0},{248,0,64,255},{64,248,0,180},{128,64,248,80}};
    int format,mips,w,h=7,i;unsigned int wi,cases=0;
    for(i=0;i<256*256;i++) source[i]=colors[i%4];
    for(format=0;format<13;format++) for(wi=0;wi<sizeof(widths)/sizeof(widths[0]);wi++)
    {
        w=widths[wi];
        for(mips=0;mips<=TexImportMaxMipmaps(w,h);mips++)
        {
            TexImportOptions options={format,mips,1,2};unsigned char *data=NULL;DWORD size,bytes;const char *why="";
            TexInfoRecord info;int dw,dh,level;
            bytes=TexImportTmemBytes(w,h,format,mips);
            if(bytes>TexImportTmemLimit(format)) {assert(!TexEncodeRecord(source,w,h,&options,&data,&size,&why));assert(!data);continue;}
            assert(TexEncodeRecord(source,w,h,&options,&data,&size,&why));
            assert(TexInfoReadRecord(data,size,&info)&&info.size==size&&info.info.format==format&&info.info.mipmaps==mips);
            assert(data[4]==1&&data[5]==mips+1&&data[6]==mips+1);
            assert(TexDecodeRecord(data,size,decoded,&dw,&dh)&&dw==w&&dh==h);
            /* Mips may require extra palette entries; a base-only four-color image is exact. */
            if(mips==0) for(i=0;i<w*h;i++) {TexPixel expected=Expected(source[i],format);assert(!memcmp(&decoded[i],&expected,sizeof(expected)));}
            for(level=0;level<=mips;level++)
            {
                unsigned char copy[8192];const unsigned char *desc=data+16+level*12;
                memcpy(copy,data,size);memcpy(copy+16,desc,12);copy[4]=0;copy[5]=copy[6]=1;
                /* Use a standalone record to verify every mip/palette index through the decoder. */
                {DWORD header=(DWORD)copy[10]<<8|copy[11],off=Read32(desc+4),length=Read32(desc+8);memmove(copy+header,copy+off,length);Write32(copy+20,header);Write32(copy+12,(header+length+15)&~15u);}
                assert(TexDecodeRecord(copy,Read32(copy+12),decoded,&dw,&dh));
            }
            free(data);cases++;
        }
    }
    /* More unique colors than a palette can store must quantize safely. */
    for(i=0;i<64*32;i++) source[i]=(TexPixel){i%256,(i*7)%256,(i*19)%256,(i%5) ? 255 : 0};
    for(format=9;format<13;format++)
    {TexImportOptions o={format,0,1,1};unsigned char *data;DWORD size;const char *why;assert(TexEncodeRecord(source,64,32,&o,&data,&size,&why));assert(TexDecodeRecord(data,size,decoded,&w,&h));free(data);}
    assert(TexImportTmemBytes(32,32,0,0)==4096);
    assert(TexImportTmemBytes(32,32,0,1)>4096);
    assert(TexImportTmemBytes(64,32,9,0)==2048);
    assert(TexImportTmemBytes(64,32,9,1)>2048);
    assert(!TexImportTmemBytes(256,1,8,0));
    assert(!TexImportTmemBytes(1,1,1,1));
    printf("PASS: %u native format/mipmap combinations, row padding, palette quantization and TMEM limits.\n",cases);
}
static void Fixture(const char *project)
{
    RomFile rom={0};TexImportOptions o={1,0,1,1};unsigned char *record;DWORD size,i,cursor=IMAGES;char path[MAX_PATH];FILE *f;const char *why;
    for(i=0;i<16;i++) source[i]=(TexPixel){i*8,64,128,255};
    assert(TexEncodeRecord(source,4,4,&o,&record,&size,&why));rom.size=2*1024*1024;rom.data=calloc(rom.size,1);
    rom.info.entrycount=4;rom.info.entries[0]=(RomManifestEntry){0x494d4753,IMAGES,IMAGES+BASE_COUNT*size,0};
    rom.info.entries[1]=(RomManifestEntry){0x434d4150,0x110000,CMAP_END,0x80000000};
    rom.info.entries[2]=(RomManifestEntry){0x54585442,TABLE,TABLE+(TEX_IMAGE_CAPACITY+1)*8,TEX_IMAGE_CAPACITY};
    rom.info.entries[3]=(RomManifestEntry){0x54584346,CONFIG,CONFIG+8,1};
    memcpy(rom.data+MANIFEST,"GUDGEDITORMANIF",16);Write32(rom.data+MANIFEST+16,2);Write32(rom.data+MANIFEST+20,4);
    for(i=0;i<4;i++) {unsigned char *p=rom.data+MANIFEST+24+i*16;Write32(p,rom.info.entries[i].kind);Write32(p+4,rom.info.entries[i].romstart);Write32(p+8,rom.info.entries[i].romend);Write32(p+12,rom.info.entries[i].flags);}
    Write32(rom.data+CONFIG,IMAGES);Write32(rom.data+CONFIG+4,BASE_COUNT);
    assert(CreateDirectory(project,NULL));snprintf(path,sizeof(path),"%s\\images",project);assert(CreateDirectory(path,NULL));
    for(i=0;i<BASE_COUNT;i++) {memcpy(rom.data+cursor,record,size);cursor+=size;Write32(rom.data+TABLE+i*8,0x12000000|size);Write32(rom.data+TABLE+i*8+4,0xabcd0000);snprintf(path,sizeof(path),"%s\\images\\%04lX.bmp",project,(unsigned long)i);assert(TexDecodeRecord(record,size,decoded,(int[]){0},(int[]){0}));assert(TexWriteBmp(path,decoded,4,4));}
    Write32(rom.data+TABLE+BASE_COUNT*8,0xffff);snprintf(path,sizeof(path),"%s\\base.z64",project);f=fopen(path,"wb");assert(f&&fwrite(rom.data,1,rom.size,f)==rom.size&&!fclose(f));free(record);RomFree(&rom);
}
static void Pipeline(const char *project)
{
    const char *why="";char basepath[MAX_PATH],path[MAX_PATH],renamed[MAX_PATH];DWORD id,i,basehash;int w,h;
    RomFile rom;TexRomBank bank,after;TexThumb *thumbs=NULL;unsigned char *thumbpixels=NULL;DWORD count;
    snprintf(basepath,sizeof(basepath),"%s\\base.z64",project);
    assert(RomLoad(basepath,&rom,&why));assert(TexRomReadBank(&rom,&bank,&why));basehash=TexDataHash(rom.data,rom.size);
    assert(ImageEditsNextId(project,&id,&why)&&id==BASE_COUNT);
    for(i=0;i<64;i++) source[i]=(TexPixel){(i%4)*64,(i%3)*64,(i%5)*32,(i%2)*255};
    {TexImportOptions o={1,0,1,1};
     assert(ImageEditsImport(project,source,8,8,&o,&id,&why)&&ImageEditsHasUnsaved());
     ImageEditsReset();assert(!ImageEditsHasUnsaved());
     assert(ImageEditsNextId(project,&id,&why)&&id==BASE_COUNT);}
    for(i=0;i<13;i++)
    {
        TexImportOptions o={i,2,i,12-i};
        assert(ImageEditsImport(project,source,8,8,&o,&id,&why)&&id==BASE_COUNT+i);
        assert(TexGetProjectImageSize(project,id,&w,&h)&&w==8&&h==8);
        assert(TexLoadProjectImage(project,id,decoded,&w,&h));
    }
    assert(ImageEditsHasUnsaved());
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);
    assert(count==BASE_COUNT+13);
    for(i=0;i<13;i++) {assert(strtoul(thumbs[BASE_COUNT+i].label,NULL,16)==BASE_COUNT+i);assert(thumbs[BASE_COUNT+i].info.format==i);assert(thumbs[BASE_COUNT+i].info.hitsound==i&&thumbs[BASE_COUNT+i].info.hittexture==12-i);}
    /* Orientation: top-left thumbnail is the last native texel, not a mirror. */
    assert(TexLoadProjectImage(project,BASE_COUNT,decoded,&w,&h));
    {unsigned char *p=thumbpixels+thumbs[BASE_COUNT].pixeloffset;assert(p[0]==decoded[63].b&&p[1]==decoded[63].g&&p[2]==decoded[63].r);}
    free(thumbs);free(thumbpixels);
    assert(ImageEditsExportToRom(project,&rom,&why)&&TexDataHash(rom.data,rom.size)==basehash);
    test_fail_move=1;assert(!ImageEditsSave(project,&why)&&ImageEditsHasUnsaved());
    test_fail_move=2;assert(!ImageEditsSave(project,&why)&&ImageEditsHasUnsaved());
    assert(ImageEditsSave(project,&why)&&!ImageEditsHasUnsaved());ImageEditsReset();
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);assert(count==BASE_COUNT+13);
    for(i=0;i<13;i++) {assert(thumbs[BASE_COUNT+i].info.valid&&thumbs[BASE_COUNT+i].info.surfacevalid&&thumbs[BASE_COUNT+i].info.format==i);}
    free(thumbs);free(thumbpixels);
    assert(ImageEditsNextId(project,&id,&why)&&id==BASE_COUNT+13);
    assert(ImageEditsExportToRom(project,&rom,&why));assert(TexRomReadBank(&rom,&after,&why)&&after.count==BASE_COUNT+13);
    assert(after.images>=CMAP_END&&after.images!=bank.images);
    assert(!memcmp(rom.data+bank.images,rom.data+after.images,bank.imagebytes));
    for(i=0;i<BASE_COUNT;i++) assert(Read32(rom.data+TABLE+i*8+4)==0xabcd0000);
    {DWORD offset=after.images+bank.imagebytes;for(i=0;i<13;i++) {assert(rom.data[TABLE+(BASE_COUNT+i)*8]==((i<<4)|(12-i)));assert(TexDecodeRecord(rom.data+offset,rom.size-offset,decoded,&w,&h));offset+=Read32(rom.data+offset+12);}}
    RomFree(&rom);assert(RomLoad(basepath,&rom,&why)&&TexDataHash(rom.data,rom.size)==basehash);
    /* Imported BMP edits are re-encoded with the saved format and mip settings. */
    for(i=0;i<64;i++) { source[i]=(TexPixel){255,12,34,255}; }
    snprintf(path,sizeof(path),"%s\\images\\0010.bmp",project);assert(TexWriteBmp(path,source,8,8));
    assert(ImageEditsExportToRom(project,&rom,&why));assert(TexRomReadBank(&rom,&after,&why));
    assert(TexDecodeRecord(rom.data+after.images+bank.imagebytes,rom.size-after.images-bank.imagebytes,decoded,&w,&h));assert(decoded[0].r==255&&decoded[0].g==12);
    RomFree(&rom);
    /* Gaps and missing BMPs must fail rather than silently renumber images. */
    snprintf(path,sizeof(path),"%s\\images\\native\\0012.gtex",project);snprintf(renamed,sizeof(renamed),"%s\\images\\native\\0012.hold",project);assert(MoveFileEx(path,renamed,0));
    assert(RomLoad(basepath,&rom,&why));assert(!ImageEditsExportToRom(project,&rom,&why));RomFree(&rom);assert(MoveFileEx(renamed,path,0));
    snprintf(path,sizeof(path),"%s\\images\\0011.bmp",project);snprintf(renamed,sizeof(renamed),"%s\\images\\0011.hold",project);assert(MoveFileEx(path,renamed,0));
    assert(RomLoad(basepath,&rom,&why));assert(!ImageEditsExportToRom(project,&rom,&why));RomFree(&rom);assert(MoveFileEx(renamed,path,0));
    /* Exercise expansion and the last legal image ID without changing base files. */
    assert(RomLoad(basepath,&rom,&why));rom.data[rom.size-1]=0x75;
    assert(ImageEditsExportToRom(project,&rom,&why)&&rom.size==4*1024*1024);assert(TexRomReadBank(&rom,&after,&why));RomFree(&rom);
    assert(RomLoad(basepath,&rom,&why));rom.data=realloc(rom.data,64u*1024*1024);memset(rom.data+rom.size,0,64u*1024*1024-rom.size);rom.size=rom.info.size=64u*1024*1024;rom.data[rom.size-1]=1;
    assert(!ImageEditsExportToRom(project,&rom,&why));RomFree(&rom);
    puts("PASS: pending imports, BMP orientation, thumbnails, save failure/retry, reopen, surface settings, preserved originals, edited BMPs, missing assets, ROM relocation/growth and 64 MB limit.");
}

static void Limits(const char *project)
{
    char path[MAX_PATH];const char *why="";RomFile rom;TexRomBank bank,after;
    const unsigned char **records;DWORD *sizes,count,i,hash;
    unsigned char *surfaces,*record;DWORD size;TexImportOptions options={1,0,1,1};
    snprintf(path,sizeof(path),"%s\\base.z64",project);
    assert(RomLoad(path,&rom,&why));assert(TexRomReadBank(&rom,&bank,&why));
    hash=TexDataHash(rom.data,rom.size);
    rom.info.entries[2].romend--;assert(!TexRomReadBank(&rom,&after,&why));rom.info.entries[2].romend++;
    Write32(rom.data+CONFIG+4,4097);assert(!TexRomReadBank(&rom,&after,&why));Write32(rom.data+CONFIG+4,BASE_COUNT);
    Write32(rom.data+CONFIG,IMAGES+16);assert(!TexRomReadBank(&rom,&after,&why));Write32(rom.data+CONFIG,IMAGES);
    Write32(rom.data+TABLE+BASE_COUNT*8,0);assert(!TexRomReadBank(&rom,&after,&why));Write32(rom.data+TABLE+BASE_COUNT*8,0xffff);
    assert(TexDataHash(rom.data,rom.size)==hash);
    count=TEX_IMAGE_CAPACITY-BASE_COUNT;
    assert(TexEncodeRecord(source,4,4,&options,&record,&size,&why));
    records=malloc((count+1)*sizeof(*records));sizes=malloc((count+1)*sizeof(*sizes));surfaces=malloc(count+1);
    for(i=0;i<=count;i++) {records[i]=record;sizes[i]=size;surfaces[i]=0x12;}
    assert(!TexRomAppendImages(&rom,&bank,records,sizes,surfaces,count+1,&why));assert(TexDataHash(rom.data,rom.size)==hash);
    assert(TexRomAppendImages(&rom,&bank,records,sizes,surfaces,count,&why));
    assert(TexRomReadBank(&rom,&after,&why)&&after.count==4096);
    assert((Read32(rom.data+TABLE+4095*8)&0xffffff)==size);
    assert(Read32(rom.data+TABLE+4096*8)==0xffff);
    assert(!TexRomAppendImages(&rom,&after,records,sizes,surfaces,1,&why));
    free(records);free(sizes);free(surfaces);free(record);RomFree(&rom);
    /* A checksum failure in a saved native asset must stop the build. */
    snprintf(path,sizeof(path),"%s\\images\\native\\0010.gtex",project);
    {FILE *f=fopen(path,"rb+");int byte;assert(f);assert(!fseek(f,32,SEEK_SET));byte=fgetc(f);assert(!fseek(f,32,SEEK_SET));assert(fputc(byte^1,f)!=EOF);fclose(f);}
    snprintf(path,sizeof(path),"%s\\base.z64",project);assert(RomLoad(path,&rom,&why));
    assert(!ImageEditsExportToRom(project,&rom,&why));RomFree(&rom);
    puts("PASS: malformed manifest/config/table checks, all 4096 IDs, capacity sentinel and corrupt saved-image rejection.");
}


static DWORD FileHash(const char *path)
{
    FILE *file=fopen(path,"rb");unsigned char bytes[8192];size_t length;
    assert(file);length=fread(bytes,1,sizeof(bytes),file);assert(feof(file)&&!ferror(file));fclose(file);
    return TexDataHash(bytes,(DWORD)length);
}
static DWORD ImageOffset(const RomFile *rom,const TexRomBank *bank,DWORD id)
{
    DWORD i,offset=bank->images;
    for(i=0;i<id;i++) { offset+=Read32(rom->data+bank->table+i*8)&0xffffffu; }
    return offset;
}
static void AssertBlank(const RomFile *rom,const TexRomBank *bank,DWORD id)
{
    DWORD offset=ImageOffset(rom,bank,id);int w,h;
    assert(TexDecodeRecord(rom->data+offset,rom->size-offset,decoded,&w,&h));
    assert(w==1&&h==1&&!decoded[0].a&&!decoded[0].r&&!decoded[0].g&&!decoded[0].b);
    assert(rom->data[bank->table+id*8]==0&&Read32(rom->data+bank->table+id*8+4)==0);
}
/* One normalized-UV triangle with an authored 4x4 image. The replacement
 * is 8x8; preview texel coordinates must remain 4, rather than becoming 8. */
static void ModelReplacementUvs(const char *project)
{
    char path[MAX_PATH];FILE *file;const char *why="";DWORD count;
    BgVertex *vertices;unsigned short *tags;BgRenderFlags *flags;
    float data[]={0,0,0, 1,0,0, 0,1,0, 0,0, 1,0, 0,1};
    snprintf(path,sizeof(path),"%s\\triangle.bin",project);
    file=fopen(path,"wb");assert(file&&fwrite(data,1,sizeof(data),file)==sizeof(data)&&!fclose(file));
    snprintf(path,sizeof(path),"%s\\triangle.gltf",project);
    file=fopen(path,"wb");assert(file);
    fputs("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"uri\":\"triangle.bin\",\"byteLength\":60}],"
          "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},{\"buffer\":0,\"byteOffset\":36,\"byteLength\":24}],"
          "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
          "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"}],"
          "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"TEXCOORD_0\":1},"
          "\"extras\":{\"goldeneyeTextureTag\":3,\"goldeneyeTextureSize\":[4,4]}}]}]}",file);
    assert(!fclose(file));
    vertices=GltfLoadModel(path,project,&count,&tags,&flags,&why);
    if(!vertices) { fprintf(stderr,"glTF: %s\n",why); }
    assert(vertices&&count==1&&tags[0]==3);
    assert(vertices[1].s==4&&vertices[2].t==4);
    free(vertices);free(tags);free(flags);
}
static void ImageActions(const char *project)
{
    const char *why="";TexImportOptions o={0,2,12,6},other={5,1,4,9};
    RomFile rom;TexRomBank bank,after;DWORD id,next,originalbmp,savedbmp,savednative,originalrom,count,i;
    char basepath[MAX_PATH],bmp[MAX_PATH],native[MAX_PATH],path[MAX_PATH];int w,h;
    TexThumb *thumbs;unsigned char *thumbpixels;
    snprintf(basepath,sizeof(basepath),"%s\\base.z64",project);
    snprintf(bmp,sizeof(bmp),"%s\\images\\0003.bmp",project);
    snprintf(native,sizeof(native),"%s\\images\\native\\0003.gtex",project);
    originalbmp=FileHash(bmp);assert(RomLoad(basepath,&rom,&why));
    originalrom=TexDataHash(rom.data,rom.size);assert(TexRomReadBank(&rom,&bank,&why));
    for(i=0;i<64;i++) { source[i]=(TexPixel){240,32,96,255}; }
    /* Cancelling an edit or closing without save preserves the original BMP/ROM. */
    assert(ImageEditsReplace(project,3,source,8,8,&o,&why));
    assert(TexLoadProjectImage(project,3,decoded,&w,&h)&&w==8&&h==8&&decoded[0].r==240);
    assert(FileHash(bmp)==originalbmp&&ImageEditsHasUnsaved());
    assert(ImageEditsExportToRom(project,&rom,&why)&&TexDataHash(rom.data,rom.size)==originalrom);
    ImageEditsReset();assert(TexLoadProjectImage(project,3,decoded,&w,&h)&&w==4);
    assert(ImageEditsDelete(project,3,&why));
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);assert(count==BASE_COUNT-1);
    for(i=0;i<count;i++) { assert(strtoul(thumbs[i].label,NULL,16)!=3); }
    free(thumbs);free(thumbpixels);assert(FileHash(bmp)==originalbmp);
    ImageEditsReset();assert(TexLoadProjectImage(project,3,decoded,&w,&h)&&w==4);
    /* Replacement changes every setting at the same ID; no new ID is consumed. */
    assert(ImageEditsReplace(project,3,source,8,8,&o,&why));
    assert(ImageEditsNextId(project,&next,&why)&&next==BASE_COUNT);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    savedbmp=FileHash(bmp);savednative=FileHash(native);
    ModelReplacementUvs(project);
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);assert(count==BASE_COUNT);
    assert(thumbs[3].imagewidth==8&&thumbs[3].info.format==0&&thumbs[3].info.mipmaps==2);
    assert(thumbs[3].info.hitsound==12&&thumbs[3].info.hittexture==6&&!thumbs[3].info.generatedmipmaps);
    free(thumbs);free(thumbpixels);
    assert(ImageEditsExportToRom(project,&rom,&why)&&TexRomReadBank(&rom,&after,&why)&&after.count==BASE_COUNT);
    for(i=0;i<BASE_COUNT;i++) if(i!=3)
    {
        DWORD offset=ImageOffset(&rom,&after,i),old,size=Read32(rom.data+TABLE+i*8)&0xffffffu;
        /* In this fixture all originals have the same record size. */
        old=bank.images+i*(bank.imagebytes/BASE_COUNT);
        assert(!memcmp(rom.data+offset,rom.data+old,size));assert(Read32(rom.data+TABLE+i*8+4)==0xabcd0000);
    }
    assert(rom.data[TABLE+3*8]==0xc6&&Read32(rom.data+TABLE+3*8+4)==0);
    {DWORD offset=ImageOffset(&rom,&after,3);TexInfoRecord info;
     assert(TexInfoReadRecord(rom.data+offset,rom.size-offset,&info)&&info.info.format==0&&info.info.mipmaps==2);}
    RomFree(&rom);
    /* Late save failures restore BOTH prior BMP and native settings. */
    assert(ImageEditsReplace(project,3,source,4,4,&other,&why));
    for(i=1;i<=3;i++)
    {
        test_fail_move=i;assert(!ImageEditsSave(project,&why)&&ImageEditsHasUnsaved());
        assert(FileHash(bmp)==savedbmp&&FileHash(native)==savednative);
    }
    assert(RomLoad(basepath,&rom,&why));assert(ImageEditsExportToRom(project,&rom,&why));
    assert(TexRomReadBank(&rom,&after,&why));
    {DWORD offset=ImageOffset(&rom,&after,3);assert(TexDecodeRecord(rom.data+offset,rom.size-offset,decoded,&w,&h)&&w==8);}
    RomFree(&rom);ImageEditsReset();
    assert(ImageEditsDelete(project,3,&why));
    test_fail_move=2;assert(!ImageEditsSave(project,&why));
    assert(FileHash(bmp)==savedbmp&&FileHash(native)==savednative);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(GetFileAttributes(bmp)==INVALID_FILE_ATTRIBUTES);
    assert(TexLoadProjectImage(project,3,decoded,&w,&h)&&w==1&&h==1&&!decoded[0].a);
    assert(TexGetProjectImageSize(project,3,&w,&h)&&w==1&&h==1);
    assert(!ImageEditsCanEdit(project,3,&why));
    /* Mix pending imports/deletion/replacement with a saved base deletion.
     * Thumbnail compaction must preserve every surviving pixel block. */
    assert(ImageEditsImport(project,source,4,4,&other,&id,&why)&&id==BASE_COUNT);
    assert(ImageEditsReplace(project,id,source,8,8,&o,&why));
    assert(ImageEditsImport(project,source,4,4,&other,&next,&why)&&next==BASE_COUNT+1);
    assert(ImageEditsDelete(project,next,&why));
    assert(ImageEditsImport(project,source,4,4,&other,&next,&why)&&next==BASE_COUNT+2);
    assert(ImageEditsDelete(project,0,&why));
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);assert(count==BASE_COUNT);
    for(i=0;i<count;i++)
    {
        DWORD tid=strtoul(thumbs[i].label,NULL,16);const unsigned char *px=thumbpixels+thumbs[i].pixeloffset;
        assert(tid!=0&&tid!=3&&tid!=BASE_COUNT+1);
        assert(TexLoadProjectImage(project,tid,decoded,&w,&h));
        assert(px[0]==decoded[w*h-1].b&&px[1]==decoded[w*h-1].g&&px[2]==decoded[w*h-1].r);
    }
    free(thumbs);free(thumbpixels);
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(ImageEditsNextId(project,&next,&why)&&next==BASE_COUNT+3);
    snprintf(path,sizeof(path),"%s\\images\\0011.bmp",project);assert(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES);
    /* A saved imported image is replaceable and later deletable at its own ID. */
    assert(ImageEditsReplace(project,BASE_COUNT+2,source,8,8,&o,&why));
    assert(ImageEditsSave(project,&why));assert(ImageEditsDelete(project,BASE_COUNT+2,&why));
    assert(ImageEditsSave(project,&why));ImageEditsReset();
    assert(RomLoad(basepath,&rom,&why)&&TexDataHash(rom.data,rom.size)==originalrom);
    assert(ImageEditsExportToRom(project,&rom,&why)&&TexRomReadBank(&rom,&after,&why));
    assert(after.count==BASE_COUNT+3);AssertBlank(&rom,&after,0);AssertBlank(&rom,&after,3);
    AssertBlank(&rom,&after,BASE_COUNT+1);AssertBlank(&rom,&after,BASE_COUNT+2);
    {DWORD offset=ImageOffset(&rom,&after,BASE_COUNT);assert(TexDecodeRecord(rom.data+offset,rom.size-offset,decoded,&w,&h)&&w==8&&decoded[0].r==240);}
    RomFree(&rom);
    count=TexLoadProjectThumbnails(project,&thumbs,&thumbpixels,&why);assert(count==BASE_COUNT-1);
    free(thumbs);free(thumbpixels);
    /* Only current GTI2 native-image metadata is accepted. */
    snprintf(path,sizeof(path),"%s\\images\\native\\0010.gtex",project);
    {FILE *f=fopen(path,"rb+");assert(f);assert(fwrite("GTI1",1,4,f)==4);fclose(f);}
    assert(RomLoad(basepath,&rom,&why));assert(!ImageEditsExportToRom(project,&rom,&why));RomFree(&rom);
    puts("PASS: base/imported/pending replacement and deletion, unchanged IDs/records, all settings, discard, saved-only export, save rollback, thumbnail compaction, blank slots and rejection of unsupported metadata.");
}

void CheckBmpAlpha(const char *, const char *);
int main(int argc,char **argv)
{
    char actions[MAX_PATH];assert(argc==2 || argc==3);Encoders();Fixture(argv[1]);
    CheckBmpAlpha(argv[1],argc==3?argv[2]:NULL);Pipeline(argv[1]);Limits(argv[1]);
    snprintf(actions,sizeof(actions),"%s-actions",argv[1]);Fixture(actions);ImageActions(actions);return 0;
}
