#include "newprops.h"
#include "propcompile.h"
#include "modelcompile.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BANK_LIMIT (64u*1024u*1024u)
typedef struct NewProp {
    char name[64];
    unsigned char *data;
    DWORD size;
    float radius,scale;
} NewProp;
typedef struct NewPropStore { NewProp entries[CUSTOM_PROP_CAPACITY]; DWORD count; } NewPropStore;
static NewPropStore g_Props;
static char g_Project[MAX_PATH];
static BOOL g_Loaded,g_Supported,g_Dirty;
static const char *g_LoadError;
static DWORD Read32(const unsigned char *p) { return (DWORD)p[0]<<24|(DWORD)p[1]<<16|(DWORD)p[2]<<8|p[3]; }
static void Write32(unsigned char *p,DWORD n) { p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n; }
static float ReadFloat(const unsigned char *p) { DWORD n=Read32(p);float f;memcpy(&f,&n,4);return f; }
static void WriteFloat(unsigned char *p,float f) { DWORD n;memcpy(&n,&f,4);Write32(p,n); }
static BOOL NameValid(const char *name)
{
    size_t i,n=strlen(name);
    if (n<3 || n>=64 || name[0]!='P' || name[n-1]!='Z') return FALSE;
    for (i=0;i<n;i++) if (!isalnum((unsigned char)name[i]) && name[i]!='_') return FALSE;
    return TRUE;
}
static BOOL Path(char path[MAX_PATH],const char *project,const char *suffix)
{
    int n=snprintf(path,MAX_PATH,"%s\\models\\newprops.gnp%s",project,suffix);
    return n>=0 && n<MAX_PATH;
}
static void FreeStore(NewPropStore *store)
{
    DWORD i;
    for (i=0;i<store->count;i++) free(store->entries[i].data);
    memset(store,0,sizeof(*store));
}
void NewPropsReset(void)
{
    FreeStore(&g_Props);g_Project[0]=0;g_Loaded=g_Supported=g_Dirty=FALSE;g_LoadError=NULL;
}
BOOL NewPropsHasUnsaved(void) { return g_Dirty; }
int NewPropsCount(void) { return (int)g_Props.count; }
static NewProp *Find(const char *name)
{
    DWORD i;
    for (i=0;i<g_Props.count;i++) if (!strcmp(g_Props.entries[i].name,name)) return &g_Props.entries[i];
    return NULL;
}
BOOL NewPropsDefinition(int id,const char **name,float *scale)
{
    if (id<CUSTOM_PROP_BASE) return FALSE;
    id-=CUSTOM_PROP_BASE;
    if (id<0 || (DWORD)id>=g_Props.count) return FALSE;
    if (name) *name=g_Props.entries[id].name;
    if (scale) *scale=g_Props.entries[id].scale;
    return TRUE;
}
const unsigned char *NewPropsData(const char *project,const char *name,DWORD *size)
{
    NewProp *prop;
    if (strcmp(g_Project,project) || !(prop=Find(name))) return NULL;
    *size=prop->size;return prop->data;
}

