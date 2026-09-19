/* Rebase saved edits onto a compatible GUD ROM. ROM addresses may move;
 * resource names/IDs and native model content may not. Image banks may gain
 * or lose an appended suffix; surviving image IDs must still agree.
 * BG/setup/stan files and editable level fields use a three-way merge.
 * Binary conflicts are reported, never guessed or merged byte by byte. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include "projectrebase.h"
#include "romexport.h"
#include "texrom.h"
#include "imageedits.h"
#include "newprops.h"

#define REBASE_MAX_FILES 1024u
typedef struct RebaseUpdate {
    char path[MAX_PATH];
    DWORD offset, size;
} RebaseUpdate;
typedef struct RebasePlan {
    RomFile oldrom, newrom;
    GEditorProject project;
    RebaseUpdate updates[REBASE_MAX_FILES];
    DWORD count;
} RebasePlan;
static char g_RebaseError[512];

static BOOL Fail(const char **why, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(g_RebaseError, sizeof(g_RebaseError), format, args);
    va_end(args);
    *why = g_RebaseError;
    return FALSE;
}
static void Conflict(ProjectRebaseReport *report, const char *name, const char *detail)
{
    size_t used = strlen(report->details);
    report->conflicts++;
    if (used < sizeof(report->details) - 256)
    {
        snprintf(report->details + used, sizeof(report->details) - used,
            "%s: %s\r\n", name, detail);
    }
}
static DWORD Read32(const unsigned char *p)
{ return (DWORD)p[0]<<24 | (DWORD)p[1]<<16 | (DWORD)p[2]<<8 | p[3]; }
static const RomManifestEntry *Entry(const RomFile *rom, DWORD kind)
{
    DWORD i;
    const RomManifestEntry *result = NULL;
    for (i = 0; i < rom->info.entrycount; i++) if (rom->info.entries[i].kind == kind)
    {
        if (result) { return NULL; }
        result = &rom->info.entries[i];
    }
    return result;
}
static BOOL Join(char path[MAX_PATH], const char *dir, const char *name, const char **why)
{
    int n = snprintf(path, MAX_PATH, "%s\\%s", dir, name);
    return (n >= 0 && n < MAX_PATH) || Fail(why, "A project path is too long: %s", name);
}
static BOOL DirectoryPath(const char *path, char canonical[MAX_PATH], const char **why)
{
    HANDLE handle;
    DWORD length;
    handle = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (handle == INVALID_HANDLE_VALUE) { return Fail(why, "Cannot open project directory: %s", path); }
    /* Resolving junctions here prevents a destination alias from pointing
     * back inside the source. Child reparse points are refused during copy. */
    length = GetFinalPathNameByHandle(handle, canonical, MAX_PATH, FILE_NAME_NORMALIZED);
    CloseHandle(handle);
    if (!length || length >= MAX_PATH) { return Fail(why, "Cannot resolve directory: %s", path); }
    while (length && (canonical[length-1] == '\\' || canonical[length-1] == '/')) { canonical[--length] = 0; }
    return TRUE;
}
BOOL ProjectRebaseDestination(const GEditorProject *source, const char *parent,
    const char *name, char destination[MAX_PATH], const char **why)
{
    char original[MAX_PATH], target[MAX_PATH], prefix[MAX_PATH];
    DWORD attrs;
    *why = "";
    if (!source || !source->dir[0]) { return Fail(why, "Open a project first."); }
    if (!name || strlen(name) >= GEDITOR_NAME_MAX || !RomExportNameIsValid(name, why))
    { return Fail(why, "Enter a valid project name (up to %u characters).", GEDITOR_NAME_MAX - 1); }
    attrs = GetFileAttributes(parent);
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
    { return Fail(why, "Choose an existing destination directory."); }
    if (!DirectoryPath(source->dir, original, why) || !DirectoryPath(parent, target, why)) { return FALSE; }
    if (!Join(prefix, original, "", why)) { return FALSE; }
    if (!lstrcmpi(original, target) || !_strnicmp(prefix, target, strlen(prefix)))
    { return Fail(why, "Choose a destination outside the current project folder."); }
    if (!Join(destination, parent, name, why)) { return FALSE; }
    if (GetFileAttributes(destination) != INVALID_FILE_ATTRIBUTES)
    { return Fail(why, "That destination already exists. Choose a new project name."); }
    if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND)
    { return Fail(why, "The destination cannot be checked. Choose another directory."); }
    return TRUE;
}

