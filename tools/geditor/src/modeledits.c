/* Project-wide pending model replacements. The .gmodel contains the compiled
 * native model plus source/content fingerprints. glTF is its editable export.
 * No texture files, texture tables, or ROM image data are pruned. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "modeledits.h"
#include "modelcompile.h"
#include "newprops.h"
#include "texload.h"

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
static void ResetLegacy(void)
{
    while(g_ModelEdits) { ModelEdit *next=g_ModelEdits->next;free(g_ModelEdits->data);free(g_ModelEdits);g_ModelEdits=next; }
    g_EditProject[0]=0;
}
void ModelEditsReset(void) { ResetLegacy();NewPropsReset(); }
static void SetProject(const char *project)
{
    if(strcmp(g_EditProject,project)) { ResetLegacy();lstrcpyn(g_EditProject,project,MAX_PATH); }
}
BOOL ModelEditsHasUnsaved(void)
{
    ModelEdit *edit;
    for(edit=g_ModelEdits;edit;edit=edit->next) { if(edit->dirty) { return TRUE; } }
    return NewPropsHasUnsaved();
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
    const unsigned char *custom;
    int result;
    *why="";*size=0;SetProject(project);
    if (!NewPropsOpen(project,why)) return NULL;
    custom=NewPropsData(project,name,size);if (custom) return custom;
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
    if (!NewPropsOpen(project,why)) return FALSE;
    edited=NewPropsData(project,name,&editedsize);
    if (edited)
    {
        *data=malloc(editedsize);
        if (!*data) { *why="Out of memory loading the new prop.";return FALSE; }
        memcpy(*data,edited,editedsize);*size=editedsize;*basehash=0;return TRUE;
    }
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
BOOL ModelEditsReadSource(const char *project, const char *name, ModelSource *source,
    DWORD *revision, const char **why)
{
    unsigned char *data = NULL;
    DWORD size, basehash;
    BOOL ok;
    ZeroMemory(source, sizeof(*source)); *revision = 0;
    if (!LoadSource(project, name, &data, &size, &basehash, why)) { return FALSE; }
    ok = ModelReadSource(data, size, source, why);
    if (ok) ok=ModelMaterialsEnsure(source,project,why);
    if (ok) { *revision = ModelDataHash(data, size); source->closestpreview = name[0] == 'C'; }
    else ModelFreeSource(source);
    free(data); return ok;
}
BOOL ModelEditsSetProperties(const char *project, const char *name, DWORD revision,
    const DWORD *faces, DWORD count, int culling, int surface, const char **why)
{
    unsigned char *data = NULL, *compiled = NULL;
    DWORD size, basehash, compiledsize;
    ModelSource source = {0}, check = {0};
    ModelEdit *edit;
    BOOL ok = FALSE;
    if (!LoadSource(project, name, &data, &size, &basehash, why)) { goto done; }
    if (ModelDataHash(data, size) != revision)
    { *why = "The model revision changed. Reload the model and select its faces again."; goto done; }
    if (!ModelReadSource(data, size, &source, why)
        || !ModelMaterialsEnsure(&source,project,why)
        || !ModelCompileProperties(data, size, &source, faces, count, culling, surface,
            &compiled, &compiledsize, why)
        || !ModelMaterialsAttach(&compiled,&compiledsize,&source.materials,why)
        || !ModelReadSource(compiled, compiledsize, &check, why)) { goto done; }
    if (source.count != check.count)
    { *why = "The property edit changed the model's face count."; goto done; }
    if (size == compiledsize && !memcmp(data, compiled, size)) { ok = TRUE; goto done; }
    if (NewPropsData(project,name,&basehash))
    {
        ok=NewPropsReplace(name,compiled,compiledsize,why);
        if (ok) compiled=NULL;
        goto done;
    }
    for (edit = g_ModelEdits; edit && strcmp(edit->name, name); edit = edit->next) {}
    if (!edit)
    {
        edit = calloc(1, sizeof(*edit));
        if (!edit) { *why = "Out of memory retaining model properties."; goto done; }
        lstrcpyn(edit->name, name, sizeof(edit->name)); edit->next = g_ModelEdits; g_ModelEdits = edit;
    }
    free(edit->data); edit->data = compiled; compiled = NULL;
    edit->size = compiledsize; edit->basehash = basehash; edit->dirty = TRUE;
    ok = TRUE; *why = "";
done:
    free(data); free(compiled); ModelFreeSource(&source); ModelFreeSource(&check); return ok;
}

/* Takes ownership on success, matching NewPropsReplace. */
static BOOL RetainPaint(const char *project, const char *name, DWORD basehash,
    unsigned char *data, DWORD size, const char **why)
{
    DWORD customsize;
    ModelEdit *edit;
    if (NewPropsData(project, name, &customsize))
    { return NewPropsReplace(name, data, size, why); }
    for (edit = g_ModelEdits; edit && strcmp(edit->name, name); edit = edit->next) {}
    if (!edit)
    {
        edit = calloc(1, sizeof(*edit));
        if (!edit) { *why = "Out of memory retaining the painted model."; return FALSE; }
        lstrcpyn(edit->name, name, sizeof(edit->name)); edit->next = g_ModelEdits; g_ModelEdits = edit;
    }
    free(edit->data); edit->data = data;
    edit->size = size; edit->basehash = basehash; edit->dirty = TRUE;
    *why = ""; return TRUE;
}