static BOOL ParseBank(const unsigned char *data,DWORD size,NewPropStore *store,const char **why)
{
    DWORD count,i,previous;
    *why="The project's new-prop data is damaged or uses an unsupported format.";
    if (size<16 || size>BANK_LIMIT || Read32(data)!=CUSTOM_PROP_MAGIC
        || Read32(data+8)!=CUSTOM_PROP_ENTRY_SIZE || Read32(data+12)!=CUSTOM_PROP_BASE) return FALSE;
    count=Read32(data+4);
    if (count>CUSTOM_PROP_CAPACITY || count>(size-16)/CUSTOM_PROP_ENTRY_SIZE) return FALSE;
    previous=16+count*CUSTOM_PROP_ENTRY_SIZE;
    for (i=0;i<count;i++)
    {
        const unsigned char *entry=data+16+i*CUSTOM_PROP_ENTRY_SIZE;
        DWORD at=Read32(entry+64),length=Read32(entry+68),j;
        NewProp *prop=&store->entries[i];
        ModelSource source={0};
        if (entry[63] || !NameValid((const char *)entry) || at!=previous || (at&15)
            || length<176 || (length&15) || at>size || length>size-at
            || Read32(entry+76)!=ModelDataHash(data+at,length)
            || Read32(entry+84) || Read32(entry+88) || Read32(entry+92)) goto fail;
        for (j=0;j<i;j++) if (!lstrcmpi(store->entries[j].name,(const char *)entry)) goto fail;
        prop->radius=ReadFloat(entry+72);prop->scale=ReadFloat(entry+80);
        if (!isfinite(prop->radius) || prop->radius<=0 || !isfinite(prop->scale) || prop->scale<=0) goto fail;
        /* The runtime header contract requires root=4 and one switch. */
        if (Read32(data+at)!=0x05000004u || data[at+4]!=0 || data[at+5]!=2
            || !ModelReadSource(data+at,length,&source,why)) goto fail;
        j=source.count;ModelFreeSource(&source);
        if (!j) goto fail;
        prop->data=malloc(length);
        if (!prop->data) { *why="Out of memory loading new props.";goto fail; }
        memcpy(prop->data,data+at,length);prop->size=length;lstrcpyn(prop->name,(const char *)entry,64);
        store->count++;previous=at+length;
    }
    if (previous!=size) goto fail;
    *why="";return TRUE;
fail:
    FreeStore(store);return FALSE;
}

static unsigned char *PackBank(const NewPropStore *store,DWORD *size,const char **why)
{
    DWORD i,cursor=16+store->count*CUSTOM_PROP_ENTRY_SIZE,total=cursor;
    unsigned char *data;
    for (i=0;i<store->count;i++)
    {
        if (store->entries[i].size>BANK_LIMIT-total) { *why="The new prop bank exceeds 64 MB.";return NULL; }
        total+=store->entries[i].size;
    }
    data=calloc(total,1);
    if (!data) { *why="Out of memory packing new props.";return NULL; }
    Write32(data,CUSTOM_PROP_MAGIC);Write32(data+4,store->count);
    Write32(data+8,CUSTOM_PROP_ENTRY_SIZE);Write32(data+12,CUSTOM_PROP_BASE);
    for (i=0;i<store->count;i++)
    {
        const NewProp *prop=&store->entries[i];unsigned char *entry=data+16+i*CUSTOM_PROP_ENTRY_SIZE;
        memcpy(entry,prop->name,strlen(prop->name));Write32(entry+64,cursor);Write32(entry+68,prop->size);
        WriteFloat(entry+72,prop->radius);Write32(entry+76,ModelDataHash(prop->data,prop->size));
        WriteFloat(entry+80,prop->scale);memcpy(data+cursor,prop->data,prop->size);cursor+=prop->size;
    }
    *size=total;return data;
}

