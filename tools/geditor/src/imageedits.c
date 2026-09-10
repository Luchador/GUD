/* Image imports/replacements keep native GUTX + settings beside their BMP.
 * Changes stay in memory until Save Project. Deletion keeps a blank native
 * record at the same ID, so hard-coded and authored references never shift.
 * Unused images are retained unless the user explicitly deletes them. */
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
    BOOL deleted, restorebmp;
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
/* Overrides below the base count are allowed. Above it, even deleted slots
 * retain metadata, preserving every later image's identity. */
static BOOL Contiguous(const unsigned char *ids,const TexRomBank *bank,DWORD *total,const char **why)
{
    DWORD i,end=bank->count;
    for(i=bank->count;i<TEX_IMAGE_CAPACITY;i++) if(ids[i]) { end=i+1; }
    if(end>bank->capacity)
    { *why="This project has reached the game's 4096-image limit.";return FALSE; }
    for(i=bank->count;i<end;i++) if(!ids[i])
    { *why="Imported image IDs are missing. Restore the missing native image assets; deleted images must retain their deletion records.";return FALSE; }
    *total=end;return TRUE;
}
static BOOL ReadSaved(const char *project,DWORD id,const TexRomBank *bank,ImageEdit *edit,const char **why)
{
    char path[MAX_PATH];unsigned char header[32];FILE *file;long length;TexInfoRecord info;int level,w,h;
    *why="An imported image's native asset is damaged, unreadable, or belongs to another base ROM.";
    if(!Path(path,project,id,".gtex") || !(file=fopen(path,"rb"))) { return FALSE; }
    if(fseek(file,0,SEEK_END) || (length=ftell(file))<144 || length>8192 || fseek(file,0,SEEK_SET)
        || fread(header,1,32,file)!=32 || memcmp(header,"GTI2",4)
        || Read32(header+12)!=id || Read32(header+16)!=(DWORD)length-32
        || header[28]>12 || header[29]>12 || header[30]>1 || header[31]
        || (bank && (Read32(header+4)!=bank->hash || Read32(header+8)!=bank->count)))
    { fclose(file);return FALSE; }
    edit->size=Read32(header+16);edit->data=malloc(edit->size);
    if(!edit->data || fread(edit->data,1,edit->size,file)!=edit->size)
    { fclose(file);free(edit->data);edit->data=NULL;return FALSE; }
    fclose(file);
    if(TexDataHash(edit->data,edit->size)!=Read32(header+20)
        || !TexInfoReadRecord(edit->data,edit->size,&info) || info.size!=edit->size
        || edit->data[4]!=1 || edit->data[5]<1 || edit->data[5]>7) { goto invalid; }
    edit->deleted=header[30]!=0;
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
    if(edit->deleted)
    {
        TexPixel pixel;int width,height;
        if(edit->width!=1 || edit->height!=1 || edit->options.format!=1 || edit->options.mipmaps
            || edit->options.hitsound || edit->options.hittexture
            || !TexDecodeRecord(edit->data,edit->size,&pixel,&width,&height)
            || pixel.r || pixel.g || pixel.b || pixel.a) { goto invalid; }
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
    if(!Contiguous(ids,&bank,id,why)) { return FALSE; }
    if(*id>=bank.capacity) { *why="This project has reached the game's 4096-image limit.";return FALSE; }
    if(!Path(path,project,*id,".bmp")) { *why="The image path is too long.";return FALSE; }
    if(GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES)
    { *why="The next image filename already exists without import metadata. Move that BMP out of the project's images folder before importing it.";return FALSE; }
    return TRUE;
}
static ImageEdit *Pending(const char *project,DWORD id)
{
    ImageEdit *edit;
    if(strcmp(project,g_ImageProject)) { return NULL; }
    for(edit=g_ImageEdits;edit;edit=edit->next) if(edit->id==id) { return edit; }
    return NULL;
}
BOOL ImageEditsCanEdit(const char *project,DWORD id,const char **why)
{
    unsigned char ids[TEX_IMAGE_CAPACITY];DWORD count,total;TexRomBank bank;ImageEdit *edit;
    if(!LoadBank(project,&bank,why) || !SavedIds(project,ids,&count,why)) { return FALSE; }
    if(!strcmp(project,g_ImageProject)) for(edit=g_ImageEdits;edit;edit=edit->next) { ids[edit->id]=1; }
    if(!Contiguous(ids,&bank,&total,why)) { return FALSE; }
    *why="That image is unavailable or has already been deleted.";
    if(id>=total) { return FALSE; }
    edit=Pending(project,id);
    if(edit) { if(edit->deleted) { return FALSE; } }
    else if(ids[id])
    {
        ImageEdit saved={0};BOOL deleted;
        if(!ReadSaved(project,id,&bank,&saved,why)) { return FALSE; }
        deleted=saved.deleted;FreeEdit(&saved);
        if(deleted) { *why="That image has already been deleted.";return FALSE; }
    }
    *why="";return TRUE;
}
static BOOL Stage(const char *project,DWORD id,const TexPixel *pixels,int width,int height,
    const TexImportOptions *options,BOOL deleted,const char **why)
{
    ImageEdit *edit,**slot;TexRomBank bank;
    if(!LoadBank(project,&bank,why)) { return FALSE; }
    edit=calloc(1,sizeof(*edit));if(!edit) { *why="Out of memory editing the image.";return FALSE; }
    if(!TexEncodeRecord(pixels,width,height,options,&edit->data,&edit->size,why)) { free(edit);return FALSE; }
    edit->pixels=malloc((size_t)width*height*sizeof(TexPixel));
    if(!edit->pixels || !TexDecodeRecord(edit->data,edit->size,edit->pixels,&edit->width,&edit->height))
    { FreeEdit(edit);free(edit);*why="The converted image could not be previewed.";return FALSE; }
    if(strcmp(project,g_ImageProject)) { ImageEditsReset();lstrcpyn(g_ImageProject,project,MAX_PATH); }
    edit->id=id;edit->options=*options;edit->basehash=bank.hash;edit->basecount=bank.count;edit->deleted=deleted;
    edit->pixelhash=TexDataHash((unsigned char *)edit->pixels,width*height*sizeof(TexPixel));
    /* Keep ascending IDs so a partial disk-save failure leaves a valid prefix.
     * Replace a previous pending edit only after the new conversion succeeds. */
    slot=&g_ImageEdits;while(*slot && (*slot)->id<id) { slot=&(*slot)->next; }
    if(*slot && (*slot)->id==id)
    {
        ImageEdit *old=*slot;edit->restorebmp=old->restorebmp;
        edit->next=old->next;FreeEdit(old);free(old);
    }
    else { edit->next=*slot; }
    *slot=edit;*why="";return TRUE;
}
BOOL ImageEditsImport(const char *project,const TexPixel *pixels,int width,int height,
    const TexImportOptions *options,DWORD *id,const char **why)
{
    return ImageEditsNextId(project,id,why) && Stage(project,*id,pixels,width,height,options,FALSE,why);
}
BOOL ImageEditsReplace(const char *project,DWORD id,const TexPixel *pixels,int width,int height,
    const TexImportOptions *options,const char **why)
{
    return ImageEditsCanEdit(project,id,why) && Stage(project,id,pixels,width,height,options,FALSE,why);
}
BOOL ImageEditsDelete(const char *project,DWORD id,const char **why)
{
    const TexPixel blank={0,0,0,0};const TexImportOptions options={1,0,0,0};
    return ImageEditsCanEdit(project,id,why) && Stage(project,id,&blank,1,1,&options,TRUE,why);
}
/* Saved deletions have no BMP; image consumers can still resolve the blank
 * slot without accidentally falling back to the base ROM's original pixels. */
BOOL ImageEditsGetDeletedPixels(const char *project,DWORD id,TexPixel *out,int *width,int *height)
{
    ImageEdit edit={0};const char *why="";BOOL deleted;
    if(!ReadSaved(project,id,NULL,&edit,&why)) { return FALSE; }
    deleted=edit.deleted;FreeEdit(&edit);
    if(!deleted) { return FALSE; }
    *width=*height=1;if(out) { *out=(TexPixel){0,0,0,0}; }return TRUE;
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
        char bmp[MAX_PATH],bmptemp[MAX_PATH],native[MAX_PATH],temp[MAX_PATH],backup[MAX_PATH];
        FILE *file;BOOL ok,bmpwritten=FALSE,backedup=FALSE;
        if(!Path(bmp,project,edit->id,".bmp") || !Path(bmptemp,project,edit->id,".bmp.tmp")
            || !Path(native,project,edit->id,".gtex") || !Path(temp,project,edit->id,".gtex.tmp")
            || !Path(backup,project,edit->id,".bmp.rollback"))
        { *why="The image save path is too long.";return FALSE; }
        if(edit->restorebmp)
        {
            if(!MoveFileEx(backup,bmp,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            { *why="Close programs using this image, then retry Save Project. Its previous BMP is preserved in the .bmp.rollback file.";return FALSE; }
            edit->restorebmp=FALSE;
        }
        memcpy(header,"GTI2",4);Write32(header+4,edit->basehash);Write32(header+8,edit->basecount);
        Write32(header+12,edit->id);Write32(header+16,edit->size);Write32(header+20,TexDataHash(edit->data,edit->size));
        Write32(header+24,edit->pixelhash);header[28]=edit->options.hitsound;header[29]=edit->options.hittexture;
        header[30]=edit->deleted ? 1 : 0;
        file=fopen(temp,"wb");ok=file && fwrite(header,1,32,file)==32 && fwrite(edit->data,1,edit->size,file)==edit->size;
        if(file && fclose(file)) { ok=FALSE; }
        if(ok && !edit->deleted) { ok=TexWriteBmp(bmptemp,edit->pixels,edit->width,edit->height); }
        if(ok && GetFileAttributes(bmp)!=INVALID_FILE_ATTRIBUTES)
        { ok=backedup=MoveFileEx(bmp,backup,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
        if(ok && !edit->deleted) { ok=bmpwritten=MoveFileEx(bmptemp,bmp,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
        /* Commit the native record last. On a failed replacement/deletion,
         * restore the old BMP too, rather than destroying the saved version. */
        if(ok) { ok=MoveFileEx(temp,native,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH); }
        if(!ok)
        {
            DeleteFile(temp);DeleteFile(bmptemp);
            if(bmpwritten) { DeleteFile(bmp); }
            if(backedup && !MoveFileEx(backup,bmp,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
            {
                edit->restorebmp=TRUE;
                *why="The image save failed. Close programs using the BMP and retry Save Project; the previous BMP is preserved in its .bmp.rollback file.";
                return FALSE;
            }
            *why="An image could not be saved. The previous saved version and pending edits are retained; retry Save Project.";return FALSE;
        }
        if(backedup) { DeleteFile(backup); }
        g_ImageEdits=edit->next;FreeEdit(edit);free(edit);
    }
    return TRUE;
}
BOOL ImageEditsExportToRom(const char *project,RomFile *rom,const char **why)
{
    unsigned char ids[TEX_IMAGE_CAPACITY],*surfaces=NULL;DWORD count,total,i,*sizes=NULL;
    unsigned char **records=NULL;TexRomBank bank;BOOL ok=FALSE;
    if(!SavedIds(project,ids,&count,why)) { return FALSE; }
    if(!TexRomReadBank(rom,&bank,why) || !Contiguous(ids,&bank,&total,why)) { return FALSE; }
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
    records=calloc(total,sizeof(*records));sizes=calloc(total,sizeof(*sizes));surfaces=calloc(total,1);
    if(!records || !sizes || !surfaces) { *why="Out of memory loading imported images.";goto done; }
    for(i=0;i<total;i++) if(ids[i])
    {
        ImageEdit edit={0};TexPixel *pixels;int width,height;
        if(!ReadSaved(project,i,&bank,&edit,why)) { goto done; }
        records[i]=edit.data;sizes[i]=edit.size;surfaces[i]=(edit.options.hitsound<<4)|edit.options.hittexture;
        if(edit.deleted) { continue; }
        pixels=malloc(256*256*sizeof(TexPixel));
        if(!pixels) { *why="Out of memory checking the imported BMP.";goto done; }
        if(!TexLoadSavedProjectImage(project,edit.id,pixels,&width,&height))
        { free(pixels);*why="An imported image's BMP is missing or unreadable. Restore it before building the ROM.";goto done; }
        if(width!=edit.width || height!=edit.height || TexDataHash((unsigned char *)pixels,width*height*sizeof(TexPixel))!=edit.pixelhash)
        {
            unsigned char *encoded;DWORD size;
            if(!TexEncodeRecord(pixels,width,height,&edit.options,&encoded,&size,why)) { free(pixels);goto done; }
            free(records[i]);records[i]=encoded;sizes[i]=size;
        }
        free(pixels);
    }
    ok=TexRomUpdateImages(rom,&bank,(const unsigned char *const *)records,sizes,surfaces,total,why);
done:
    if(records) { for(i=0;i<total;i++) { free(records[i]); } }
    free(records);free(sizes);free(surfaces);return ok;
}

static void RemoveThumbnail(TexThumb *items,DWORD *count,DWORD id)
{
    DWORD i;
    for(i=0;i<*count;i++) if(strtoul(items[i].label,NULL,16)==id)
    { memmove(items+i,items+i+1,(--*count-i)*sizeof(*items));return; }
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
        if(stored.deleted) { RemoveThumbnail(*items,count,i); }
        else for(j=0;j<*count;j++) if(strtoul((*items)[j].label,NULL,16)==i) { SetInfo(&(*items)[j],&stored);break; }
        FreeEdit(&stored);
    }
    /* Compact into a fresh block: the item list may be sorted, so copying
     * in place could overwrite a later item's source pixels. */
    if(*count)
    {
        unsigned char *compact=malloc((size_t)*count*TEX_THUMB_MAX*TEX_THUMB_MAX*4);
        if(!compact) { return; }
        for(j=0;j<*count;j++)
        {
            unsigned int offset=j*TEX_THUMB_MAX*TEX_THUMB_MAX*4;
            memcpy(compact+offset,*pixels+(*items)[j].pixeloffset,TEX_THUMB_MAX*TEX_THUMB_MAX*4);
            (*items)[j].pixeloffset=offset;
        }
        free(*pixels);*pixels=compact;
    }
    if(strcmp(project,g_ImageProject)) { return; }
    /* Remove pending deletions after append/update so their pixel offsets
     * cannot be reused by new thumbnails in this pass. */
    for(edit=g_ImageEdits;edit;edit=edit->next)
    {
        TexThumb *thumb;int x,y,longest=edit->width>edit->height ? edit->width : edit->height;
        if(edit->deleted) { continue; }
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
    for(edit=g_ImageEdits;edit;edit=edit->next) if(edit->deleted) { RemoveThumbnail(*items,count,edit->id); }
}