BOOL ModelEditsSetVertexColor(const char *project, const char *name, DWORD revision,
    DWORD corner, const unsigned char rgba[4], ModelVertexPaint *change, const char **why)
{
    unsigned char *data = NULL, *painted = NULL;
    DWORD size, basehash;
    ModelSource source = {0};
    ModelVertexPaint step = {0};
    BOOL ok = FALSE;
    if (change) { memset(change, 0, sizeof(*change)); }
    if (!LoadSource(project, name, &data, &size, &basehash, why)) { goto done; }
    if (ModelDataHash(data, size) != revision)
    { *why = "The model changed. Reload it before painting."; goto done; }
    if (!ModelReadSource(data, size, &source, why)
        || !ModelCompileVertexColor(data, size, &source, corner, rgba, &painted, why)) { goto done; }
    step.offset = source.vertexoffsets[corner];
    step.beforeRevision = revision; step.afterRevision = ModelDataHash(painted, size);
    memcpy(step.before, data + step.offset + 12, 4);
    memcpy(step.after, painted + step.offset + 12, 4);
    if (!memcmp(step.before, step.after, 4)) { ok = TRUE; goto done; }
    ok = RetainPaint(project, name, basehash, painted, size, why);
    if (ok) { painted = NULL; }
done:
    if (ok && change) { *change = step; }
    free(data); free(painted); ModelFreeSource(&source); return ok;
}

