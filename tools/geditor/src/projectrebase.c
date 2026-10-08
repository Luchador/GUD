/* Rebase saved edits onto a compatible GUD ROM. ROM addresses and file-table
 * indices may move; resource names and native model IDs remain stable. Image banks may gain
 * or lose an appended suffix; differing image IDs use an explicit choice of
 * project or incoming image data and complete native settings.
 * Saved setup files always stay with the project. Other level resources and
 * editable fields use a three-way merge; binary conflicts are never byte merged. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include "projectrebase.h"
#include "textbank.h"
#include "romexport.h"
#include "texrom.h"
#include "imageedits.h"
#include "newprops.h"
#include "modeledits.h"
#include "modelcompile.h"
#include "editorpath.h"

#define REBASE_MAX_FILES 1024u
typedef struct RebaseUpdate {
    char path[MAX_PATH];
    DWORD offset, size;
    BOOL remove;
    int model; /* 0 = level resource; 1 = incoming model; 2 = keep native edit. */
} RebaseUpdate;
typedef struct RebaseFile {
    const char *name;
    DWORD offset, size;
} RebaseFile;
typedef struct RebasePlan {
    RomFile oldrom, newrom;
    GEditorProject project;
    RebaseUpdate updates[REBASE_MAX_FILES];
    RebaseFile oldfiles[REBASE_MAX_FILES], newfiles[REBASE_MAX_FILES];
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
static void Note(ProjectRebaseReport *report, const char *name, const char *detail)
{
    size_t used=strlen(report->details);
    if (used<sizeof(report->details)-256)
        snprintf(report->details+used,sizeof(report->details)-used,"%s: %s\r\n",name,detail);
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
    return EditorPathJoin(path, MAX_PATH, dir, name)
        || Fail(why, "A project path is too long: %s", name);
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
static BOOL CatalogRows(const RomFile *rom, const RomManifestEntry *entry,
    DWORD stride, const unsigned char **rows, DWORD *count, const char **why)
{
    if (!entry || entry->flags != 0x80000000u || entry->romstart > rom->size
        || rom->size-entry->romstart < 16 || entry->romend != entry->romstart+16)
    { return Fail(why,"The ROM has an invalid model catalog descriptor."); }
    const unsigned char *data=rom->data+entry->romstart;
    *count=Read32(data+4);
    if (Read32(data+8)!=stride || Read32(data+12)!=1 || *count>rom->size/stride)
    { return Fail(why,"The ROM has an unsupported model catalog format or count."); }
    *rows=Mapped(rom,Read32(data),*count*stride);
    return *rows!=NULL || Fail(why,"The model catalog lies outside the ROM's mapped data.");
}
static BOOL CatalogUnused(const unsigned char *row, const char *name, DWORD stride)
{
    if (!row) { return TRUE; }
    /* Expanded character arrays include the former {NULL,NULL,1.0f,...}
     * terminator followed by zero-initialized capacity. Neither is a model.
     * Model-less ITEM rows can still define usable inventory entries: only
     * an entirely zero row is unused there, including its stats pointer. */
    if (stride==56)
    {
        for (DWORD i=0;i<stride;i++) if (row[i]) { return FALSE; }
        return TRUE;
    }
    if (Read32(row) || name[0] || (Read32(row+8)!=0 && Read32(row+8)!=0x3f800000u)) { return FALSE; }
    for (DWORD i=12;i<stride;i++) if (row[i]) { return FALSE; }
    return TRUE;
}
static BOOL Catalogs(const RomFile *a, const RomFile *b, const char **why)
{
    static const struct { DWORD tag, stride; const char *name; } catalogs[] = {
        {0x4348524du,20,"Character"}, {0x50524f50u,12,"Prop"}, {0x4954454du,56,"Item"}
    };
    unsigned int kind;
    for (kind = 0; kind < sizeof(catalogs)/sizeof(catalogs[0]); kind++)
    {
        const RomManifestEntry *ae = Entry(a,catalogs[kind].tag), *be = Entry(b,catalogs[kind].tag);
        const unsigned char *arows, *brows;
        DWORD acount, bcount, i, stride = catalogs[kind].stride;
        if (!ae && !be) { continue; } /* Older v3 ROMs may omit discovery tables. */
        if (!ae || !be)
        { return Fail(why, "The ROMs have incompatible model catalogs. Use matching GUD/editor formats."); }
        if (!CatalogRows(a,ae,stride,&arows,&acount,why)
            || !CatalogRows(b,be,stride,&brows,&bcount,why)) { return FALSE; }
        /* Slot counts are capacity, not identity. Validate the entire incoming
         * table, but require compatibility only for occupied old IDs. */
        for (i=0;i<acount || i<bcount;i++)
        {
            const unsigned char *ar=i<acount ? arows+i*stride : NULL;
            const unsigned char *br=i<bcount ? brows+i*stride : NULL;
            DWORD an=ar ? Read32(ar+4) : 0, bn=br ? Read32(br+4) : 0;
            const char *as=an ? MappedString(a,an) : "", *bs=bn ? MappedString(b,bn) : "";
            if (!as || !bs)
            { return Fail(why,"%s model catalog ID %lu has an invalid resource name.",
                catalogs[kind].name,(unsigned long)i); }
            if (CatalogUnused(ar,as,stride)) { continue; }
            /* Header/stat pointers may relocate with optimized code. Compare
             * their presence, model names and scalars, not linked addresses. */
            if (!br || CatalogUnused(br,bs,stride) || !!Read32(ar)!=!!Read32(br)
                || !!an!=!!bn || strcmp(as,bs) || memcmp(ar+8,br+8,4)
                || (stride==20 && memcmp(ar+12,br+12,8))
                || (stride==56 && (!!Read32(ar+12)!=!!Read32(br+12) || memcmp(ar+16,br+16,40))))
            { return Fail(why, "%s model catalog ID %lu changed or was removed (%s); model IDs or settings need migration.",
                catalogs[kind].name,(unsigned long)i,as[0] ? as : "model-less entry"); }
        }
    }
    return TRUE;
}
static BOOL ImageBank(const RomFile *a, RomFile *b, ProjectRebaseChoice choice,
    ProjectRebaseReport *report, const char **why)
{
    TexRomBank oldbank, newbank;
    DWORD i, oldat, newat, common, count;
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
        {
            size_t used=strlen(report->details);
            if (choice==PROJECT_REBASE_STOP)
            { return Fail(why,"Image %04lX changed in the base ROM. Choose an Image conflicts resolution.",(unsigned long)i); }
            BOOL keep=choice==PROJECT_REBASE_KEEP_PROJECT;
            if (keep) { report->imagespreserved++; } else { report->imagesupdated++; }
            report->resolved++;
            if (used<sizeof(report->details)-128)
                snprintf(report->details+used,sizeof(report->details)-used,
                    "Image %04lX: %s texture and image settings.\r\n",(unsigned long)i,
                    keep ? "keeping the project's base" : "using the new ROM's");
        }
        oldat+=size;
        /* A retained replacement may have different dimensions/format. */
        newat+=Read32(b->data+newbank.table+i*8)&0xffffffu;
    }
    if (oldbank.count<=newbank.count) report->imagesadded=newbank.count-oldbank.count;
    if (oldbank.count<=newbank.count && !report->imagespreserved) { return TexRomCompactImages(b,why); }
    if (oldbank.count>newbank.capacity)
    { return Fail(why,"The new ROM has insufficient image capacity to retain the project's base images."); }
    /* Retain an absent suffix to preserve referenced image IDs. Shared slots
     * use the chosen base, including complete native mipmaps/detail flags. */
    count=oldbank.count>newbank.count ? oldbank.count : newbank.count;
    records=calloc(count,sizeof(*records)); sizes=calloc(count,sizeof(*sizes));
    surfaces=calloc(count,1);
    if (!records || !sizes || !surfaces)
    { free(records); free(sizes); free(surfaces); return Fail(why,"Out of memory retaining the project's base images."); }
    oldat=oldbank.images;
    for (i=0;i<oldbank.count;i++)
    {
        DWORD size=Read32(a->data+oldbank.table+i*8)&0xffffffu;
        if (i>=common || choice==PROJECT_REBASE_KEEP_PROJECT)
        { records[i]=a->data+oldat; sizes[i]=size; }
        /* A legacy original can use flags unavailable to fresh imports.
         * Supply neutral import flags here and restore its full entry below. */
        oldat+=size;
    }
    ok=TexRomUpdateImages(b,&newbank,records,sizes,surfaces,count,why);
    if (ok)
    {
        /* These are originals, not fresh imports. Preserve all detail flags
         * too; TexRomUpdateImages normally clears them on replacement. */
        for (i=0;i<oldbank.count;i++) if (records[i])
            memcpy(b->data+newbank.table+i*8,a->data+oldbank.table+i*8,8);
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
/* Migration identities from older GUD builds. Only these unused placeholders
 * may disappear automatically; losing a playable/custom stage is an error. */
static const struct { LONG id; const char *stem; } g_RetiredLevels[] = {
    {42,"sho"}, {44,"eld"}, {47,"lue"}, {49,"rit"}, {51,"ear"},
    {52,"lee"}, {53,"lip"}, {55,"wax"}, {56,"pam"}
};
static BOOL RetiredStage(const RomLevel *level)
{
    unsigned int i;
    char name[64];
    for (i=0;i<sizeof(g_RetiredLevels)/sizeof(g_RetiredLevels[0]);i++)
    {
        const char *stem=g_RetiredLevels[i].stem;
        if (level->levelID!=g_RetiredLevels[i].id) { continue; }
        snprintf(name,sizeof(name),"Usetup%sZ",stem);
        if (strcmp(name,level->setupname)) { return FALSE; }
        snprintf(name,sizeof(name),"bg/bg_%s_all_p.seg",stem);
        if (strcmp(name,level->bgname)) { return FALSE; }
        snprintf(name,sizeof(name),"Tbg_%s_all_p_stanZ",stem);
        return !strcmp(name,level->stanname);
    }
    return FALSE;
}
static BOOL RetiredResource(const RomInfo *incoming, const char *resource)
{
    static const char *formats[]={"Usetup%sZ","bg/bg_%s_all_p.seg","Tbg_%s_all_p_stanZ","L%sE"};
    unsigned int i,j;
    char name[64];
    for (i=0;i<sizeof(g_RetiredLevels)/sizeof(g_RetiredLevels[0]);i++)
    {
        if (Level(incoming,g_RetiredLevels[i].id)) { continue; }
        for (j=0;j<sizeof(formats)/sizeof(formats[0]);j++)
        {
            snprintf(name,sizeof(name),formats[j],g_RetiredLevels[i].stem);
            if (!strcmp(resource,name)) { return TRUE; }
        }
    }
    return FALSE;
}
static BOOL Levels(RebasePlan *plan, const GEditorProject *source,
    ProjectRebaseReport *report, const char **why)
{
    DWORD i;
    plan->project = *source;
    if (!source->levelcount || source->levelcount != plan->oldrom.info.levelcount
        || plan->newrom.info.levelcount > ROM_MAX_LEVELS)
    { return Fail(why, "The project's level table does not match its base ROM."); }
    for (i=0;i<plan->newrom.info.levelcount;i++)
    {
        if (!Level(&plan->newrom.info,plan->newrom.info.levels[i].levelID))
        { return Fail(why,"The new ROM contains duplicate level IDs."); }
    }
    plan->project.levelcount=0;
    for (i=0;i<source->levelcount;i++)
    {
        RomLevel *p=&plan->project.levels[plan->project.levelcount];
        *p=source->levels[i];
        const RomLevel *a=Level(&plan->oldrom.info,p->levelID), *b=Level(&plan->newrom.info,p->levelID);
        DWORD j;
        for (j=0;j<i;j++) if (source->levels[j].levelID==p->levelID)
        { return Fail(why,"The project contains duplicate level IDs."); }
        if (!a || !LevelNames(a,p))
        { return Fail(why, "Level %ld has missing, duplicate or changed resource names.", (long)p->levelID); }
        if (!b && RetiredStage(a))
        {
            const char *names[]={a->bgname,a->setupname,a->stanname};
            if (p->levelscale!=a->levelscale || p->renderScale!=a->renderScale || p->chrLODDistance!=a->chrLODDistance
                || p->music!=a->music || p->bgsound!=a->bgsound || p->xtrack!=a->xtrack)
            { Conflict(report,p->name,"removed from the new ROM but has edited level settings"); }
            /* Some placeholders never had a setup/stan resource. A local
             * file without an old base cannot safely be classified as unused. */
            for (j=0;j<sizeof(names)/sizeof(names[0]);j++)
            {
                char path[MAX_PATH];
                DWORD offset,size;
                const char *unused;
                if (!RomFindFile(&plan->oldrom,names[j],&offset,&size,&unused)
                    && RomExportProjectResourcePath(source,names[j],path,sizeof(path))==1)
                {
                    DWORD attrs=GetFileAttributes(path), error=GetLastError();
                    if (attrs!=INVALID_FILE_ATTRIBUTES)
                    { Conflict(report,names[j],"removed level has a saved asset with no base to compare"); }
                    else if (error!=ERROR_FILE_NOT_FOUND && error!=ERROR_PATH_NOT_FOUND)
                    { return Fail(why,"Cannot inspect project asset: %s",path); }
                }
            }
            report->levelsremoved++;
            continue;
        }
        if (!b || !LevelNames(a,b))
        { return Fail(why, "Level %ld has missing, duplicate or changed resource names.", (long)p->levelID); }
        /* Coordinate scale affects BG/setup/stan as a group. Do not merge
         * it independently and silently change the meaning of saved positions. */
        if (!isfinite(p->levelscale) || p->levelscale <= 0 || a->levelscale != b->levelscale)
        { Conflict(report,p->name,"world coordinate scale changed; geometry migration required"); }
#define MERGE_FIELD(field) \
        if (p->field == a->field) { p->field=b->field; } \
        else if (b->field != a->field && p->field != b->field) { Conflict(report,p->name,#field " changed in both project and ROM"); }
        MERGE_FIELD(renderScale)
        MERGE_FIELD(chrLODDistance)
        MERGE_FIELD(music)
        MERGE_FIELD(bgsound)
        MERGE_FIELD(xtrack)
#undef MERGE_FIELD
        lstrcpyn(p->name,b->name,sizeof(p->name));
        lstrcpyn(p->world,b->world,sizeof(p->world));
        p->hasbackgroundcolor=b->hasbackgroundcolor;
        memcpy(p->backgroundcolor,b->backgroundcolor,sizeof(p->backgroundcolor)); p->fog=b->fog; p->clouds=b->clouds;
        p->water=b->water; p->skybody=b->skybody; p->skygradient=b->skygradient;
        plan->project.levelcount++;
    }
    /* New named rows (MP variants and Title) retain existing stage IDs.
     * Resources() separately verifies the file catalog by resource name. */
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
    const char *environmentwhy = "";
    BOOL available = EnvironmentReadRom(&plan->newrom, &plan->project.environments, NULL, &environmentwhy);
    if (source->environmentOverrides.count)
    {
        EnvironmentTable oldbase;
        if (!available || !EnvironmentReadRom(&plan->oldrom, &oldbase, NULL, &environmentwhy)
            || !EnvironmentRebase(&oldbase, &plan->project.environments, &plan->project.environmentOverrides, &environmentwhy))
        { Conflict(report, "Environment", environmentwhy); }
    }
    EnvironmentRefreshLevels(&plan->project.environments, &plan->project.environmentOverrides,
        plan->project.levels, plan->project.levelcount);
    const char *memorywhy = "";
    available = LevelMemoryReadRom(&plan->newrom, &plan->project.memory, &memorywhy);
    if (source->memoryOverrides.count)
    {
        LevelMemoryTable oldbase;
        if (!available || !LevelMemoryReadRom(&plan->oldrom, &oldbase, &memorywhy)
            || !LevelMemoryRebase(&oldbase, &plan->project.memory, &plan->project.memoryOverrides, &memorywhy))
        { Conflict(report, "Level memory", memorywhy); }
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
static BOOL EqualResource(const char *name, const unsigned char *a, DWORD asize, const unsigned char *b, DWORD bsize)
{
    return Equal(a,asize,b,bsize) || (TextBankIsResource(name) && TextBankEqual(a,asize,b,bsize));
}
/* ROM slots may include up to 15 alignment bytes beyond a saved model. */
static BOOL EqualModel(const unsigned char *a, DWORD asize, const unsigned char *b, DWORD bsize)
{
    DWORD common=asize<bsize ? asize : bsize, extra=asize>bsize ? asize-bsize : bsize-asize;
    const unsigned char *tail=asize>bsize ? a+common : b+common;
    if (extra>15 || memcmp(a,b,common)) { return FALSE; }
    for (DWORD i=0;i<extra;i++) if (tail[i]) { return FALSE; }
    return TRUE;
}
static const char *ModelFolder(const char *name)
{
    size_t length=strlen(name);
    if (length<2 || length>=64 || name[length-1]!='Z') { return NULL; }
    for (size_t i=0;i<length;i++) if (!isalnum((unsigned char)name[i]) && name[i]!='_') { return NULL; }
    if (name[0]=='C') { return "characters"; }
    if (name[0]=='P') { return "objects"; }
    if (name[0]!='G') { return NULL; }
    return !strcmp(name,"GcartblueZ") || !strcmp(name,"GcartridgeZ")
        || !strcmp(name,"GcartrifleZ") || !strcmp(name,"GcartshellZ") ? "casings" : "guns";
}
static BOOL ModelResource(RebasePlan *plan, const GEditorProject *source,
    const RebaseFile *a, const RebaseFile *b, ProjectRebaseChoice choice,
    ProjectRebaseReport *report, const char **why)
{
    unsigned char *local=NULL;
    DWORD size=0;
    ModelSource incoming={0};
    const unsigned char *old=plan->oldrom.data+a->offset, *next=plan->newrom.data+b->offset;
    int present=ModelEditsReadReplacement(source->dir,a->name,old,a->size,&local,&size,why);
    if (present<0) { return FALSE; }
    if (!ModelReadSource(next,b->size,&incoming,why)) { free(local); return FALSE; }
    ModelFreeSource(&incoming);
    BOOL edited=present && !EqualModel(local,size,old,a->size);
    BOOL matches=present && EqualModel(local,size,next,b->size);
    BOOL conflict=edited && !matches && !EqualModel(old,a->size,next,b->size);
    free(local);
    if (conflict && choice==PROJECT_REBASE_STOP)
    { Conflict(report,a->name,"model changed in both project and ROM; choose a Model conflicts resolution"); return TRUE; }
    /* A saved override remains intentional even after its geometry has been
     * baked into base.z64. Keep project must not drop it merely because the
     * old base now matches it, then silently adopt an older incoming model. */
    BOOL keep=present && (choice==PROJECT_REBASE_KEEP_PROJECT || matches || (edited && !conflict));
    RebaseUpdate *update=&plan->updates[plan->count++];
    lstrcpyn(update->path,a->name,sizeof(update->path));
    update->model=keep ? 2 : 1; update->offset=b->offset; update->size=b->size;
    if (conflict) { report->resolved++; }
    if (keep)
    {
        report->modelskept++;
        Note(report,a->name,matches ? "project model already matches the new ROM; retaining editor materials"
            : "keeping the project model and rebinding it to the new base");
    }
    else
    {
        report->modelsupdated++;
        Note(report,a->name,conflict ? "using the new ROM model instead of the project edit"
            : "updating from the new ROM; no competing project model edit");
    }
    return TRUE;
}
static const RebaseFile *File(const RebaseFile *files, DWORD count, const char *name)
{
    DWORD i;
    for (i=0;i<count;i++) if (!strcmp(files[i].name,name)) { return &files[i]; }
    return NULL;
}
static BOOL Files(const RomFile *rom, RebaseFile *files, DWORD *count, const char **why)
{
    const RomManifestEntry *table=Entry(rom,0x4654424cu);
    const RomManifestEntry *map=Entry(rom,0x434d4150u);
    DWORD i,j;
    if (table && map && table->romstart>=map->romstart) for (i=0;i<REBASE_MAX_FILES;i++)
    {
        DWORD at=table->romstart+i*12;
        char name[64];
        if (at>map->romend || map->romend-at<12) { break; }
        if (!Read32(rom->data+at+4)) { *count=i; return TRUE; }
        files[i].name=MappedString(rom,Read32(rom->data+at+4));
        /* File IDs are runtime table indices, distinct from persistent stage,
         * model and image IDs. Validate each catalog before matching names. */
        if (Read32(rom->data+at)!=i || !files[i].name || strlen(files[i].name)>=sizeof(name))
        { return Fail(why,"ROM resource ID %lu has an invalid index or name.",(unsigned long)i); }
        for (j=0;j<i;j++) if (!lstrcmpi(files[i].name,files[j].name))
        { return Fail(why,"Duplicate ROM resource name: %s",files[i].name); }
        if (!files[i].name[0]) { continue; }
        if (!RomGetFileByIndex(rom,i,name,sizeof(name),&files[i].offset,&files[i].size))
        { return Fail(why,"Invalid ROM resource: %s",files[i].name); }
    }
    return Fail(why,"The ROM file table has no valid terminator.");
}
static BOOL Resources(RebasePlan *plan, const GEditorProject *source,
    const ProjectRebaseOptions *options, ProjectRebaseReport *report, const char **why)
{
    DWORD count, newcount, i;
    if (!Files(&plan->oldrom,plan->oldfiles,&count,why)
        || !Files(&plan->newrom,plan->newfiles,&newcount,why)) { return FALSE; }
    for (i=0;i<newcount;i++)
    {
        if (!File(plan->oldfiles,count,plan->newfiles[i].name))
        { return Fail(why,"The new ROM introduced resource %s; this rebase needs asset migration.",plan->newfiles[i].name); }
    }
    for (i=0;i<count;i++)
    {
        const RebaseFile *a=&plan->oldfiles[i], *b=File(plan->newfiles,newcount,a->name);
        const char *name=a->name;
        char path[MAX_PATH];
        DWORD size,attrs;
        unsigned char *data;
        int managed;
        BOOL changed;
        if (!name[0]) { continue; }
        if (!b)
        {
            DWORD j;
            if (!RetiredResource(&plan->newrom.info,name))
            { return Fail(why,"ROM resource %s is missing; only unused level resources can be retired automatically.",name); }
            for (j=0;j<plan->newrom.info.levelcount;j++)
            {
                const RomLevel *level=&plan->newrom.info.levels[j];
                if (!strcmp(level->bgname,name) || !strcmp(level->setupname,name) || !strcmp(level->stanname,name))
                { return Fail(why,"Removed resource %s is still used by %s.",name,level->name); }
            }
            report->resourcesremoved++;
        }
        report->checked++;
        changed=b && !EqualResource(name,plan->oldrom.data+a->offset,a->size,plan->newrom.data+b->offset,b->size);
        if (!b && TextBankIsResource(name))
        {
            DWORD j;
            /* Retired language banks were empty. Customized contents may
             * still be referenced by saved setup string IDs. */
            for (j=0;j<a->size && !plan->oldrom.data[a->offset+j];j++) {}
            if (j!=a->size) { Conflict(report,name,"removed text bank contains data; string migration required"); }
        }
        managed=RomExportProjectResourcePath(source,name,path,sizeof(path));
        if (managed<0) { return Fail(why,"Invalid project resource path: %s",name); }
        if (!managed)
        {
            if (changed && ModelFolder(name))
            {
                if (!ModelResource(plan,source,a,b,options->modelConflicts,report,why)) { return FALSE; }
            }
            else if (changed) { Conflict(report,name,"unsupported base resource changed; asset migration required"); }
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
        if (!b)
        {
            if (!EqualResource(name,data,size,plan->oldrom.data+a->offset,a->size))
            { Conflict(report,name,"removed from the new ROM but has saved project edits"); }
            else
            {
                RebaseUpdate *update=&plan->updates[plan->count++];
                lstrcpyn(update->path,path+strlen(source->dir)+1,sizeof(update->path));
                update->remove=TRUE;
            }
        }
        else if (!strncmp(name,"Usetup",6) || !strncmp(name,"Ump_setup",9))
        {
            /* A saved setup is the project's authoritative level state, even
             * when it was already baked into the old base ROM. The snapshot
             * contains the complete .set, including editor action metadata.
             * Never queue an incoming-ROM overwrite, regardless of the other
             * level-file conflict choice. Removed resources still use the
             * compatibility checks above; export validation still runs. */
            report->kept++; report->setupskept++;
            if (changed && !EqualResource(name,data,size,plan->oldrom.data+a->offset,a->size)
                && !EqualResource(name,data,size,plan->newrom.data+b->offset,b->size))
            { report->resolved++; }
            Note(report,name,"keeping the saved project setup (project always wins)");
        }
        else if (changed && !EqualResource(name,data,size,plan->oldrom.data+a->offset,a->size)
            && !EqualResource(name,data,size,plan->newrom.data+b->offset,b->size))
        {
            if (options->levelConflicts==PROJECT_REBASE_STOP)
            { Conflict(report,name,"changed differently in the project and the new ROM; choose a BG/stan/text conflicts resolution"); }
            else
            {
                report->resolved++;
                if (options->levelConflicts==PROJECT_REBASE_KEEP_PROJECT)
                { report->kept++; Note(report,name,"keeping the project file instead of the new ROM version"); }
                else
                {
                    RebaseUpdate *update=&plan->updates[plan->count++];
                    lstrcpyn(update->path,path+strlen(source->dir)+1,sizeof(update->path));
                    update->offset=b->offset; update->size=b->size; report->updated++;
                    Note(report,name,"using the new ROM file instead of the project edit");
                }
            }
        }
        else if (changed)
        {
            RebaseUpdate *update=&plan->updates[plan->count++];
            lstrcpyn(update->path,path+strlen(source->dir)+1,sizeof(update->path));
            update->offset=b->offset; update->size=b->size; report->updated++;
        }
        else if (!EqualResource(name,data,size,plan->oldrom.data+a->offset,a->size)) { report->kept++; }
        free(data);
    }
    return TRUE;
}
static BOOL Prepare(RebasePlan *plan, const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, ProjectRebaseReport *report, const char **why)
{
    char base[MAX_PATH];
    ZeroMemory(report,sizeof(*report)); *why="";
    if (!source || !source->dir[0] || source->levelcount>ROM_MAX_LEVELS) { return Fail(why,"Open a valid saved project first."); }
    if (!options || options->imageConflicts<PROJECT_REBASE_STOP || options->imageConflicts>PROJECT_REBASE_USE_ROM
        || options->levelConflicts<PROJECT_REBASE_STOP || options->levelConflicts>PROJECT_REBASE_USE_ROM
        || options->modelConflicts<PROJECT_REBASE_STOP || options->modelConflicts>PROJECT_REBASE_USE_ROM)
    { return Fail(why,"Choose valid rebase conflict resolutions."); }
    if (!Join(base,source->dir,ROM_EXPORT_BASE_FILENAME,why)
        || !RomLoad(base,&plan->oldrom,why) || !RomLoad(rompath,&plan->newrom,why)
        || !ImageBank(&plan->oldrom,&plan->newrom,options->imageConflicts,report,why) || !Catalogs(&plan->oldrom,&plan->newrom,why)
        || !NewPropsCheckRebase(source->dir,&plan->newrom,why)
        || !Levels(plan,source,report,why) || !Resources(plan,source,options,report,why)) { return FALSE; }
    if (report->conflicts) { return Fail(why,"Rebase blocked by %lu conflict(s). See the report.",(unsigned long)report->conflicts); }
    /* Validate against the original base before new stock IDs can turn an
     * orphan BMP or a gap in imported IDs into an apparently valid asset. */
    return RomExportValidateRebaseSource(source,&plan->newrom,why)
        && ImageEditsRebase(source->dir,&plan->oldrom,&plan->newrom,options->imageConflicts,report,FALSE,why);
}
static void FreePlan(RebasePlan *plan)
{ if (plan) { RomFree(&plan->oldrom); RomFree(&plan->newrom); free(plan); } }
BOOL ProjectRebaseCheck(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, ProjectRebaseReport *report, const char **why)
{
    ProjectRebaseOptions options={keepBaseImages ? PROJECT_REBASE_KEEP_PROJECT : PROJECT_REBASE_STOP,
        PROJECT_REBASE_STOP,PROJECT_REBASE_STOP};
    return ProjectRebaseCheckWithOptions(source,rompath,&options,report,why);
}
BOOL ProjectRebaseCheckWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, ProjectRebaseReport *report, const char **why)
{
    RebasePlan *plan=calloc(1,sizeof(*plan));
    BOOL ok;
    ZeroMemory(report,sizeof(*report));
    if (!plan) { return Fail(why,"Out of memory checking the project."); }
    ok=Prepare(plan,source,rompath,options,report,why);
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
static BOOL EnsureDirectory(const char *path, const char **why)
{
    DWORD attrs=GetFileAttributes(path);
    if (attrs!=INVALID_FILE_ATTRIBUTES)
        return (attrs & FILE_ATTRIBUTE_DIRECTORY) || Fail(why,"Expected a directory: %s",path);
    return CreateDirectory(path,NULL) || Fail(why,"Cannot create model directory: %s",path);
}
static BOOL ApplyModel(const RebaseUpdate *update, const char *project,
    const RomFile *rom, const char **why)
{
    char path[MAX_PATH],relative[128],models[MAX_PATH],folder[MAX_PATH];
    const unsigned char *incoming=rom->data+update->offset;
    snprintf(relative,sizeof(relative),"models\\native\\%s.gmodel",update->path);
    if (!Join(path,project,relative,why)) { return FALSE; }
    if (update->model==2)
    {
        unsigned char *data=NULL; DWORD size;
        if (!ReadFileBytes(path,&data,&size,why)) { return FALSE; }
        /* Prepare validated the full native payload against the old base.
         * Keep its content hash, material slots, UVs and export identity. */
        if (size<16 || memcmp(data,"GMD1",4)) { free(data); return Fail(why,"Invalid model edit: %s",path); }
        DWORD hash=ModelDataHash(incoming,update->size);
        for (DWORD k=0;k<4;k++) { data[4+k]=(unsigned char)(hash>>(24-k*8)); }
        BOOL ok=WriteFileBytes(path,data,size,why); free(data); return ok;
    }
    DWORD attrs=GetFileAttributes(path);
    if (attrs!=INVALID_FILE_ATTRIBUTES)
    {
        if (!DeleteFile(path)) { return Fail(why,"Cannot replace project model: %s",path); }
    }
    else if (GetLastError()!=ERROR_FILE_NOT_FOUND && GetLastError()!=ERROR_PATH_NOT_FOUND)
    { return Fail(why,"Cannot inspect model edit: %s",path); }
    if (!Join(models,project,"models",why) || !EnsureDirectory(models,why)
        || !Join(folder,models,ModelFolder(update->path),why) || !EnsureDirectory(folder,why)) { return FALSE; }
    snprintf(relative,sizeof(relative),"%s.gltf",update->path);
    if (!Join(path,folder,relative,why)) { return FALSE; }
    ModelSource model={0};
    BOOL ok=ModelReadSource(incoming,update->size,&model,why)
        && ModelMaterialsEnsure(&model,project,why);
    model.closestpreview=update->path[0]=='C';
    if (ok) { ok=GltfWriteEditableModel(path,project,&model,ModelDataHash(incoming,update->size),why); }
    ModelFreeSource(&model); return ok;
}
BOOL ProjectRebaseCreate(const GEditorProject *source, const char *rompath,
    BOOL keepBaseImages, const char *parent, const char *name, GEditorProject *output,
    ProjectRebaseReport *report, const char **why)
{
    ProjectRebaseOptions options={keepBaseImages ? PROJECT_REBASE_KEEP_PROJECT : PROJECT_REBASE_STOP,
        PROJECT_REBASE_STOP,PROJECT_REBASE_STOP};
    return ProjectRebaseCreateWithOptions(source,rompath,&options,parent,name,output,report,why);
}
BOOL ProjectRebaseCreateWithOptions(const GEditorProject *source, const char *rompath,
    const ProjectRebaseOptions *options, const char *parent, const char *name, GEditorProject *output,
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
    if (!Prepare(plan,&snapshot,rompath,options,report,why)) { goto done; }
    for (i=0;i<plan->count;i++)
    {
        RebaseUpdate *update=&plan->updates[i];
        if (update->model) { continue; }
        if (!Join(path,staging,update->path,why)) { goto done; }
        if (update->remove)
        {
            if (!DeleteFile(path)) { Fail(why,"Cannot remove retired project asset: %s",path); goto done; }
        }
        else if (!WriteFileBytes(path,plan->newrom.data+update->offset,update->size,why)) { goto done; }
    }
    lstrcpyn(plan->project.name,name,sizeof(plan->project.name));
    snprintf(filename,sizeof(filename),"%s.gep",name);
    if (!Join(plan->project.geppath,staging,filename,why)
        || !ImageEditsRebase(staging,&plan->oldrom,&plan->newrom,options->imageConflicts,NULL,TRUE,why)
        || !RomExportStoreProjectBase(&plan->project,&plan->newrom,why)
        || !ProjectSave(&plan->project,why)) { goto done; }
    /* Model previews must use the adopted base images and image edits. */
    for (i=0;i<plan->count;i++)
        if (plan->updates[i].model && !ApplyModel(&plan->updates[i],staging,&plan->newrom,why)) { goto done; }
    if (!RomExportValidateProject(&plan->project,why)) { goto done; }
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