static const unsigned char *Mapped(const RomFile *rom, DWORD address, DWORD size)
{
    const RomManifestEntry *map = Entry(rom, 0x434d4150u);
    DWORD offset;
    if (!map || address < map->flags || address-map->flags > map->romend-map->romstart) { return NULL; }
    offset = map->romstart + address-map->flags;
    return size <= map->romend-offset ? rom->data+offset : NULL;
}
static const char *MappedString(const RomFile *rom, DWORD address)
{
    const unsigned char *p = Mapped(rom, address, 1);
    const RomManifestEntry *map = Entry(rom, 0x434d4150u);
    if (!p || !memchr(p, 0, rom->data + map->romend - p)) { return NULL; }
    return (const char *)p;
}
static BOOL Catalogs(const RomFile *a, const RomFile *b, const char **why)
{
    static const struct { DWORD tag, stride; } catalogs[] = {
        {0x4348524du,20}, {0x50524f50u,12}, {0x4954454du,56}
    };
    unsigned int kind;
    for (kind = 0; kind < sizeof(catalogs)/sizeof(catalogs[0]); kind++)
    {
        const RomManifestEntry *ae = Entry(a,catalogs[kind].tag), *be = Entry(b,catalogs[kind].tag);
        const unsigned char *ad, *bd, *ar, *br;
        DWORD count, i, stride = catalogs[kind].stride;
        if (!ae && !be) { continue; } /* Older v3 ROMs may omit discovery tables. */
        if (!ae || !be || ae->flags != 0x80000000u || be->flags != ae->flags
            || ae->romend-ae->romstart != 16 || be->romend-be->romstart != 16)
        { return Fail(why, "The ROMs have incompatible model catalogs. Use matching GUD/editor formats."); }
        ad = a->data+ae->romstart; bd = b->data+be->romstart;
        count = Read32(ad+4);
        if (Read32(ad+8) != stride || Read32(ad+12) != 1 || memcmp(ad+4,bd+4,12)
            || count > a->size/stride || count > b->size/stride
            || !(ar = Mapped(a,Read32(ad),count*stride)) || !(br = Mapped(b,Read32(bd),count*stride)))
        { return Fail(why, "Model catalog sizes, IDs or formats changed; this rebase needs asset migration."); }
        for (i = 0; i < count; i++, ar+=stride, br+=stride)
        {
            DWORD an = Read32(ar+4), bn = Read32(br+4);
            const char *as = an ? MappedString(a,an) : "", *bs = bn ? MappedString(b,bn) : "";
            /* Header/stat pointers may relocate with optimized code. Compare
             * the actual model name and all scalar fields, not those addresses. */
            if (!as || !bs || !!an != !!bn || strcmp(as,bs) || memcmp(ar+8,br+8,4)
                || (stride == 20 && memcmp(ar+12,br+12,8))
                || (stride == 56 && memcmp(ar+16,br+16,40)))
            { return Fail(why, "Model catalog ID %lu changed (%s); this version does not remap model IDs or scales.",
                (unsigned long)i, as ? as : "invalid name"); }
        }
    }
    return TRUE;
}
static BOOL ImageBank(const RomFile *a, RomFile *b, ProjectRebaseReport *report, const char **why)
{
    TexRomBank oldbank, newbank;
    DWORD i, oldat, newat, common;
    const unsigned char **records=NULL;
    DWORD *sizes=NULL;
    unsigned char *surfaces=NULL;
    BOOL ok;
    if (!TexRomReadBank(a,&oldbank,why) || !TexRomReadBank(b,&newbank,why)) { return FALSE; }
    common=oldbank.count<newbank.count ? oldbank.count : newbank.count;
    oldat=oldbank.images; newat=newbank.images;
    for (i=0;i<common;i++)
    {
        DWORD size=Read32(a->data+oldbank.table+i*8)&0xffffffu;
        /* Compare each complete table entry first (size, surface and detail
         * flags). No prefix hash or pixel-only match can establish identity. */
        if (memcmp(a->data+oldbank.table+i*8,b->data+newbank.table+i*8,8)
            || memcmp(a->data+oldat,b->data+newat,size))
        { return Fail(why,"Image %04lX changed in the base ROM. Rebase cannot automatically merge different textures or image settings at the same ID.",(unsigned long)i); }
        oldat+=size; newat+=size;
    }
    if (oldbank.count<=newbank.count)
    { report->imagesadded=newbank.count-oldbank.count; return TRUE; }
    if (oldbank.count>newbank.capacity)
    { return Fail(why,"The new ROM has insufficient image capacity to retain the project's base images."); }
    /* A project made from an exported ROM can have textures baked into its
     * base that a fresh GUD build lacks. Retain that suffix at the same IDs
     * in the private incoming ROM; no BG/model/material references move. */
    records=calloc(oldbank.count,sizeof(*records)); sizes=calloc(oldbank.count,sizeof(*sizes));
    surfaces=calloc(oldbank.count,1);
    if (!records || !sizes || !surfaces)
    { free(records); free(sizes); free(surfaces); return Fail(why,"Out of memory retaining the project's base images."); }
    for (i=common;i<oldbank.count;i++)
    {
        records[i]=a->data+oldat; sizes[i]=Read32(a->data+oldbank.table+i*8)&0xffffffu;
        /* A legacy original can use flags unavailable to fresh imports.
         * Supply neutral import flags here and restore its full entry below. */
        oldat+=sizes[i];
    }
    ok=TexRomUpdateImages(b,&newbank,records,sizes,surfaces,oldbank.count,why);
    if (ok)
    {
        /* These are originals, not fresh imports. Preserve all detail flags
         * too; TexRomUpdateImages normally clears them on replacement. */
        memcpy(b->data+newbank.table+common*8,a->data+oldbank.table+common*8,(oldbank.count-common)*8);
        report->imagesretained=oldbank.count-common;
    }
    free(records); free(sizes); free(surfaces); return ok;
}
static BOOL LevelNames(const RomLevel *a, const RomLevel *b)
{
    return a->levelID == b->levelID && !strcmp(a->setupname,b->setupname)
        && !strcmp(a->bgname,b->bgname) && !strcmp(a->stanname,b->stanname);
}
static const RomLevel *Level(const RomInfo *info, LONG id)
{
    DWORD i;
    const RomLevel *found = NULL;
    for (i=0;i<info->levelcount;i++) if (info->levels[i].levelID == id)
    { if (found) { return NULL; } found=&info->levels[i]; }
    return found;
}
static BOOL Levels(RebasePlan *plan, const GEditorProject *source,
    ProjectRebaseReport *report, const char **why)
{
    DWORD i;
    plan->project = *source;
    if (!source->levelcount || source->levelcount != plan->oldrom.info.levelcount
        || plan->newrom.info.levelcount > ROM_MAX_LEVELS)
    { return Fail(why, "The project's level table does not match its base ROM."); }
    for (i=0;i<source->levelcount;i++)
    {
        RomLevel *p=&plan->project.levels[i];
        const RomLevel *a=Level(&plan->oldrom.info,p->levelID), *b=Level(&plan->newrom.info,p->levelID);
        DWORD j;
        for (j=0;j<i;j++) if (source->levels[j].levelID==p->levelID)
        { return Fail(why,"The project contains duplicate level IDs."); }
        if (!a || !b || !LevelNames(a,b) || !LevelNames(a,p))
        { return Fail(why, "Level %ld has missing, duplicate or changed resource names.", (long)p->levelID); }
        /* Coordinate scale affects BG/setup/stan as a group. Do not merge
         * it independently and silently change the meaning of saved positions. */
        if (!isfinite(p->levelscale) || p->levelscale <= 0 || a->levelscale != b->levelscale)
        { Conflict(report,p->name,"world coordinate scale changed; geometry migration required"); }
#define MERGE_FIELD(field) \
        if (p->field == a->field) { p->field=b->field; } \
        else if (b->field != a->field && p->field != b->field) { Conflict(report,p->name,#field " changed in both project and ROM"); }
        MERGE_FIELD(renderScale)
        MERGE_FIELD(music)
        MERGE_FIELD(bgsound)
        MERGE_FIELD(xtrack)
#undef MERGE_FIELD
        lstrcpyn(p->name,b->name,sizeof(p->name));
        lstrcpyn(p->world,b->world,sizeof(p->world));
        p->hasbackgroundcolor=b->hasbackgroundcolor;
        memcpy(p->backgroundcolor,b->backgroundcolor,sizeof(p->backgroundcolor)); p->fog=b->fog; p->clouds=b->clouds;
    }
    /* New named rows (MP variants and Title) can be added without changing
     * any existing resource IDs. Resources() still verifies the file catalog. */
    for (i = 0; i < plan->newrom.info.levelcount; i++)
    {
        const RomLevel *level = &plan->newrom.info.levels[i];
        if (!Level(&plan->oldrom.info, level->levelID))
        {
            if (plan->project.levelcount >= ROM_MAX_LEVELS)
            { return Fail(why, "The expanded level table exceeds the editor's capacity."); }
            plan->project.levels[plan->project.levelcount++] = *level;
        }
    }
    return TRUE;
}
static BOOL ReadFileBytes(const char *path, unsigned char **data, DWORD *size, const char **why)
{
    HANDLE file;
    DWORD got, high=0;
    BOOL ok;
    *data=NULL; *size=0;
    file=CreateFile(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if (file==INVALID_HANDLE_VALUE) { return Fail(why,"Cannot read %s",path); }
    *size=GetFileSize(file,&high);
    if (high || *size==INVALID_FILE_SIZE || *size>64u*1024u*1024u || !(*data=malloc(*size ? *size : 1)))
    { CloseHandle(file); return Fail(why,"Cannot load project asset (too large or out of memory): %s",path); }
    ok=ReadFile(file,*data,*size,&got,NULL) && got==*size;
    if (!CloseHandle(file)) { ok=FALSE; }
    if (!ok) { free(*data); *data=NULL; return Fail(why,"Could not fully read %s",path); }
    return TRUE;
}
static BOOL WriteFileBytes(const char *path, const unsigned char *data, DWORD size, const char **why)
{
    HANDLE file=CreateFile(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    DWORD written;
    BOOL ok;
    if (file==INVALID_HANDLE_VALUE) { return Fail(why,"Cannot write %s",path); }
    ok=WriteFile(file,data,size,&written,NULL) && written==size;
    if (!CloseHandle(file)) { ok=FALSE; }
    return ok || Fail(why,"Could not fully write %s",path);
}
static BOOL Equal(const unsigned char *a, DWORD asize, const unsigned char *b, DWORD bsize)
{ return asize==bsize && !memcmp(a,b,asize); }
static BOOL FileCount(const RomFile *rom, DWORD *count, const char **why)
{
    const RomManifestEntry *table=Entry(rom,0x4654424cu);
    const RomManifestEntry *map=Entry(rom,0x434d4150u);
    DWORD i;
    if (table && map && table->romstart>=map->romstart) for (i=0;i<REBASE_MAX_FILES;i++)
    {
        DWORD at=table->romstart+i*12;
        if (at>map->romend || map->romend-at<12) { break; }
        if (!Read32(rom->data+at+4)) { *count=i; return TRUE; }
    }
    return Fail(why,"The ROM file table has no valid terminator.");
}
static BOOL Resources(RebasePlan *plan, const GEditorProject *source,
    ProjectRebaseReport *report, const char **why)
{
    DWORD count, newcount, i;
    const RomManifestEntry *oldtable=Entry(&plan->oldrom,0x4654424cu), *newtable=Entry(&plan->newrom,0x4654424cu);
    if (!FileCount(&plan->oldrom,&count,why) || !FileCount(&plan->newrom,&newcount,why)) { return FALSE; }
    if (count!=newcount) { return Fail(why,"The ROM file IDs changed. This version does not remap resource IDs."); }
    for (i=0;i<count;i++)
    {
        char name[64], newname[64], path[MAX_PATH];
        const char *full=MappedString(&plan->oldrom,Read32(plan->oldrom.data+oldtable->romstart+i*12+4));
        const char *newfull=MappedString(&plan->newrom,Read32(plan->newrom.data+newtable->romstart+i*12+4));
        DWORD oldat,oldsize,newat,newsize,size,attrs;
        unsigned char *data;
        int managed;
        BOOL changed;
        if (!full || !newfull || strlen(full)>=sizeof(name) || strlen(newfull)>=sizeof(newname)
            || !RomGetFileByIndex(&plan->oldrom,i,name,sizeof(name),NULL,NULL)
            || !RomGetFileByIndex(&plan->newrom,i,newname,sizeof(newname),NULL,NULL)
            || strcmp(name,newname) || memcmp(plan->oldrom.data+oldtable->romstart+i*12,plan->newrom.data+newtable->romstart+i*12,4))
        { return Fail(why,"ROM resource ID %lu is missing or has changed names.",(unsigned long)i); }
        if (!name[0]) { continue; }
        if (!RomGetFileByIndex(&plan->oldrom,i,name,sizeof(name),&oldat,&oldsize)
            || !RomGetFileByIndex(&plan->newrom,i,newname,sizeof(newname),&newat,&newsize))
        { return Fail(why,"Invalid ROM resource: %s",name); }
        report->checked++;
        changed=!Equal(plan->oldrom.data+oldat,oldsize,plan->newrom.data+newat,newsize);
        managed=RomExportProjectResourcePath(source,name,path,sizeof(path));
        if (managed<0) { return Fail(why,"Invalid project resource path: %s",name); }
        if (!managed)
        {
            /* Includes native models and other resources for which the editor
             * has no complete merge schema. Keep model fingerprints intact. */
            if (changed) { Conflict(report,name,"base asset changed; this version supports code and level-resource updates only"); }
            continue;
        }
        attrs=GetFileAttributes(path);
        if (attrs==INVALID_FILE_ATTRIBUTES)
        {
            DWORD error=GetLastError();
            if (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) { continue; }
            return Fail(why,"Cannot inspect project asset: %s",path);
        }
        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) || !ReadFileBytes(path,&data,&size,why)) { return Fail(why,"Cannot read project asset: %s",path); }
        if (changed && !Equal(data,size,plan->oldrom.data+oldat,oldsize)
            && !Equal(data,size,plan->newrom.data+newat,newsize))
        { Conflict(report,name,"changed differently in the project and the new ROM"); }
        else if (changed)
        {
            RebaseUpdate *update=&plan->updates[plan->count++];
            lstrcpyn(update->path,path+strlen(source->dir)+1,sizeof(update->path));
            update->offset=newat; update->size=newsize; report->updated++;
        }
        else if (!Equal(data,size,plan->oldrom.data+oldat,oldsize)) { report->kept++; }
        free(data);
    }
    return TRUE;
}
static BOOL Prepare(RebasePlan *plan, const GEditorProject *source, const char *rompath,
    ProjectRebaseReport *report, const char **why)
{
    char base[MAX_PATH];
    ZeroMemory(report,sizeof(*report)); *why="";
    if (!source || !source->dir[0] || source->levelcount>ROM_MAX_LEVELS) { return Fail(why,"Open a valid saved project first."); }
    if (!Join(base,source->dir,ROM_EXPORT_BASE_FILENAME,why)
        || !RomLoad(base,&plan->oldrom,why) || !RomLoad(rompath,&plan->newrom,why)
        || !ImageBank(&plan->oldrom,&plan->newrom,report,why) || !Catalogs(&plan->oldrom,&plan->newrom,why)
        || !NewPropsCheckRebase(source->dir,&plan->newrom,why)
        || !Levels(plan,source,report,why) || !Resources(plan,source,report,why)) { return FALSE; }
    if (report->conflicts) { return Fail(why,"Rebase blocked by %lu conflict(s). See the report.",(unsigned long)report->conflicts); }
    /* Validate against the original base before new stock IDs can turn an
     * orphan BMP or a gap in imported IDs into an apparently valid asset. */
    return RomExportValidateProject(source,why)
        && ImageEditsRebase(source->dir,&plan->oldrom,&plan->newrom,FALSE,why);
}
static void FreePlan(RebasePlan *plan)
{ if (plan) { RomFree(&plan->oldrom); RomFree(&plan->newrom); free(plan); } }
BOOL ProjectRebaseCheck(const GEditorProject *source, const char *rompath,
    ProjectRebaseReport *report, const char **why)
{
    RebasePlan *plan=calloc(1,sizeof(*plan));
    BOOL ok;
    ZeroMemory(report,sizeof(*report));
    if (!plan) { return Fail(why,"Out of memory checking the project."); }
    ok=Prepare(plan,source,rompath,report,why);
    FreePlan(plan); return ok;
}

/* Copy only into a directory owned by this operation. Never follow reparse
 * points, recurse into the destination, overwrite a user file, or discard an
 * enumeration/I/O error. All project sidecars and source-path settings survive. */
static BOOL CopyTree(const char *source, const char *target, const char *skip, const char **why)
{
    char pattern[MAX_PATH], from[MAX_PATH], to[MAX_PATH];
    WIN32_FIND_DATA entry;
    HANDLE search;
    DWORD error;
    BOOL ok=TRUE;
    if (!Join(pattern,source,"*",why)) { return FALSE; }
    search=FindFirstFile(pattern,&entry);
    if (search==INVALID_HANDLE_VALUE)
    { return GetLastError()==ERROR_FILE_NOT_FOUND || Fail(why,"Cannot list directory: %s",source); }
    do
    {
        if (!strcmp(entry.cFileName,".") || !strcmp(entry.cFileName,"..")) { continue; }
        if (skip && !lstrcmpi(entry.cFileName,skip)) { continue; }
        if (!Join(from,source,entry.cFileName,why) || !Join(to,target,entry.cFileName,why)) { ok=FALSE; break; }
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        { ok=Fail(why,"Linked files/folders cannot be rebased: %s",from); break; }
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!CreateDirectory(to,NULL)) { ok=Fail(why,"Cannot create directory: %s",to); break; }
            if (!CopyTree(from,to,NULL,why)) { ok=FALSE; break; }
        }
        else if (!CopyFile(from,to,TRUE) || !SetFileAttributes(to,FILE_ATTRIBUTE_NORMAL))
        { ok=Fail(why,"Could not copy project file: %s",from); break; }
    } while (FindNextFile(search,&entry));
    error=GetLastError(); FindClose(search);
    if (ok && error!=ERROR_NO_MORE_FILES) { ok=Fail(why,"Could not finish listing: %s",source); }
    return ok;
}
static BOOL RemoveTree(const char *dir)
{
    char pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATA entry;
    HANDLE search;
    const char *why="";
    BOOL ok=TRUE;
    DWORD error;
    if (!Join(pattern,dir,"*",&why)) { return FALSE; }
    search=FindFirstFile(pattern,&entry);
    if (search!=INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!strcmp(entry.cFileName,".") || !strcmp(entry.cFileName,"..")) { continue; }
            if (!Join(path,dir,entry.cFileName,&why)) { ok=FALSE; continue; }
            if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (entry.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { if (!RemoveDirectory(path)) { ok=FALSE; } }
                else if (!RemoveTree(path)) { ok=FALSE; }
            }
            else { SetFileAttributes(path,FILE_ATTRIBUTE_NORMAL); if (!DeleteFile(path)) { ok=FALSE; } }
        } while (FindNextFile(search,&entry));
        error=GetLastError(); FindClose(search);
        if (error!=ERROR_NO_MORE_FILES) { ok=FALSE; }
    }
    else if (GetLastError()!=ERROR_FILE_NOT_FOUND) { ok=FALSE; }
    return RemoveDirectory(dir) && ok;
}
BOOL ProjectRebaseCreate(const GEditorProject *source, const char *rompath,
    const char *parent, const char *name, GEditorProject *output,
    ProjectRebaseReport *report, const char **why)
{
    char destination[MAX_PATH], staging[MAX_PATH], filename[MAX_PATH], path[MAX_PATH];
    const char *basename;
    RebasePlan *plan=NULL;
    GEditorProject snapshot;
    DWORD attempt,i;
    BOOL owned=FALSE,ok=FALSE;
    ZeroMemory(output,sizeof(*output)); ZeroMemory(report,sizeof(*report)); staging[0]=0;
    if (!ProjectRebaseDestination(source,parent,name,destination,why)) { return FALSE; }
    /* Reserve a sibling staging directory exclusively; no existing folder is
     * ever treated as ours, even after a stale temporary-directory collision. */
    for (attempt=0;attempt<64;attempt++)
    {
        snprintf(filename,sizeof(filename),".geditor-rebase-%08lx-%08lx-%lu",
            (unsigned long)GetCurrentProcessId(),(unsigned long)GetTickCount(),(unsigned long)attempt);
        if (!Join(staging,parent,filename,why)) { return FALSE; }
        if (CreateDirectory(staging,NULL)) { owned=TRUE; break; }
        if (GetLastError()!=ERROR_ALREADY_EXISTS) { return Fail(why,"Cannot create a temporary project in %s",parent); }
    }
    if (!owned) { return Fail(why,"Could not reserve a temporary project folder."); }
    basename=strrchr(source->geppath,'\\');
    if (!basename) { basename=strrchr(source->geppath,'/'); }
    basename=basename ? basename+1 : source->geppath;
    if (!CopyTree(source->dir,staging,basename,why)) { goto done; }
    snapshot=*source; lstrcpyn(snapshot.dir,staging,sizeof(snapshot.dir));
    /* Re-read compatibility from the copied snapshot, not a stale UI check. */
    plan=calloc(1,sizeof(*plan));
    if (!plan) { Fail(why,"Out of memory rebasing the project."); goto done; }
    if (!Prepare(plan,&snapshot,rompath,report,why)) { goto done; }
    for (i=0;i<plan->count;i++)
    {
        RebaseUpdate *update=&plan->updates[i];
        if (!Join(path,staging,update->path,why)
            || !WriteFileBytes(path,plan->newrom.data+update->offset,update->size,why)) { goto done; }
    }
    lstrcpyn(plan->project.name,name,sizeof(plan->project.name));
    snprintf(filename,sizeof(filename),"%s.gep",name);
    if (!Join(plan->project.geppath,staging,filename,why)
        || !ImageEditsRebase(staging,&plan->oldrom,&plan->newrom,TRUE,why)
        || !RomExportStoreProjectBase(&plan->project,&plan->newrom,why)
        || !ProjectSave(&plan->project,why)
        || !RomExportValidateProject(&plan->project,why)) { goto done; }
    /* Validate the final .gep path before publication as well. */
    *output=plan->project;
    lstrcpyn(output->dir,destination,sizeof(output->dir));
    if (!Join(output->geppath,destination,filename,why)) { goto done; }
    if (!MoveFileEx(staging,destination,MOVEFILE_WRITE_THROUGH))
    { Fail(why,"Could not publish the new project. The destination may already exist or be inaccessible."); goto done; }
    owned=FALSE; ok=TRUE; *why="";
done:
    FreePlan(plan);
    if (!ok) { ZeroMemory(output,sizeof(*output)); }
    if (owned && !RemoveTree(staging))
    {
        char previous[512]; lstrcpyn(previous,*why,sizeof(previous));
        Fail(why,"%.200s\nTemporary files could not all be removed: %.240s",previous,staging);
    }
    return ok;
}