/* 0: old ROM without this feature; 1: valid config; -1: malformed contract. */
static int Config(const RomFile *rom,DWORD *config,DWORD *bankindex,const char **why)
{
    DWORD i;const RomManifestEntry *entry=NULL,*bank=NULL;
    for (i=0;i<rom->info.entrycount;i++)
    {
        if (rom->info.entries[i].kind==CUSTOM_PROP_MANIFEST_KIND)
        { if (entry) { *why="Duplicate new-prop config entry.";return -1; } entry=&rom->info.entries[i]; }
        if (rom->info.entries[i].kind==CUSTOM_PROP_DATA_KIND)
        { if (bank) { *why="Duplicate new-prop bank entry.";return -1; } bank=&rom->info.entries[i];*bankindex=i; }
    }
    if (!entry && !bank) return 0;
    *why="The ROM has an invalid new-prop manifest.";
    if (!entry || !bank || entry->romstart>rom->size || rom->size-entry->romstart<16
        || entry->romend!=entry->romstart+16 || entry->flags!=CUSTOM_PROP_CONFIG_VERSION
        || Read32(rom->data+entry->romstart)!=CUSTOM_PROP_CONFIG_VERSION
        || Read32(rom->data+entry->romstart+12)
        || bank->flags!=CUSTOM_PROP_CONFIG_VERSION) return -1;
    *config=entry->romstart;
    if (bank->romstart>bank->romend || bank->romend>rom->size
        || Read32(rom->data+*config+4)!=bank->romstart
        || Read32(rom->data+*config+8)!=bank->romend-bank->romstart) return -1;
    if (bank->romend!=bank->romstart)
    {
        DWORD manifestend=rom->info.manifestoffset+24+rom->info.entrycount*16;
        if (bank->romstart<0x101000u || (bank->romstart&15)
            || (bank->romstart<manifestend && bank->romend>rom->info.manifestoffset)) return -1;
        for (i=0;i<rom->info.entrycount;i++)
        {
            const RomManifestEntry *other=&rom->info.entries[i];
            if (other!=bank && other->romend>other->romstart
                && bank->romstart<other->romend && bank->romend>other->romstart) return -1;
        }
    }
    *why="";return 1;
}
static int ReadSaved(const char *project,NewPropStore *store,const char **why)
{
    char path[MAX_PATH];FILE *file;long size;unsigned char *data;BOOL ok;
    *why="The saved new-prop bank could not be read.";
    if (!Path(path,project,"")) return -1;
    if (GetFileAttributes(path)==INVALID_FILE_ATTRIBUTES)
    {
        DWORD error=GetLastError();
        if (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) { *why="";return 0; }
        return -1;
    }
    file=fopen(path,"rb");if (!file) return -1;
    if (fseek(file,0,SEEK_END) || (size=ftell(file))<16 || (unsigned long)size>BANK_LIMIT || fseek(file,0,SEEK_SET))
    { fclose(file);return -1; }
    data=malloc(size);ok=data && fread(data,1,size,file)==(size_t)size;
    fclose(file);
    if (ok) ok=ParseBank(data,(DWORD)size,store,why);
    free(data);return ok?1:-1;
}

BOOL NewPropsOpen(const char *project,const char **why)
{
    char path[MAX_PATH];RomFile rom={0};NewPropStore base={0};DWORD config=0,index=0,i;int result,saved;
    *why="";
    if (g_Loaded && !strcmp(project,g_Project)) { *why=g_LoadError?g_LoadError:"";return !g_LoadError; }
    NewPropsReset();lstrcpyn(g_Project,project,MAX_PATH);g_Loaded=TRUE;
    if (snprintf(path,sizeof(path),"%s\\base.z64",project)>=(int)sizeof(path)
        || !RomLoad(path,&rom,why)) goto fail;
    result=Config(&rom,&config,&index,why);if (result<0) goto fail;
    g_Supported=result==1;
    if (g_Supported && Read32(rom.data+config+8)
        && !ParseBank(rom.data+Read32(rom.data+config+4),Read32(rom.data+config+8),&base,why)) goto fail;
    saved=ReadSaved(project,&g_Props,why);if (saved<0) goto fail;
    if (!saved) { g_Props=base;memset(&base,0,sizeof(base));g_Dirty=g_Props.count!=0; }
    else
    {
        if (base.count>g_Props.count) { *why="The base ROM has additional prop IDs absent from the project.";goto fail; }
        for (i=0;i<base.count;i++) if (strcmp(base.entries[i].name,g_Props.entries[i].name))
        { *why="A new prop ID means a different model in this base ROM.";goto fail; }
    }
    FreeStore(&base);RomFree(&rom);*why="";return TRUE;
fail:
    FreeStore(&base);FreeStore(&g_Props);RomFree(&rom);
    g_LoadError=(*why && **why)?*why:"The new props could not be loaded.";*why=g_LoadError;return FALSE;
}

