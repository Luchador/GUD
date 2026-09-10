/* Imported images keep a native GUTX + surface settings beside their BMP.
 * New imports stay in memory until Save Project, just like model imports.
 * Saved images are never pruned because they become unused. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imageedits.h"
#include "texrom.h"

typedef struct ImageEdit {
    DWORD id, size, basehash, basecount, pixelhash;
    unsigned char *data;
    TexPixel *pixels;
    int width, height;
    TexImportOptions options;
    struct ImageEdit *next;
} ImageEdit;
static ImageEdit *g_ImageEdits;
static char g_ImageProject[MAX_PATH];
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Write32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static BOOL Path(char *path,const char *project,DWORD id,const char *suffix)
{
    int n=snprintf(path,MAX_PATH,"%s\\images\\%s%04lX%s",project,
                   strstr(suffix,"gtex") ? "native\\" : "",(unsigned long)id,suffix);
    return n>=0 && n<MAX_PATH;
}
static void FreeEdit(ImageEdit *edit) { free(edit->data);free(edit->pixels); }
void ImageEditsReset(void)
{
    while(g_ImageEdits) { ImageEdit *next=g_ImageEdits->next;FreeEdit(g_ImageEdits);free(g_ImageEdits);g_ImageEdits=next; }
    g_ImageProject[0]=0;
}
BOOL ImageEditsHasUnsaved(void) { return g_ImageEdits!=NULL; }

/* Enumerate metadata, including malformed/gapped sets, so exports cannot
 * silently drop a texture and shift every ID following it. */
