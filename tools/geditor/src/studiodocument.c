/* Persistent studio instances, materials and lights. All writes replace a
 * completed sibling temporary file; failed loads leave the open scene intact. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "studioscene.h"
#include "gltfjson.h"
#include "editorpath.h"

#define STUDIO_MAX_OBJECTS 1024
#define STUDIO_MAX_SCENE_BYTES (16u * 1024u * 1024u)

const StudioGlobalLight g_StudioDefaultAmbientLight={{1,1,1},.2,{0,0,0}};
/* Direction follows the light's travel, matching spotlight direction semantics. */
const StudioGlobalLight g_StudioDefaultDirectionalLight={{1,1,1},1,{-0.348742916,-0.813733471,-0.464990554}};

void StudioSceneDefaultLighting(StudioScene *scene)
{
    scene->ambient=g_StudioDefaultAmbientLight; scene->directional=g_StudioDefaultDirectionalLight;
}

BOOL StudioGlobalLightValid(const StudioGlobalLight *light,BOOL directional)
{
    double length=0;
    if (!isfinite(light->intensity) || light->intensity<0 || light->intensity>10000) { return FALSE; }
    for (int k=0;k<3;k++)
    {
        if (!isfinite(light->color[k]) || light->color[k]<0 || light->color[k]>1) { return FALSE; }
        if (directional)
        {
            if (!isfinite(light->direction[k]) || fabs(light->direction[k])>1e9) { return FALSE; }
            length+=light->direction[k]*light->direction[k];
        }
    }
    return !directional || length>1e-24;
}

BOOL StudioAssetFilename(const char *name, const char *extension)
{
    size_t length = name ? strlen(name) : 0, suffix = strlen(extension);
    if (length <= suffix || length >= MAX_PATH || lstrcmpi(name + length - suffix, extension)) { return FALSE; }
    if (name[length-1] == ' ' || name[0] == ' ') { return FALSE; }
    for (size_t i=0;i<length;i++)
        if ((unsigned char)name[i]<32 || strchr("\\/:*?\"<>|",name[i])) { return FALSE; }
    return TRUE;
}

BOOL StudioTransformValid(const StudioTransform *t)
{
    for (int k=0;k<3;k++)
        if (!isfinite(t->position[k]) || fabs(t->position[k])>1e9
            || !isfinite(t->rotation[k]) || fabs(t->rotation[k])>1e9
            || !isfinite(t->scale[k]) || t->scale[k]<0.0001 || t->scale[k]>10000) { return FALSE; }
    return TRUE;
}

BOOL StudioMaterialValid(const StudioMaterial *m)
{
    if (!m->name[0] || (m->image[0] && !StudioAssetFilename(m->image,".bmp"))) { return FALSE; }
    for (int i=0;i<3;i++)
        if (!isfinite(m->base[i]) || m->base[i]<0 || m->base[i]>1
            || !isfinite(m->emission[i]) || m->emission[i]<0 || m->emission[i]>1
            || !isfinite(m->specular[i]) || m->specular[i]<0 || m->specular[i]>1) { return FALSE; }
    return isfinite(m->metalness) && m->metalness>=0 && m->metalness<=1
        && isfinite(m->environmentblur) && m->environmentblur>=0 && m->environmentblur<=1
        && isfinite(m->intensity) && m->intensity>=0 && m->intensity<=1
        && isfinite(m->shininess) && m->shininess>=1 && m->shininess<=128;
}

BOOL StudioLightValid(const StudioLight *light, BOOL spotlight)
{
    double length=0;
    for (int k=0;k<3;k++)
    {
        if (!isfinite(light->position[k]) || fabs(light->position[k])>1e9
            || !isfinite(light->color[k]) || light->color[k]<0 || light->color[k]>1) { return FALSE; }
        if (spotlight)
        {
            if (!isfinite(light->direction[k]) || fabs(light->direction[k])>1e9) { return FALSE; }
            length+=light->direction[k]*light->direction[k];
        }
    }
    if (!isfinite(light->intensity) || light->intensity<0 || light->intensity>10000) { return FALSE; }
    if (!spotlight) { return isfinite(light->radius) && light->radius>=0.0001 && light->radius<=1e9; }
    return length>1e-24 && isfinite(light->inner) && isfinite(light->outer)
        && light->inner>=0 && light->inner<=light->outer && light->outer>0 && light->outer<=90;
}