BOOL NewPropsImport(const char *project,const char *name,const char *path,BOOL replace,DWORD *triangles,const char **why)
{
    BgVertex *vertices=NULL;unsigned short *tags=NULL;BgRenderFlags *flags=NULL;
    unsigned char *data=NULL;DWORD count,size;float radius;NewProp *prop;BOOL ok=FALSE;
    ModelSource check={0};RomFile rom={0};char base[MAX_PATH];DWORD offset,span;
    if (!NewPropsOpen(project,why)) return FALSE;
    if (!g_Supported) { *why="Rebuild GUD with new-prop support, then rebase this project to that ROM before adding models.";return FALSE; }
    if (!NameValid(name)) { *why="Use a unique prop name such as PpendantZ (letters, digits and underscores, ending in Z).";return FALSE; }
    prop=Find(name);
    if ((replace && !prop) || (!replace && prop)) { *why="That prop name is already used, or the reimport target no longer exists.";return FALSE; }
    if (!replace)
    {
        DWORD i;
        for (i=0;i<CUSTOM_PROP_BASE;i++)
        {
            const char *stock;
            if (ModelGetPropDefinition(i,&stock,NULL) && !lstrcmpi(name,stock))
            { *why="This name belongs to an existing prop model.";return FALSE; }
        }
        for (i=0;i<g_Props.count;i++) if (!lstrcmpi(name,g_Props.entries[i].name))
        { *why="Another prop already uses that name.";return FALSE; }
        if (g_Props.count>=CUSTOM_PROP_CAPACITY) { *why="The project already contains 128 added prop models.";return FALSE; }
        snprintf(base,sizeof(base),"%s\\base.z64",project);
        if (!RomLoad(base,&rom,why)) goto done;
        if (RomFindFile(&rom,name,&offset,&span,why)) { *why="This name belongs to an existing ROM resource.";goto done; }
    }
    vertices=GltfReadNewProp(path,project,&count,&tags,&flags,why);
    if (!vertices && !**why) *why="The selected scene contains no triangles.";
    if (!vertices || !PropCompile(vertices,tags,flags,count,project,&data,&size,&radius,why)
        || !ModelReadSource(data,size,&check,why)) goto done;
    if (check.count!=count) { *why="The compiled model did not reproduce every imported triangle.";goto done; }
    if (!prop) { prop=&g_Props.entries[g_Props.count++];lstrcpyn(prop->name,name,64); }
    free(prop->data);prop->data=data;data=NULL;prop->size=size;prop->radius=radius;prop->scale=.1f;
    *triangles=count;g_Dirty=TRUE;*why="";ok=TRUE;
done:
    RomFree(&rom);ModelFreeSource(&check);free(vertices);free(tags);free(flags);free(data);return ok;
}

BOOL NewPropsReplace(const char *name,unsigned char *data,DWORD size,const char **why)
{
    NewProp *prop=Find(name);DWORD aligned=(size+15)&~15u;unsigned char *grown;
    if (!prop) { *why="The new prop is no longer available.";return FALSE; }
    if (aligned!=size)
    {
        grown=malloc(aligned);if (!grown) { *why="Out of memory retaining model properties.";return FALSE; }
        memcpy(grown,data,size);memset(grown+size,0,aligned-size);free(data);data=grown;
    }
    free(prop->data);prop->data=data;prop->size=aligned;g_Dirty=TRUE;*why="";return TRUE;
}