static BOOL SavedIds(const char *project,unsigned char ids[TEX_IMAGE_CAPACITY],DWORD *count,const char **why)
{
    char pattern[MAX_PATH];WIN32_FIND_DATA find;HANDLE search;DWORD error;
    int n=snprintf(pattern,sizeof(pattern),"%s\\images\\native\\*.gtex",project);
    memset(ids,0,TEX_IMAGE_CAPACITY);*count=0;*why="";
    if(n<0 || n>=MAX_PATH) { *why="The image asset path is too long.";return FALSE; }
    search=FindFirstFile(pattern,&find);
    if(search==INVALID_HANDLE_VALUE)
    {
        error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) { return TRUE; }
        *why="The imported image folder could not be read.";return FALSE;
    }
    do
    {
        char *end;unsigned long id;
        if(find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) { continue; }
        id=strtoul(find.cFileName,&end,16);
        if(end-find.cFileName!=4 || lstrcmpi(end,".gtex") || id>=TEX_IMAGE_CAPACITY || ids[id])
        { FindClose(search);*why="An imported image has an invalid or duplicate filename.";return FALSE; }
        ids[id]=1;(*count)++;
    } while(FindNextFile(search,&find));
    error=GetLastError();FindClose(search);
    if(error!=ERROR_NO_MORE_FILES) { *why="The imported image folder could not be completely read.";return FALSE; }
    return TRUE;
}
static BOOL LoadBank(const char *project,TexRomBank *bank,const char **why)
{
    char path[MAX_PATH];RomFile rom;BOOL ok;int n=snprintf(path,sizeof(path),"%s\\base.z64",project);
    if(n<0 || n>=MAX_PATH) { *why="The project path is too long.";return FALSE; }
    if(!RomLoad(path,&rom,why)) { return FALSE; }
    ok=TexRomReadBank(&rom,bank,why);RomFree(&rom);return ok;
}
static BOOL Contiguous(const unsigned char *ids,DWORD count,const TexRomBank *bank,const char **why)
{
    DWORD i;
    if(count>bank->capacity-bank->count)
    { *why="This project has reached the game's 4096-image limit.";return FALSE; }
    for(i=0;i<TEX_IMAGE_CAPACITY;i++)
    {
        if((ids[i]!=0)!=(i>=bank->count && i<bank->count+count))
        { *why="Imported image IDs are missing, overlap the base ROM, or belong to a different project. Restore the missing image assets.";return FALSE; }
    }
    return TRUE;
}
static BOOL ReadSaved(const char *project,DWORD id,const TexRomBank *bank,ImageEdit *edit,const char **why)
{
    char path[MAX_PATH];unsigned char header[32];FILE *file;long length;TexInfoRecord info;int level,w,h;
    *why="An imported image's native asset is damaged, unreadable, or belongs to another base ROM.";
    if(!Path(path,project,id,".gtex") || !(file=fopen(path,"rb"))) { return FALSE; }
    if(fseek(file,0,SEEK_END) || (length=ftell(file))<144 || length>8192 || fseek(file,0,SEEK_SET)
        || fread(header,1,32,file)!=32 || memcmp(header,"GTI1",4)
        || Read32(header+12)!=id || Read32(header+16)!=(DWORD)length-32
        || header[28]>12 || header[29]>12 || header[30] || header[31]
        || (bank && (Read32(header+4)!=bank->hash || Read32(header+8)!=bank->count)))
    { fclose(file);return FALSE; }
    edit->size=Read32(header+16);edit->data=malloc(edit->size);
    if(!edit->data || fread(edit->data,1,edit->size,file)!=edit->size)
    { fclose(file);free(edit->data);edit->data=NULL;return FALSE; }
    fclose(file);
    if(TexDataHash(edit->data,edit->size)!=Read32(header+20)
        || !TexInfoReadRecord(edit->data,edit->size,&info) || info.size!=edit->size
        || edit->data[4]!=1 || edit->data[5]<1 || edit->data[5]>7) { goto invalid; }
    edit->id=id;edit->width=w=edit->data[17];edit->height=h=edit->data[18];
    edit->options=(TexImportOptions){info.info.format,info.info.mipmaps,header[28],header[29]};
    edit->basehash=Read32(header+4);edit->basecount=Read32(header+8);edit->pixelhash=Read32(header+24);
    if(!TexImportTmemBytes(w,h,edit->options.format,edit->options.mipmaps)
        || TexImportTmemBytes(w,h,edit->options.format,edit->options.mipmaps)>TexImportTmemLimit(edit->options.format)) { goto invalid; }
    for(level=0;level<=edit->options.mipmaps;level++)
    {
        const unsigned char *desc=edit->data+16+level*12;
        if(desc[0]!=edit->options.format || desc[1]!=w || desc[2]!=h) { goto invalid; }
        w=(w+1)/2;h=(h+1)/2;
    }
    *why="";return TRUE;
invalid:
    free(edit->data);edit->data=NULL;return FALSE;
}
BOOL ImageEditsNextId(const char *project,DWORD *id,const char **why)
{
    unsigned char ids[TEX_IMAGE_CAPACITY];DWORD count;TexRomBank bank;ImageEdit *edit;char path[MAX_PATH];
    if(!LoadBank(project,&bank,why) || !SavedIds(project,ids,&count,why)) { return FALSE; }
    if(!strcmp(project,g_ImageProject)) for(edit=g_ImageEdits;edit;edit=edit->next)
    { if(!ids[edit->id]) { ids[edit->id]=1;count++; } }
    if(!Contiguous(ids,count,&bank,why)) { return FALSE; }
    *id=bank.count+count;
    if(*id>=bank.capacity) { *why="This project has reached the game's 4096-image limit.";return FALSE; }
    if(!Path(path,project,*id,".bmp")) { *why="The image path is too long.";return FALSE; }
    if(GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES)
    { *why="The next image filename already exists without import metadata. Move that BMP out of the project's images folder before importing it.";return FALSE; }
    return TRUE;
}
BOOL ImageEditsImport(const char *project,const TexPixel *pixels,int width,int height,
    const TexImportOptions *options,DWORD *id,const char **why)
{
    ImageEdit *edit;TexRomBank bank;
    if(!ImageEditsNextId(project,id,why) || !LoadBank(project,&bank,why)) { return FALSE; }
    edit=calloc(1,sizeof(*edit));if(!edit) { *why="Out of memory importing the image.";return FALSE; }
    if(!TexEncodeRecord(pixels,width,height,options,&edit->data,&edit->size,why)) { free(edit);return FALSE; }
    edit->pixels=malloc((size_t)width*height*sizeof(TexPixel));
    if(!edit->pixels || !TexDecodeRecord(edit->data,edit->size,edit->pixels,&edit->width,&edit->height))
    { FreeEdit(edit);free(edit);*why="The converted image could not be previewed.";return FALSE; }
    if(strcmp(project,g_ImageProject)) { ImageEditsReset();lstrcpyn(g_ImageProject,project,MAX_PATH); }
    edit->id=*id;edit->options=*options;edit->basehash=bank.hash;edit->basecount=bank.count;
    edit->pixelhash=TexDataHash((unsigned char *)edit->pixels,width*height*sizeof(TexPixel));
    /* Keep import order so a partial disk-save failure leaves a valid prefix. */
    { ImageEdit **tail=&g_ImageEdits;while(*tail) { tail=&(*tail)->next; } *tail=edit; }
    return TRUE;
}
BOOL ImageEditsGetPixels(const char *project,DWORD id,TexPixel *out,int *width,int *height)
{
    ImageEdit *edit;
    if(strcmp(project,g_ImageProject)) { return FALSE; }
    for(edit=g_ImageEdits;edit;edit=edit->next) if(edit->id==id)
    {
        *width=edit->width;*height=edit->height;
        if(out) { memcpy(out,edit->pixels,(size_t)edit->width*edit->height*sizeof(TexPixel)); }
        return TRUE;
    }
    return FALSE;
}
BOOL ImageEditsSave(const char *project,const char **why)
{
    char dir[MAX_PATH];int n;
    *why="";if(!g_ImageEdits) { return TRUE; }
    if(strcmp(project,g_ImageProject)) { *why="The pending images belong to another project.";return FALSE; }
    n=snprintf(dir,sizeof(dir),"%s\\images",project);
    if(n<0 || n>=MAX_PATH || (!CreateDirectory(dir,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS))
    { *why="The images folder could not be created.";return FALSE; }
    n=snprintf(dir,sizeof(dir),"%s\\images\\native",project);
    if(n<0 || n>=MAX_PATH || (!CreateDirectory(dir,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS))
    { *why="The native image folder could not be created.";return FALSE; }
    while(g_ImageEdits)
    {
        ImageEdit *edit=g_ImageEdits;unsigned char header[32]={0};
        char bmp[MAX_PATH],bmptemp[MAX_PATH],native[MAX_PATH],temp[MAX_PATH];FILE *file;BOOL ok,bmpwritten=FALSE;
        if(!Path(bmp,project,edit->id,".bmp") || !Path(bmptemp,project,edit->id,".bmp.tmp")
            || !Path(native,project,edit->id,".gtex") || !Path(temp,project,edit->id,".gtex.tmp"))
        { *why="The image save path is too long.";return FALSE; }
        memcpy(header,"GTI1",4);Write32(header+4,edit->basehash);Write32(header+8,edit->basecount);
        Write32(header+12,edit->id);Write32(header+16,edit->size);Write32(header+20,TexDataHash(edit->data,edit->size));
        Write32(header+24,edit->pixelhash);header[28]=edit->options.hitsound;header[29]=edit->options.hittexture;
        file=fopen(temp,"wb");ok=file && fwrite(header,1,32,file)==32 && fwrite(edit->data,1,edit->size,file)==edit->size;
        if(file && fclose(file)) { ok=FALSE; }
        if(ok) { ok=TexWriteBmp(bmptemp,edit->pixels,edit->width,edit->height); }
        if(ok) { ok=bmpwritten=MoveFileEx(bmptemp,bmp,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
        if(ok) { ok=MoveFileEx(temp,native,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
        if(!ok)
        {
            DeleteFile(temp);DeleteFile(bmptemp);
            if(bmpwritten) { DeleteFile(bmp); }
            *why="An imported image could not be saved. Its pending data is still available; retry Save Project.";return FALSE;
        }
        g_ImageEdits=edit->next;FreeEdit(edit);free(edit);
    }
    return TRUE;
}
BOOL ImageEditsExportToRom(const char *project,RomFile *rom,const char **why)
{
    unsigned char ids[TEX_IMAGE_CAPACITY],*surfaces=NULL;DWORD count,i,*sizes=NULL;
    unsigned char **records=NULL;TexRomBank bank;BOOL ok=FALSE;
    if(!SavedIds(project,ids,&count,why)) { return FALSE; }
    if(count==0)
    {
        /* Old projects remain buildable. New ROMs can diagnose an imported
         * BMP whose metadata was removed, even when the whole native folder
         * is gone, instead of quietly losing that image from the output. */
        for(i=0;i<rom->info.entrycount;i++) { if(rom->info.entries[i].kind==0x54584346u) { break; } }
        if(i==rom->info.entrycount) { return TRUE; }
    }
    if(!TexRomReadBank(rom,&bank,why) || !Contiguous(ids,count,&bank,why)) { return FALSE; }
    {
        char pattern[MAX_PATH];WIN32_FIND_DATA find;HANDLE search;DWORD error;
        int n=snprintf(pattern,sizeof(pattern),"%s\\images\\*.bmp",project);
        if(n<0 || n>=MAX_PATH) { *why="The image folder path is too long.";return FALSE; }
        search=FindFirstFile(pattern,&find);
        if(search!=INVALID_HANDLE_VALUE)
        {
            do
            {
                char *end;unsigned long id=strtoul(find.cFileName,&end,16);
                if(!(find.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) && end-find.cFileName==4
                    && !lstrcmpi(end,".bmp") && id>=bank.count
                    && (id>=TEX_IMAGE_CAPACITY || !ids[id]))
                { FindClose(search);*why="A new image BMP has no import metadata. Restore its images/native .gtex file or import the BMP through File > Import > Import Image.";return FALSE; }
            } while(FindNextFile(search,&find));
            error=GetLastError();FindClose(search);
            if(error!=ERROR_NO_MORE_FILES) { *why="The image folder could not be completely read.";return FALSE; }
        }
    }
    if(count==0) { *why="";return TRUE; }
    records=calloc(count,sizeof(*records));sizes=calloc(count,sizeof(*sizes));surfaces=malloc(count);
    if(!records || !sizes || !surfaces) { *why="Out of memory loading imported images.";goto done; }
    for(i=0;i<count;i++)
    {
        ImageEdit edit={0};TexPixel *pixels;int width,height;
        if(!ReadSaved(project,bank.count+i,&bank,&edit,why)) { goto done; }
        records[i]=edit.data;sizes[i]=edit.size;surfaces[i]=(edit.options.hitsound<<4)|edit.options.hittexture;
        pixels=malloc(256*256*sizeof(TexPixel));
        if(!pixels) { *why="Out of memory checking the imported BMP.";goto done; }
        if(!TexLoadProjectImage(project,edit.id,pixels,&width,&height))
        { free(pixels);*why="An imported image's BMP is missing or unreadable. Restore it before building the ROM.";goto done; }
        if(width!=edit.width || height!=edit.height || TexDataHash((unsigned char *)pixels,width*height*sizeof(TexPixel))!=edit.pixelhash)
        {
            unsigned char *encoded;DWORD size;
            if(!TexEncodeRecord(pixels,width,height,&edit.options,&encoded,&size,why)) { free(pixels);goto done; }
            free(records[i]);records[i]=encoded;sizes[i]=size;
        }
        free(pixels);
    }
    ok=TexRomAppendImages(rom,&bank,(const unsigned char *const *)records,sizes,surfaces,count,why);
done:
    if(records) { for(i=0;i<count;i++) { free(records[i]); } }
    free(records);free(sizes);free(surfaces);return ok;
}

static void SetInfo(TexThumb *thumb,const ImageEdit *edit)
{
    thumb->info=(TexImageInfo){TRUE,TRUE,edit->options.format,edit->options.mipmaps,FALSE,
                             edit->options.hitsound,edit->options.hittexture};
}
void ImageEditsUpdateThumbnails(const char *project,TexThumb **items,unsigned char **pixels,DWORD *count)
{
    unsigned char ids[TEX_IMAGE_CAPACITY];DWORD saved,i,j;const char *why="";ImageEdit *edit;
    if(SavedIds(project,ids,&saved,&why)) for(i=0;i<TEX_IMAGE_CAPACITY;i++) if(ids[i])
    {
        ImageEdit stored={0};
        if(!ReadSaved(project,i,NULL,&stored,&why)) { continue; }
        for(j=0;j<*count;j++) if(strtoul((*items)[j].label,NULL,16)==i) { SetInfo(&(*items)[j],&stored);break; }
        FreeEdit(&stored);
    }
    if(strcmp(project,g_ImageProject)) { return; }
    for(edit=g_ImageEdits;edit;edit=edit->next)
    {
        TexThumb *thumb;int x,y,longest=edit->width>edit->height ? edit->width : edit->height;
        for(j=0;j<*count;j++) if(strtoul((*items)[j].label,NULL,16)==edit->id) { break; }
        if(j==*count)
        {
            TexThumb *list=realloc(*items,((size_t)*count+1)*sizeof(**items));unsigned char *block;
            if(!list) { return; } *items=list;
            block=realloc(*pixels,((size_t)*count+1)*TEX_THUMB_MAX*TEX_THUMB_MAX*4);
            if(!block) { return; } *pixels=block;
            ZeroMemory(&list[j],sizeof(list[j]));list[j].pixeloffset=j*TEX_THUMB_MAX*TEX_THUMB_MAX*4;(*count)++;
        }
        thumb=&(*items)[j];snprintf(thumb->label,sizeof(thumb->label),"%04lX",(unsigned long)edit->id);
        thumb->imagewidth=edit->width;thumb->imageheight=edit->height;SetInfo(thumb,edit);
        thumb->w=longest>TEX_THUMB_MAX ? edit->width*TEX_THUMB_MAX/longest : edit->width;
        thumb->h=longest>TEX_THUMB_MAX ? edit->height*TEX_THUMB_MAX/longest : edit->height;
        if(thumb->w<1) { thumb->w=1; } if(thumb->h<1) { thumb->h=1; }
        ZeroMemory(*pixels+thumb->pixeloffset,TEX_THUMB_MAX*TEX_THUMB_MAX*4);
        for(y=0;y<thumb->h;y++) for(x=0;x<thumb->w;x++)
        {
            TexPixel p=edit->pixels[(edit->height-1-y*edit->height/thumb->h)*edit->width+edit->width-1-x*edit->width/thumb->w];
            unsigned char *dst=*pixels+thumb->pixeloffset+(y*TEX_THUMB_MAX+x)*4;
            dst[0]=p.b;dst[1]=p.g;dst[2]=p.r;dst[3]=p.a;
        }
    }
}