int StudioSceneLightSlot(const StudioScene *scene, BOOL spotlight)
{
    for (int i=spotlight ? 0 : 1;i<(spotlight ? 1 : STUDIO_LIGHT_COUNT);i++)
        if (!scene->lights[i].enabled) { return i; }
    return -1;
}

int StudioSceneAddLight(StudioScene *scene, BOOL spotlight, const char **why)
{
    int slot=StudioSceneLightSlot(scene,spotlight); *why="";
    if (!scene->filename[0]) { *why="Create or open a scene before adding a light."; return -1; }
    if (slot<0) { *why=spotlight ? "A scene supports one spotlight." : "A scene supports two point lights."; return -1; }
    StudioLight light={0}; light.enabled=TRUE; light.position[1]=5; light.intensity=1;
    for (int k=0;k<3;k++) { light.color[k]=1; }
    if (spotlight) { light.direction[1]=-1; light.inner=20; light.outer=30; }
    else { light.radius=10; }
    scene->lights[slot]=light; return slot;
}

void StudioSceneFree(StudioScene *scene)
{
    for (DWORD i=0;i<scene->count;i++) { free(scene->objects[i].materials); }
    free(scene->objects);
    while (scene->assets)
    {
        StudioModel *asset=scene->assets; scene->assets=asset->next;
        GltfFreeModelImport(&asset->mesh); free(asset->basecolors); free(asset);
    }
    memset(scene,0,sizeof(*scene));
}

static StudioModel *StudioLoadModel(StudioScene *scene, const char *filename, const char **why)
{
    char folder[MAX_PATH], path[MAX_PATH]; StudioModel *asset;
    for (asset=scene->assets;asset;asset=asset->next)
        if (!lstrcmpi(asset->filename,filename)) { return asset; }
    if (!StudioAssetFilename(filename,".gltf")
        || !EditorPathJoin(folder,sizeof(folder),scene->project,"studio\\models")
        || !EditorPathJoin(path,sizeof(path),folder,filename))
    { *why="The studio model filename or path is invalid."; return NULL; }
    asset=calloc(1,sizeof(*asset));
    if (!asset) { *why="Out of memory loading the studio model."; return NULL; }
    if (!GltfReadStudioModel(path,&asset->mesh,&asset->basecolors,why) || !asset->mesh.count)
    {
        GltfFreeModelImport(&asset->mesh); free(asset->basecolors); free(asset);
        if (!(*why)[0]) { *why="The model contains no triangle meshes."; }
        return NULL;
    }
    for (DWORD i=0;i<asset->mesh.count*3;i++)
    {
        const BgVertex *v=&asset->mesh.vertices[i]; double p[3]={v->x,v->y,v->z};
        for (int k=0;k<3;k++)
        {
            if (!i || p[k]<asset->lower[k]) { asset->lower[k]=p[k]; }
            if (!i || p[k]>asset->upper[k]) { asset->upper[k]=p[k]; }
        }
    }
    lstrcpyn(asset->filename,filename,sizeof(asset->filename));
    asset->next=scene->assets; scene->assets=asset;
    return asset;
}

static BOOL StudioDefaultMaterials(StudioInstance *object, StudioModel *asset)
{
    object->materialcount=asset->mesh.materials.count;
    object->materials=calloc(object->materialcount ? object->materialcount : 1,sizeof(*object->materials));
    if (!object->materials) { return FALSE; }
    for (DWORD i=0;i<object->materialcount;i++)
    {
        StudioMaterial *m=&object->materials[i];
        lstrcpyn(m->name,asset->mesh.materials.slots[i].name,sizeof(m->name));
        for (int k=0;k<3;k++) { m->base[k]=asset->basecolors[i][k]; m->specular[k]=1; }
        m->intensity=0.25f; m->shininess=32;
    }
    return TRUE;
}