BOOL NewPropsSave(const char *project,const char **why)
{
    char path[MAX_PATH],temp[MAX_PATH];unsigned char *data;DWORD size,i;FILE *file;BOOL ok;
    if (!NewPropsOpen(project,why)) return FALSE;
    if (!g_Dirty) return TRUE;
    if (!Path(path,project,"") || !Path(temp,project,".tmp")) { *why="The new-prop save path is too long.";return FALSE; }
    data=PackBank(&g_Props,&size,why);if (!data) return FALSE;
    file=fopen(temp,"wb");ok=file && fwrite(data,1,size,file)==size;
    if (file && fclose(file)) ok=FALSE;
    free(data);
    if (!ok || !MoveFileEx(temp,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    { DeleteFile(temp);*why="New props could not be saved. Their pending imports are still available.";return FALSE; }
    for (i=0;i<g_Props.count;i++)
    {
        NewProp *prop=&g_Props.entries[i];ModelSource source={0};
        if (snprintf(path,sizeof(path),"%s\\models\\objects\\%s.gltf",project,prop->name)>=(int)sizeof(path)
            || !ModelReadSource(prop->data,prop->size,&source,why)) return FALSE;
        ok=GltfWriteEditableModel(path,project,&source,ModelDataHash(prop->data,prop->size),why);
        ModelFreeSource(&source);if (!ok) return FALSE;
    }
    g_Dirty=FALSE;*why="";return TRUE;
}

BOOL NewPropsCheckRebase(const char *project,const RomFile *rom,const char **why)
{
    NewPropStore saved={0},base={0};DWORD config=0,index=0,i;int result,found;BOOL ok=FALSE;
    found=ReadSaved(project,&saved,why);if (found<0) return FALSE;
    result=Config(rom,&config,&index,why);if (result<0) goto done;
    if (saved.count && result!=1)
    { *why="The new ROM lacks the new-prop runtime required by this project.";goto done; }
    if (result==1 && Read32(rom->data+config+8)
        && !ParseBank(rom->data+Read32(rom->data+config+4),Read32(rom->data+config+8),&base,why)) goto done;
    if (found) for (i=0;i<base.count;i++)
    {
        if (i>=saved.count || strcmp(base.entries[i].name,saved.entries[i].name))
        { *why="The new ROM assigns an added prop ID to a different model.";goto done; }
    }
    ok=TRUE;*why="";
done:
    FreeStore(&saved);FreeStore(&base);return ok;
}

BOOL NewPropsExportToRom(const char *project,RomFile *rom,const char **why)
{
    NewPropStore saved={0};unsigned char *packed=NULL,*grown;DWORD size,config=0,index=0,target,oldstart,oldsize,newsize,i,at;
    int result,found;BOOL ok=FALSE;
    found=ReadSaved(project,&saved,why);if (found<0) return FALSE;
    if (!found) return TRUE; /* The unchanged bank already lives in base.z64. */
    result=Config(rom,&config,&index,why);
    if (result!=1) { if (!result) *why="The base ROM needs GUD's new-prop runtime before these models can be exported.";goto done; }
    if (!NewPropsCheckRebase(project,rom,why)) goto done;
    packed=PackBank(&saved,&size,why);if (!packed) goto done;
    oldstart=Read32(rom->data+config+4);oldsize=Read32(rom->data+config+8);
    target=oldsize>=size?oldstart:rom->info.manifestoffset+24+rom->info.entrycount*16;
    if (oldsize<size)
    {
        if (target<0x101000u) target=0x101000u;
        for (i=0;i<rom->info.entrycount;i++) if (target<rom->info.entries[i].romend) target=rom->info.entries[i].romend;
        if (target>rom->size) { *why="The ROM contains an invalid occupied range.";goto done; }
        for (i=rom->size;i>target;i--) if (rom->data[i-1]) { target=i;break; }
        target=(target+15)&~15u;
    }
    if (target>BANK_LIMIT || size>BANK_LIMIT-target) { *why="Added props would exceed the 64 MB ROM limit.";goto done; }
    newsize=rom->size;
    if (target+size>newsize)
    {
        newsize=1024u*1024u;while (newsize<target+size) newsize*=2;
        grown=realloc(rom->data,newsize);if (!grown) { *why="Out of memory expanding the ROM.";goto done; }
        memset(grown+rom->size,0,newsize-rom->size);rom->data=grown;rom->size=rom->info.size=newsize;
    }
    if (oldsize) memset(rom->data+oldstart,0,oldsize);
    memcpy(rom->data+target,packed,size);Write32(rom->data+config+4,target);Write32(rom->data+config+8,size);
    at=rom->info.manifestoffset+24+index*16;
    Write32(rom->data+at+4,target);Write32(rom->data+at+8,target+size);
    rom->info.entries[index].romstart=target;rom->info.entries[index].romend=target+size;
    *why="";ok=TRUE;
done:
    free(packed);FreeStore(&saved);return ok;
}