BOOL ModelEditsRestoreVertexColor(const char *project, const char *name,
    const ModelVertexPaint *change, BOOL redo, const char **why)
{
    unsigned char *data = NULL;
    DWORD size, basehash;
    BOOL ok = FALSE;
    if (!change || !LoadSource(project, name, &data, &size, &basehash, why)) { goto done; }
    if (ModelDataHash(data, size) != (redo ? change->beforeRevision : change->afterRevision)
        || change->offset > size || size-change->offset < 16)
    { *why = "The model changed since this paint edit. Its paint history can no longer be applied."; goto done; }
    memcpy(data+change->offset+12, redo ? change->after : change->before, 4);
    if (ModelDataHash(data, size) != (redo ? change->afterRevision : change->beforeRevision))
    { *why = "The model's paint history does not match its vertex data."; goto done; }
    ok = RetainPaint(project, name, basehash, data, size, why);
    if (ok) { data = NULL; }
done:
    free(data); return ok;
}
BOOL ModelEditsSetMaterial(const char *project,const char *name,DWORD revision,
    DWORD slot,DWORD texture,const char **why)
{
    unsigned char *data=NULL,*compiled=NULL;
    DWORD size,basehash,compiledsize,i;
    ModelSource source={0},check={0}; GltfModelImport imported={0}; ModelEdit *edit;
    BOOL ok=FALSE;
    int w=1,h=1;
    if (!LoadSource(project,name,&data,&size,&basehash,why)) goto done;
    if (ModelDataHash(data,size)!=revision)
    { *why="The model changed. Reload its material slots and try again."; goto done; }
    if (!ModelReadSource(data,size,&source,why) || !ModelMaterialsEnsure(&source,project,why)) goto done;
    if (slot>=source.materials.count || texture>BG_TEX_NONE)
    { *why="The material slot or image is no longer available."; goto done; }
    if (texture!=BG_TEX_NONE && !TexGetProjectImageSize(project,texture,&w,&h))
    { *why="The dragged image is no longer available in this project."; goto done; }
    imported.count=source.count;
    imported.vertices=malloc((size_t)(source.count?source.count:1)*3*sizeof(*imported.vertices));
    imported.tags=malloc((size_t)(source.count?source.count:1)*sizeof(*imported.tags));
    imported.sourcevertices=malloc((size_t)(source.count?source.count:1)*3*sizeof(*imported.sourcevertices));
    imported.rebind=calloc(source.count?source.count:1,1);
    if (!imported.vertices || !imported.tags || !imported.sourcevertices || !imported.rebind)
    { *why="Out of memory assigning a material image."; goto done; }
    source.materials.slots[slot].texture=texture;
    for (i=0;i<source.count;i++)
    {
        DWORD k, target=source.materials.slots[source.materials.faces[i].slot].texture;
        imported.tags[i]=(source.tags[i]&~BG_TEX_ID_MASK)|target;
        imported.rebind[i]=source.materials.faces[i].slot==slot;
        for (k=0;k<3;k++)
        {
            DWORD corner=i*3+k;
            imported.sourcevertices[corner]=corner;
            imported.vertices[corner]=source.vertices[corner];
            imported.vertices[corner].s=source.materials.faces[i].uv[k*2];
            imported.vertices[corner].t=source.materials.faces[i].uv[k*2+1];
            /* Reflection coordinates are generated by the RSP. Rebinding an
             * image must not edit the hidden native S/T bytes. */
            if (source.flags[i]&BG_RENDER_ENVIRONMENT)
            {
                int width=1,height=1;
                if (target!=BG_TEX_NONE && !TexGetProjectImageSize(project,target,&width,&height)) goto done;
                imported.vertices[corner].s=source.vertices[corner].s/width;
                imported.vertices[corner].t=source.vertices[corner].t/height;
            }
        }
    }
    if (!ModelCompileImport(data,size,&source,&imported,project,&compiled,&compiledsize,why)
        || !ModelMaterialsAttach(&compiled,&compiledsize,&source.materials,why)
        || !ModelReadSource(compiled,compiledsize,&check,why)) goto done;
    if (check.count!=source.count)
    { *why="The material assignment changed the model's geometry."; goto done; }
    if (size==compiledsize && !memcmp(data,compiled,size)) { ok=TRUE; goto done; }
    { DWORD customsize;
      if (NewPropsData(project,name,&customsize))
      { ok=NewPropsReplace(name,compiled,compiledsize,why); if (ok) compiled=NULL; goto done; } }
    for (edit=g_ModelEdits;edit && strcmp(edit->name,name);edit=edit->next) {}
    if (!edit)
    {
        edit=calloc(1,sizeof(*edit));
        if (!edit) { *why="Out of memory retaining the material assignment."; goto done; }
        lstrcpyn(edit->name,name,sizeof(edit->name));edit->next=g_ModelEdits;g_ModelEdits=edit;
    }
    free(edit->data);edit->data=compiled;compiled=NULL;edit->size=compiledsize;
    edit->basehash=basehash;edit->dirty=TRUE;ok=TRUE;*why="";
done:
    free(data);free(compiled);ModelFreeSource(&source);ModelFreeSource(&check);
    GltfFreeModelImport(&imported);return ok;
}

/* Source identities put surviving stock-model faces back in native draw
 * order. A cyclic Blender reindexing also reorders the retained UV corners. */