BOOL StudioSceneAddModel(StudioScene *scene, const char *filename, const double position[3], const char **why)
{
    StudioInstance item={0}, *grown;
    *why="";
    if (!scene->filename[0] || scene->count>=STUDIO_MAX_OBJECTS)
    { *why="Select a scene with fewer than 1024 instances before adding a model."; return FALSE; }
    for (int k=0;k<3;k++) if (!isfinite(position[k]) || fabs(position[k])>1e9)
    { *why="The model position is outside the studio's supported range."; return FALSE; }
    for (int k=0;k<3;k++) { item.transform.scale[k]=1; }
    item.asset=StudioLoadModel(scene,filename,why);
    if (!item.asset) { return FALSE; }
    if (!StudioDefaultMaterials(&item,item.asset)) { *why="Out of memory creating instance materials."; return FALSE; }
    grown=realloc(scene->objects,(scene->count+1)*sizeof(*grown));
    if (!grown) { free(item.materials); *why="Out of memory creating a model instance."; return FALSE; }
    scene->objects=grown; lstrcpyn(item.model,filename,sizeof(item.model));
    memcpy(item.transform.position,position,sizeof(item.transform.position)); scene->objects[scene->count++]=item;
    return TRUE;
}

void StudioSceneRemove(StudioScene *scene, DWORD index)
{
    if (index>=scene->count) { return; }
    free(scene->objects[index].materials); scene->count--;
    memmove(scene->objects+index,scene->objects+index+1,(scene->count-index)*sizeof(*scene->objects));
}

static BOOL StudioScenePath(const StudioScene *scene, char folder[MAX_PATH], char path[MAX_PATH])
{
    return scene->project[0] && StudioAssetFilename(scene->filename,".rnd")
        && EditorPathJoin(folder,MAX_PATH,scene->project,"studio\\scenes")
        && EditorPathJoin(path,MAX_PATH,folder,scene->filename);
}

