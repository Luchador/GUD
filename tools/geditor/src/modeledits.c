/* Project-wide pending model replacements. The .gmodel contains the compiled
 * native model plus source/content fingerprints. glTF is its editable export.
 * No texture files, texture tables, or ROM image data are pruned. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "modeledits.h"
#include "modelcompile.h"

typedef struct ModelEdit {
    char name[64];
    unsigned char *data;
    DWORD size, basehash;
    BOOL dirty;
    struct ModelEdit *next;
} ModelEdit;
static ModelEdit *g_ModelEdits;
static char g_EditProject[MAX_PATH];
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static void Write32(unsigned char *p,DWORD v) { p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v; }
static const char *Folder(const char *name)
{
    size_t i, size=strlen(name);
    if(size<2 || size>=64 || name[size-1]!='Z') { return NULL; }
    for(i=0;i<size;i++) { if(!isalnum((unsigned char)name[i]) && name[i]!='_') { return NULL; } }
    if(!strcmp(name,"GcartblueZ") || !strcmp(name,"GcartridgeZ") || !strcmp(name,"GcartrifleZ") || !strcmp(name,"GcartshellZ")) { return "casings"; }
    return name[0]=='C' ? "characters" : name[0]=='P' ? "objects" : name[0]=='G' ? "guns" : NULL;
}
static BOOL Path(char *path, const char *project, const char *name, const char *suffix)
{
    int n;
    if(Folder(name)==NULL) { return FALSE; }
    n=snprintf(path,MAX_PATH,"%s\\models\\native\\%s%s",project,name,suffix);
    return n>=0 && n<MAX_PATH;
}
void ModelEditsReset(void)
{
    while(g_ModelEdits) { ModelEdit *next=g_ModelEdits->next;free(g_ModelEdits->data);free(g_ModelEdits);g_ModelEdits=next; }
    g_EditProject[0]=0;
}
static void SetProject(const char *project)
{
    if(strcmp(g_EditProject,project)) { ModelEditsReset();lstrcpyn(g_EditProject,project,MAX_PATH); }
}
BOOL ModelEditsHasUnsaved(void)
{
    ModelEdit *edit;
    for(edit=g_ModelEdits;edit;edit=edit->next) { if(edit->dirty) { return TRUE; } }
    return FALSE;
}
static int ReadEdit(const char *project, const char *name, ModelEdit *edit, const char **why)
{
    char path[MAX_PATH];
    unsigned char header[16];
    FILE *file;
    long length;
    *why="";
    if(!Path(path,project,name,".gmodel")) { return 0; }
    if(GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES)
    {
        DWORD error=GetLastError();
        if(error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) { return 0; }
        *why="A saved model replacement could not be accessed.";return -1;
    }
    *why="A saved model replacement is damaged or unreadable.";
    file=fopen(path,"rb");if(!file) { return -1; }
    if(fseek(file,0,SEEK_END)!=0 || (length=ftell(file))<16 || length>0x01000000 || fseek(file,0,SEEK_SET)!=0
        || fread(header,1,16,file)!=16 || memcmp(header,"GMD1",4) || Read32(header+8)!=(DWORD)length-16) { fclose(file);return -1; }
    edit->size=Read32(header+8);edit->basehash=Read32(header+4);
    edit->data=malloc(edit->size);
    if(!edit->data || fread(edit->data,1,edit->size,file)!=edit->size || ModelDataHash(edit->data,edit->size)!=Read32(header+12))
    { fclose(file);free(edit->data);edit->data=NULL;return -1; }
    fclose(file);lstrcpyn(edit->name,name,sizeof(edit->name));*why="";return 1;
}
const unsigned char *ModelEditsGetData(const char *project, const char *name, DWORD *size, const char **why)
{
    ModelEdit *edit;
    int result;
    *why="";*size=0;SetProject(project);
    for(edit=g_ModelEdits;edit;edit=edit->next)
    { if(!strcmp(edit->name,name)) { *size=edit->size;return edit->data; } }
    edit=calloc(1,sizeof(*edit));if(!edit) { *why="Out of memory loading the model.";return NULL; }
    result=ReadEdit(project,name,edit,why);
    if(result!=1) { free(edit);return NULL; }
    edit->next=g_ModelEdits;g_ModelEdits=edit;*size=edit->size;return edit->data;
}
static BOOL LoadSource(const char *project, const char *name, unsigned char **data, DWORD *size, DWORD *basehash, const char **why)
{
    RomFile rom;
    char path[MAX_PATH];
    DWORD offset, span, editedsize;
    const unsigned char *edited;
    int n=snprintf(path,sizeof(path),"%s\\base.z64",project);
    *data=NULL;*size=0;
    if(n<0 || n>=MAX_PATH || !Folder(name)) { *why="The model path is invalid.";return FALSE; }
    if(!RomLoad(path,&rom,why)) { return FALSE; }
    if(!RomFindFile(&rom,name,&offset,&span,why)) { RomFree(&rom);return FALSE; }
    *basehash=ModelDataHash(rom.data+offset,span);
    edited=ModelEditsGetData(project,name,&editedsize,why);
    if(!edited && (*why)[0]) { RomFree(&rom);return FALSE; }
    if(edited)
    {
        ModelEdit *entry;
        for(entry=g_ModelEdits;entry && strcmp(entry->name,name);entry=entry->next) {}
        if(!entry || entry->basehash!=*basehash)
        { *why="The saved model belongs to a different base ROM.";RomFree(&rom);return FALSE; }
    }
    *size=edited ? editedsize : span;*data=malloc(*size);
    if(*data) { memcpy(*data,edited ? edited : rom.data+offset,*size); }
    RomFree(&rom);
    if(!*data) { *why="Out of memory loading the source model.";return FALSE; }
    return TRUE;
}
BOOL ModelEditsExport(const char *project, const char *name, const char *path, const char **why)
{
    unsigned char *data;
    DWORD size,basehash;
    ModelSource source;
    BOOL ok;
    if(!LoadSource(project,name,&data,&size,&basehash,why)) { return FALSE; }
    ok=ModelReadSource(data,size,&source,why);
    if(ok)
    {
        source.closestpreview=name[0]=='C';
        ok=GltfWriteEditableModel(path,project,&source,ModelDataHash(data,size),why);
        ModelFreeSource(&source);
    }
    free(data);return ok;
}
BOOL ModelEditsImport(const char *project, const char *name, const char *path,
    DWORD *before, DWORD *after, const char **why)
{
    unsigned char *data=NULL,*compiled=NULL;
    DWORD size,basehash,compiledsize;
    ModelSource source={0},check={0};
    GltfModelImport imported={0};
    ModelEdit *edit;
    BOOL ok=FALSE;
    if(!LoadSource(project,name,&data,&size,&basehash,why)) { goto done; }
    if(!ModelReadSource(data,size,&source,why)
        || !GltfReadModelImport(path,ModelDataHash(data,size),&imported,why)
        || !ModelCompileImport(data,size,&source,&imported,project,&compiled,&compiledsize,why)
        || !ModelReadSource(compiled,compiledsize,&check,why)) { goto done; }
    if(check.count!=imported.count) { *why="The compiled model did not reproduce the imported face count.";goto done; }
    *before=source.count;*after=check.count;
    if(size==compiledsize && !memcmp(data,compiled,size)) { ok=TRUE;goto done; }
    for(edit=g_ModelEdits;edit && strcmp(edit->name,name);edit=edit->next) {}
    if(!edit)
    {
        edit=calloc(1,sizeof(*edit));if(!edit) { *why="Out of memory retaining the imported model.";goto done; }
        lstrcpyn(edit->name,name,sizeof(edit->name));edit->next=g_ModelEdits;g_ModelEdits=edit;
    }
    free(edit->data);edit->data=compiled;compiled=NULL;edit->size=compiledsize;edit->basehash=basehash;edit->dirty=TRUE;
    ok=TRUE;*why="";
done:
    free(data);free(compiled);ModelFreeSource(&source);ModelFreeSource(&check);GltfFreeModelImport(&imported);return ok;
}
BOOL ModelEditsSave(const char *project, const char **why)
{
    ModelEdit *edit;
    char dir[MAX_PATH];
    int n;
    *why="";
    if(!ModelEditsHasUnsaved()) { return TRUE; }
    n=snprintf(dir,sizeof(dir),"%s\\models\\native",project);
    if(n<0 || n>=MAX_PATH || (!CreateDirectory(dir,NULL) && GetLastError()!=ERROR_ALREADY_EXISTS))
    { *why="The native model folder could not be created.";return FALSE; }
    for(edit=g_ModelEdits;edit;edit=edit->next)
    {
        char path[MAX_PATH],temp[MAX_PATH],gltf[MAX_PATH],gltftemp[MAX_PATH];
        unsigned char header[16];
        FILE *file;
        ModelSource source;
        BOOL written;
        if(!edit->dirty) { continue; }
        if(!Path(path,project,edit->name,".gmodel") || !Path(temp,project,edit->name,".gmodel.tmp"))
        { *why="The model save path is too long.";return FALSE; }
        n=snprintf(gltf,sizeof(gltf),"%s\\models\\%s\\%s.gltf",project,Folder(edit->name),edit->name);
        if(n<0 || n+4>=MAX_PATH) { *why="The model asset path is too long.";return FALSE; }
        memcpy(gltftemp,gltf,(size_t)n);memcpy(gltftemp+n,".tmp",5);
        if(!ModelReadSource(edit->data,edit->size,&source,why)) { return FALSE; }
        source.closestpreview=edit->name[0]=='C';
        written=GltfWriteEditableModel(gltftemp,project,&source,ModelDataHash(edit->data,edit->size),why);
        ModelFreeSource(&source);if(!written) { DeleteFile(gltftemp);return FALSE; }
        memcpy(header,"GMD1",4);Write32(header+4,edit->basehash);Write32(header+8,edit->size);Write32(header+12,ModelDataHash(edit->data,edit->size));
        file=fopen(temp,"wb");
        written=file && fwrite(header,1,16,file)==16 && fwrite(edit->data,1,edit->size,file)==edit->size;
        if(file && fclose(file)!=0) { written=FALSE; }
        if(!written || !MoveFileEx(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)
            || !MoveFileEx(gltftemp,gltf,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        { DeleteFile(temp);DeleteFile(gltftemp);*why="An imported model could not be saved. Its pending changes are still available; retry Save Project.";return FALSE; }
        edit->dirty=FALSE;
    }
    return TRUE;
}
int ModelEditsReadReplacement(const char *project, const char *name,
    const unsigned char *base, DWORD basesize, unsigned char **data, DWORD *size, const char **why)
{
    ModelEdit edit={0};
    int result=ReadEdit(project,name,&edit,why);
    *data=NULL;*size=0;
    if(result!=1) { return result; }
    if(edit.basehash!=ModelDataHash(base,basesize))
    { free(edit.data);*why="A model replacement belongs to a different base ROM.";return -1; }
    {
        ModelSource source;
        if(!ModelReadSource(edit.data,edit.size,&source,why)) { free(edit.data);return -1; }
        ModelFreeSource(&source);
    }
    *data=edit.data;*size=edit.size;return 1;
}