static BOOL ImportMaterials(const ModelSource *source,GltfModelImport *imported,
    ModelMaterials *ordered,const char **why)
{
    DWORD *lookup=malloc((size_t)(source->count?source->count:1)*sizeof(*lookup));
    DWORD i,cursor=0;
    if (!lookup) { *why="Out of memory retaining imported slots."; return FALSE; }
    if (!ModelMaterialsCopy(ordered,&imported->materials,why)) { free(lookup); return FALSE; }
    memset(lookup,0xff,(size_t)source->count*sizeof(*lookup));
    for (i=0;i<imported->count;i++) lookup[imported->sourcevertices[i*3]/3]=i;
    for (i=0;i<source->count;i++) if (lookup[i]!=0xffffffffu)
    {
        DWORD j=lookup[i],k;
        ordered->faces[cursor].slot=imported->materials.faces[j].slot;
        for (k=0;k<3;k++)
        {
            DWORD corner=imported->sourcevertices[j*3+k]%3;
            ordered->faces[cursor].uv[corner*2]=imported->vertices[j*3+k].s;
            ordered->faces[cursor].uv[corner*2+1]=imported->vertices[j*3+k].t;
        }
        cursor++;
    }
    free(lookup);return TRUE;
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
        DWORD customsize;
        if (NewPropsData(project,name,&customsize))
        {
            DWORD i;
            for (i=0;i<source.count*3;i++)
            { source.vertices[i].x*=.001f;source.vertices[i].y*=.001f;source.vertices[i].z*=.001f; }
        }
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
    GltfModelImport imported={0}; ModelMaterials ordered={0};
    ModelEdit *edit; DWORD i;
    BOOL ok=FALSE;
    if (!NewPropsOpen(project,why)) return FALSE;
    if (NewPropsData(project,name,&size))
    {
        if (!ModelEditsReadSource(project,name,&source,&basehash,why)) return FALSE;
        *before=source.count;ModelFreeSource(&source);
        return NewPropsImport(project,name,path,TRUE,after,why);
    }
    if(!LoadSource(project,name,&data,&size,&basehash,why)) { goto done; }
    if(!ModelReadSource(data,size,&source,why) || !ModelMaterialsEnsure(&source,project,why)
        || !GltfReadModelImport(path,ModelDataHash(data,size),&imported,why)) goto done;
    ModelMaterialsMatch(&imported.materials,&source.materials);
    for (i=0;i<imported.count;i++)
        imported.tags[i]=(imported.tags[i]&~BG_TEX_ID_MASK)|imported.materials.slots[imported.materials.faces[i].slot].texture;
    int topology = ModelImportKeepsTopology(&source,&imported);
    if (topology < 0) { *why="Out of memory matching imported faces."; goto done; }
    if (topology)
    {
        if (!ModelCompileImport(data,size,&source,&imported,project,&compiled,&compiledsize,why)
            || !ImportMaterials(&source,&imported,&ordered,why)) goto done;
    }
    else if (!ModelCompileRetopology(data,size,&source,&imported,project,&ordered,&compiled,&compiledsize,why)) goto done;
    if (compiledsize==ModelMaterialsNativeSize(data,size) && !memcmp(data,compiled,compiledsize)
        && ordered.count==source.materials.count && ordered.facecount==source.materials.facecount
        && !memcmp(ordered.slots,source.materials.slots,(size_t)ordered.count*sizeof(*ordered.slots))
        && !memcmp(ordered.faces,source.materials.faces,(size_t)ordered.facecount*sizeof(*ordered.faces)))
    { *before=*after=source.count;ok=TRUE;*why="";goto done; }
    if (!ModelMaterialsAttach(&compiled,&compiledsize,&ordered,why)
        || !ModelReadSource(compiled,compiledsize,&check,why)) goto done;
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
    free(data);free(compiled);ModelFreeSource(&source);ModelFreeSource(&check);
    ModelMaterialsFree(&ordered);GltfFreeModelImport(&imported);return ok;
}
BOOL ModelEditsSave(const char *project, const char **why)
{
    ModelEdit *edit;
    char dir[MAX_PATH];
    int n;
    *why="";
    if (!NewPropsSave(project,why)) return FALSE;
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
    /* Editor slot names and unassigned UVs do not occupy N64 RAM. */
    *data=edit.data;*size=ModelMaterialsNativeSize(edit.data,edit.size);return 1;
}