BOOL StudioSceneSave(const StudioScene *scene, const char **why)
{
    char folder[MAX_PATH], path[MAX_PATH], temporary[MAX_PATH]; FILE *file; BOOL ok;
    *why="";
    if (!StudioScenePath(scene,folder,path) || scene->count>STUDIO_MAX_OBJECTS)
    { *why="The studio scene path or instance count is invalid."; return FALSE; }
    if (scene->environment[0] && !StudioAssetFilename(scene->environment,".bmp")) { goto invalid; }
    for (DWORD i=0;i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i];
        if (!StudioAssetFilename(o->model,".gltf") || o->materialcount>4096) { goto invalid; }
        if (!StudioTransformValid(&o->transform)) { goto invalid; }
        for (DWORD m=0;m<o->materialcount;m++) if (!StudioMaterialValid(&o->materials[m])) { goto invalid; }
    }
    for (int i=0;i<STUDIO_LIGHT_COUNT;i++)
        if (scene->lights[i].enabled && !StudioLightValid(&scene->lights[i],i==0)) { goto invalid; }
    if (!StudioGlobalLightValid(&scene->ambient,FALSE) || !StudioGlobalLightValid(&scene->directional,TRUE)) { goto invalid; }
    if (!GetTempFileName(folder,"rnd",0,temporary))
    { *why="Could not create a temporary scene file."; return FALSE; }
    file=fopen(temporary,"wb");
    if (!file) { DeleteFile(temporary); *why="Could not open the temporary scene file."; return FALSE; }
    ok=fputs("{\n  \"format\": \"GEditor Render Studio\",\n  \"version\": 7,\n  \"environment\": ",file)!=EOF
        && GltfJsonWriteString(file,scene->environment) && fputs(",\n  \"objects\": [",file)!=EOF;
    for (DWORD i=0;ok && i<scene->count;i++)
    {
        const StudioInstance *o=&scene->objects[i];
        ok=fprintf(file,"%s\n    {\"model\": ",i ? "," : "")>=0 && GltfJsonWriteString(file,o->model)
            && fprintf(file,", \"position\": [%.17g, %.17g, %.17g], \"rotation\": [%.17g, %.17g, %.17g], \"scale\": [%.17g, %.17g, %.17g], \"materials\": [",
                o->transform.position[0],o->transform.position[1],o->transform.position[2],
                o->transform.rotation[0],o->transform.rotation[1],o->transform.rotation[2],
                o->transform.scale[0],o->transform.scale[1],o->transform.scale[2])>=0;
        for (DWORD j=0;ok && j<o->materialcount;j++)
        {
            const StudioMaterial *m=&o->materials[j];
            ok=fprintf(file,"%s\n      {\"name\": ",j ? "," : "")>=0 && GltfJsonWriteString(file,m->name)
                && fputs(", \"image\": ",file)!=EOF && GltfJsonWriteString(file,m->image)
                && fprintf(file,", \"base\": [%.9g, %.9g, %.9g], \"specular\": [%.9g, %.9g, %.9g], \"intensity\": %.9g, \"shininess\": %.9g, \"emission\": [%.9g, %.9g, %.9g], \"metalness\": %.9g, \"environmentBlur\": %.9g}",
                    m->base[0],m->base[1],m->base[2],m->specular[0],m->specular[1],m->specular[2],m->intensity,m->shininess,
                    m->emission[0],m->emission[1],m->emission[2],m->metalness,m->environmentblur)>=0;
        }
        ok=ok && fputs("\n    ]}",file)!=EOF;
    }
    ok=ok && fputs("\n  ],\n  \"lights\": [",file)!=EOF;
    BOOL comma=FALSE;
    for (int i=0;ok && i<STUDIO_LIGHT_COUNT;i++)
    {
        const StudioLight *light=&scene->lights[i]; if (!light->enabled) { continue; }
        ok=fprintf(file,"%s\n    {\"type\": \"%s\", \"slot\": %d, \"position\": [%.17g, %.17g, %.17g], \"color\": [%.9g, %.9g, %.9g], \"intensity\": %.17g",
            comma ? "," : "",i==0 ? "spotlight" : "point",i,light->position[0],light->position[1],light->position[2],
            light->color[0],light->color[1],light->color[2],light->intensity)>=0;
        if (i==0)
            ok=ok && fprintf(file,", \"direction\": [%.17g, %.17g, %.17g], \"inner\": %.17g, \"outer\": %.17g}",
                light->direction[0],light->direction[1],light->direction[2],light->inner,light->outer)>=0;
        else { ok=ok && fprintf(file,", \"radius\": %.17g}",light->radius)>=0; }
        comma=TRUE;
    }
    const StudioGlobalLight *ambient=&scene->ambient,*directional=&scene->directional;
    ok=ok && fprintf(file,"\n  ],\n  \"ambient\": {\"color\": [%.9g, %.9g, %.9g], \"intensity\": %.17g},"
        "\n  \"directional\": {\"color\": [%.9g, %.9g, %.9g], \"intensity\": %.17g, \"direction\": [%.17g, %.17g, %.17g]}\n}\n",
        ambient->color[0],ambient->color[1],ambient->color[2],ambient->intensity,
        directional->color[0],directional->color[1],directional->color[2],directional->intensity,
        directional->direction[0],directional->direction[1],directional->direction[2])>=0 && !ferror(file);
    if (fclose(file)) { ok=FALSE; }
    if (!ok || !MoveFileEx(temporary,path,MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    { DeleteFile(temporary); *why="The scene could not be saved. Check free space and folder permissions. The previous file was preserved."; return FALSE; }
    return TRUE;
invalid:
    *why="The scene contains invalid instance, material, or light settings."; return FALSE;
}

typedef struct StudioJson { const char *text; GltfJsonToken *tokens; int count; } StudioJson;
static int Field(const StudioJson *j,int object,const char *name)
{ return GltfJsonObjectGet(j->text,j->tokens,j->count,object,name); }
static BOOL String(const StudioJson *j,int token,char *out,size_t capacity)
{
    char *text=token>=0 ? GltfJsonCopyString(j->text,&j->tokens[token]) : NULL;
    if (!text) { return FALSE; }
    size_t length=strlen(text); BOOL ok=length<capacity;
    if (ok) { memcpy(out,text,length+1); } free(text); return ok;
}
static BOOL Number(const StudioJson *j,int token,double *value)
{
    char *end;
    if (token<0 || j->tokens[token].type!=GLTF_JSON_PRIMITIVE) { return FALSE; }
    *value=strtod(j->text+j->tokens[token].start,&end);
    return end==j->text+j->tokens[token].end && end!=j->text+j->tokens[token].start && isfinite(*value);
}
static BOOL Vector(const StudioJson *j,int token,double *value,DWORD size)
{
    if (token<0 || j->tokens[token].type!=GLTF_JSON_ARRAY || GltfJsonArrayCount(j->tokens,j->count,token)!=size) { return FALSE; }
    for (DWORD i=0;i<size;i++) if (!Number(j,GltfJsonArrayGet(j->tokens,j->count,token,i),value+i)) { return FALSE; }
    return TRUE;
}

static BOOL StudioReadGlobalLight(const StudioJson *j,int object,BOOL directional,StudioGlobalLight *light)
{
    double color[3];
    if (object<0 || j->tokens[object].type!=GLTF_JSON_OBJECT
        || !Vector(j,Field(j,object,"color"),color,3)
        || !Number(j,Field(j,object,"intensity"),&light->intensity)) { return FALSE; }
    for (int k=0;k<3;k++)
    { if (color[k]<0 || color[k]>1) { return FALSE; } light->color[k]=(float)color[k]; }
    if (directional && !Vector(j,Field(j,object,"direction"),light->direction,3)) { return FALSE; }
    return StudioGlobalLightValid(light,directional);
}

BOOL StudioSceneLoad(const char *projectdir, const char *filename, StudioScene *scene, const char **why)
{
    StudioScene next={0}; StudioJson j={0}; char folder[MAX_PATH], path[MAX_PATH], *text=NULL, format[64];
    FILE *file=NULL; long length; DWORD version; int array, token; BOOL ok=FALSE, missing=FALSE;
    *why="The studio scene is invalid or uses an unsupported format.";
    if (!projectdir || strlen(projectdir)>=MAX_PATH || !filename || strlen(filename)>=MAX_PATH) { goto done; }
    lstrcpyn(next.project,projectdir,sizeof(next.project)); lstrcpyn(next.filename,filename,sizeof(next.filename));
    if (!StudioScenePath(&next,folder,path) || !(file=fopen(path,"rb"))) { goto done; }
    if (fseek(file,0,SEEK_END) || (length=ftell(file))<0 || (unsigned long)length>STUDIO_MAX_SCENE_BYTES || fseek(file,0,SEEK_SET)) { goto done; }
    text=calloc((size_t)length+1,1);
    if (!text || fread(text,1,length,file)!=(size_t)length) { goto done; }
    j.text=text;
    if (!GltfJsonParse(text,length,&j.tokens,&j.count,why) || !j.count || j.tokens[0].type!=GLTF_JSON_OBJECT) { goto done; }
    *why="The studio scene is invalid or uses an unsupported format.";
    if (!String(&j,Field(&j,0,"format"),format,sizeof(format)) || strcmp(format,"GEditor Render Studio")) { goto done; }
    token=Field(&j,0,"version");
    if (token<0 || !GltfJsonUnsigned(text,&j.tokens[token],&version) || (version<1 || version>7)) { goto done; }
    if (version>=6 && (!String(&j,Field(&j,0,"environment"),next.environment,sizeof(next.environment))
        || (next.environment[0] && !StudioAssetFilename(next.environment,".bmp")))) { goto done; }
    array=Field(&j,0,"objects");
    if (array<0 || j.tokens[array].type!=GLTF_JSON_ARRAY) { goto done; }
    next.count=GltfJsonArrayCount(j.tokens,j.count,array);
    if (next.count>STUDIO_MAX_OBJECTS) { next.count=0; goto done; }
    next.objects=calloc(next.count ? next.count : 1,sizeof(*next.objects));
    if (!next.objects) { next.count=0; goto done; }
    for (DWORD i=0;i<next.count;i++)
    {
        StudioInstance *o=&next.objects[i]; int object=GltfJsonArrayGet(j.tokens,j.count,array,i);
        if (!String(&j,Field(&j,object,"model"),o->model,sizeof(o->model)) || !StudioAssetFilename(o->model,".gltf")
            || !Vector(&j,Field(&j,object,"position"),o->transform.position,3)) { goto done; }
        for (int k=0;k<3;k++) { o->transform.scale[k]=1; }
        if (version>=2 && (!Vector(&j,Field(&j,object,"rotation"),o->transform.rotation,3)
            || !Vector(&j,Field(&j,object,"scale"),o->transform.scale,3))) { goto done; }
        if (!StudioTransformValid(&o->transform)) { goto done; }
        int materials=Field(&j,object,"materials");
        if (materials<0 || j.tokens[materials].type!=GLTF_JSON_ARRAY) { goto done; }
        o->materialcount=GltfJsonArrayCount(j.tokens,j.count,materials);
        if (o->materialcount>4096) { goto done; }
        o->materials=calloc(o->materialcount ? o->materialcount : 1,sizeof(*o->materials));
        if (!o->materials) { goto done; }
        for (DWORD m=0;m<o->materialcount;m++)
        {
            StudioMaterial *mat=&o->materials[m]; double base[3], specular[3], intensity, shine;
            token=GltfJsonArrayGet(j.tokens,j.count,materials,m);
            if (!String(&j,Field(&j,token,"name"),mat->name,sizeof(mat->name))
                || !String(&j,Field(&j,token,"image"),mat->image,sizeof(mat->image))
                || !Vector(&j,Field(&j,token,"base"),base,3) || !Vector(&j,Field(&j,token,"specular"),specular,3)
                || !Number(&j,Field(&j,token,"intensity"),&intensity) || !Number(&j,Field(&j,token,"shininess"),&shine)) { goto done; }
            for (int k=0;k<3;k++) { mat->base[k]=(float)base[k]; mat->specular[k]=(float)specular[k]; }
            mat->intensity=(float)intensity; mat->shininess=(float)shine;
            /* Older scenes retain their original appearance. New fields are
             * required in v5; validate doubles before narrowing to floats. */
            if (version>=5)
            {
                double emission[3],metalness;
                if (!Vector(&j,Field(&j,token,"emission"),emission,3)
                    || !Number(&j,Field(&j,token,"metalness"),&metalness) || metalness<0 || metalness>1) { goto done; }
                for (int k=0;k<3;k++)
                { if (emission[k]<0 || emission[k]>1) { goto done; } mat->emission[k]=(float)emission[k]; }
                mat->metalness=(float)metalness;
            }
            if (version>=7)
            {
                double blur;
                if (!Number(&j,Field(&j,token,"environmentBlur"),&blur) || blur<0 || blur>1) { goto done; }
                mat->environmentblur=(float)blur;
            }
            if (!StudioMaterialValid(mat)) { goto done; }
        }
        const char *assetwhy="";
        o->asset=StudioLoadModel(&next,o->model,&assetwhy);
        if (!o->asset) { missing=TRUE; continue; }
        /* Match overrides by slot/name, then by unique name if an asset's slots
         * were reordered externally. Distinct equal-named slots stay distinct. */
        StudioInstance defaults={0};
        if (!StudioDefaultMaterials(&defaults,o->asset)) { goto done; }
        for (DWORD m=0;m<defaults.materialcount;m++)
        {
            int found=-1, matches=0;
            if (m<o->materialcount && !strcmp(defaults.materials[m].name,o->materials[m].name)) { found=(int)m; }
            else
            {
                for (DWORD n=0;n<o->materialcount;n++) if (!strcmp(defaults.materials[m].name,o->materials[n].name)) { found=(int)n; matches++; }
                for (DWORD n=0;n<defaults.materialcount;n++) if (n!=m && !strcmp(defaults.materials[m].name,defaults.materials[n].name)) { matches=2; }
                if (matches!=1) { found=-1; }
            }
            if (found>=0) { defaults.materials[m]=o->materials[found]; }
        }
        free(o->materials); o->materials=defaults.materials; o->materialcount=defaults.materialcount;
    }
    if (version>=3)
    {
        int lights=Field(&j,0,"lights");
        if (lights<0 || j.tokens[lights].type!=GLTF_JSON_ARRAY) { goto done; }
        DWORD count=GltfJsonArrayCount(j.tokens,j.count,lights);
        if (count>STUDIO_LIGHT_COUNT) { goto done; }
        for (DWORD i=0;i<count;i++)
        {
            int object=GltfJsonArrayGet(j.tokens,j.count,lights,i); char type[16]; double color[3];
            if (!String(&j,Field(&j,object,"type"),type,sizeof(type))) { goto done; }
            BOOL spotlight=!strcmp(type,"spotlight");
            if (!spotlight && strcmp(type,"point")) { goto done; }
            int slot=StudioSceneLightSlot(&next,spotlight),savedslot=Field(&j,object,"slot");
            if (savedslot>=0)
            {
                DWORD value;
                if (!GltfJsonUnsigned(text,&j.tokens[savedslot],&value) || value>=STUDIO_LIGHT_COUNT
                    || (spotlight ? value!=0 : value==0) || next.lights[value].enabled) { goto done; }
                slot=(int)value;
            }
            if (slot<0) { goto done; }
            StudioLight *light=&next.lights[slot]; light->enabled=TRUE;
            if (!Vector(&j,Field(&j,object,"position"),light->position,3)
                || !Vector(&j,Field(&j,object,"color"),color,3)
                || !Number(&j,Field(&j,object,"intensity"),&light->intensity)) { goto done; }
            for (int k=0;k<3;k++)
            {
                if (color[k]<0 || color[k]>1) { goto done; }
                light->color[k]=(float)color[k];
            }
            if (spotlight)
            {
                if (!Vector(&j,Field(&j,object,"direction"),light->direction,3)
                    || !Number(&j,Field(&j,object,"inner"),&light->inner)
                    || !Number(&j,Field(&j,object,"outer"),&light->outer)) { goto done; }
            }
            else if (!Number(&j,Field(&j,object,"radius"),&light->radius)) { goto done; }
            if (!StudioLightValid(light,spotlight)) { goto done; }
        }
    }
    if (version>=4)
    {
        if (!StudioReadGlobalLight(&j,Field(&j,0,"ambient"),FALSE,&next.ambient)
            || !StudioReadGlobalLight(&j,Field(&j,0,"directional"),TRUE,&next.directional)) { goto done; }
    }
    else
    {
        /* Preserve the old preview: its fallback directional light was off
         * whenever any local light existed, even with zero local intensity. */
        StudioSceneDefaultLighting(&next);
        for (int i=0;i<STUDIO_LIGHT_COUNT;i++) if (next.lights[i].enabled) { next.directional.intensity=0; }
    }
    StudioSceneFree(scene); *scene=next; memset(&next,0,sizeof(next));
    *why=missing ? "Some scene models are missing or unreadable. Their instances and material settings were retained." : ""; ok=TRUE;
done:
    if (file) { fclose(file); } free(text); free(j.tokens); StudioSceneFree(&next);
    return ok;
}
