#define _GNU_SOURCE
#undef fopen
#include "windows.h"
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <fcntl.h>
#include <limits.h>

/* File-API shim only. ROM parsing, compatibility, copy/publish orchestration,
 * model/image metadata validation and ROM export use production code. */
static DWORD error;
int test_fail_copy, test_fail_write, test_fail_move, test_publish_race;
typedef struct { FILE *file; char path[1024]; } File;
typedef struct { DIR *dir; char path[1024], pattern[MAX_PATH]; } Search;
static void Path(char *out,const char *in)
{ while (*in) { *out++=*in=='\\' ? '/' : *in; in++; } *out=0; }
static void Error(void)
{ error=errno==ENOENT ? ERROR_FILE_NOT_FOUND : errno==EEXIST ? ERROR_ALREADY_EXISTS : ERROR_ACCESS_DENIED; }
FILE *TestFopen(const char *path,const char *mode)
{ char native[1024]; Path(native,path); return fopen(native,mode); }
DWORD GetLastError(void) { return error; }
DWORD GetFileAttributes(const char *path)
{
    char p[1024]; struct stat st,link;
    Path(p,path);
    if (lstat(p,&link) || stat(p,&st)) { Error(); return INVALID_FILE_ATTRIBUTES; }
    return (S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL)
        | (S_ISLNK(link.st_mode) ? FILE_ATTRIBUTE_REPARSE_POINT : 0);
}
BOOL CreateDirectory(const char *path,void *unused)
{ char p[1024]; (void)unused; Path(p,path); if (!mkdir(p,0700)) { return TRUE; } Error(); return FALSE; }
BOOL DeleteFile(const char *path)
{ char p[1024]; Path(p,path); if (!unlink(p)) { return TRUE; } Error(); return FALSE; }
BOOL RemoveDirectory(const char *path)
{ char p[1024]; Path(p,path); if (!rmdir(p)) { return TRUE; } Error(); return FALSE; }
BOOL SetFileAttributes(const char *path,DWORD attrs)
{ char p[1024]; (void)attrs; Path(p,path); return !chmod(p,0600); }
BOOL CopyFile(const char *source,const char *dest,BOOL exclusive)
{
    char a[1024],b[1024],buffer[8192]; FILE *in,*out; size_t count; BOOL ok;
    if (test_fail_copy && --test_fail_copy==0) { error=ERROR_ACCESS_DENIED; return FALSE; }
    Path(a,source); Path(b,dest); in=fopen(a,"rb");
    if (!in) { Error(); return FALSE; }
    out=fopen(b,exclusive ? "wbx" : "wb");
    if (!out) { Error(); fclose(in); return FALSE; }
    ok=TRUE;
    while ((count=fread(buffer,1,sizeof(buffer),in))) if (fwrite(buffer,1,count,out)!=count) { ok=FALSE; break; }
    ok=ok && !ferror(in); fclose(in); if (fclose(out)) { ok=FALSE; }
    return ok;
}
BOOL MoveFileEx(const char *source,const char *dest,DWORD flags)
{
    char a[1024],b[1024]; struct stat st;
    Path(a,source); Path(b,dest);
    if (test_fail_move && --test_fail_move==0) { return FALSE; }
    if (test_publish_race && !(flags&MOVEFILE_REPLACE_EXISTING))
    { test_publish_race=0; mkdir(b,0700); error=ERROR_ALREADY_EXISTS; return FALSE; }
    if (!(flags&MOVEFILE_REPLACE_EXISTING) && !lstat(b,&st)) { error=ERROR_ALREADY_EXISTS; return FALSE; }
    if (!rename(a,b)) { return TRUE; } Error(); return FALSE;
}
HANDLE CreateFile(const char *path,DWORD mode,DWORD share,void *security,DWORD creation,DWORD attrs,HANDLE tmpl)
{
    File *f=calloc(1,sizeof(*f)); (void)share; (void)security; (void)creation; (void)tmpl;
    Path(f->path,path);
    f->file=fopen(f->path,(attrs&FILE_FLAG_BACKUP_SEMANTICS) || mode==GENERIC_READ ? "rb" : "wb");
    if (!f->file) { Error(); free(f); return INVALID_HANDLE_VALUE; } return f;
}
DWORD GetFileSize(HANDLE h,void *high)
{ File *f=h; long pos=ftell(f->file),end; fseek(f->file,0,SEEK_END); end=ftell(f->file); fseek(f->file,pos,SEEK_SET); if(high) { *(DWORD *)high=0; } return end; }
DWORD SetFilePointer(HANDLE h,LONG off,void *high,DWORD base)
{ File *f=h; (void)high; (void)base; return fseek(f->file,off,SEEK_SET) ? (DWORD)-1 : (DWORD)off; }
BOOL ReadFile(HANDLE h,void *data,DWORD size,DWORD *got,void *o)
{ File *f=h; (void)o; *got=fread(data,1,size,f->file); return !ferror(f->file); }
BOOL WriteFile(HANDLE h,const void *data,DWORD size,DWORD *got,void *o)
{ File *f=h; (void)o; if(test_fail_write && --test_fail_write==0) { *got=0; return FALSE; } *got=fwrite(data,1,size,f->file); return !ferror(f->file); }
BOOL CloseHandle(HANDLE h)
{ File *f=h; BOOL ok=!fclose(f->file); free(f); return ok; }
static DWORD Canonical(const char *path,char *out,DWORD max)
{
    char native[1024],resolved[PATH_MAX],*end; size_t size;
    Path(native,path);
    if (!realpath(native,resolved))
    {
        end=strrchr(native,'/'); if (!end) { return 0; } *end=0;
        if (!realpath(native,resolved)) { return 0; }
        strcat(resolved,"/"); strcat(resolved,end+1);
    }
    size=strlen(resolved); if (size>=max) { return size+1; }
    for (size_t i=0;i<=size;i++) { out[i]=resolved[i]=='/' ? '\\' : resolved[i]; }
    return size;
}
DWORD GetFinalPathNameByHandle(HANDLE h,char *out,DWORD max,DWORD flags)
{ File *f=h; (void)flags; return Canonical(f->path,out,max); }
DWORD GetFullPathName(const char *path,DWORD max,char *out,char **part)
{ (void)part; return Canonical(path,out,max); }
DWORD GetCurrentProcessId(void) { return getpid(); }
DWORD GetTickCount(void) { return 42; }
DWORD GetTempFileName(const char *dir,const char *prefix,unsigned int unique,char *out)
{
    char path[MAX_PATH]; int fd;
    (void)unique;
    if (snprintf(path,sizeof(path),"%s/%sXXXXXX",dir,prefix)>=(int)sizeof(path)) { return 0; }
    Path(out,path); fd=mkstemp(out);
    if (fd<0) { Error(); return 0; }
    close(fd); return 1;
}
static BOOL Next(Search *s,WIN32_FIND_DATA *result)
{
    struct dirent *entry;
    while ((entry=readdir(s->dir))) if (!fnmatch(s->pattern,entry->d_name,0))
    {
        char p[1400]; snprintf(p,sizeof(p),"%s/%s",s->path,entry->d_name);
        lstrcpyn(result->cFileName,entry->d_name,sizeof(result->cFileName));
        result->dwFileAttributes=GetFileAttributes(p); return TRUE;
    }
    error=ERROR_NO_MORE_FILES; return FALSE;
}
HANDLE FindFirstFile(const char *pattern,WIN32_FIND_DATA *result)
{
    Search *s=calloc(1,sizeof(*s)); char *end;
    Path(s->path,pattern); end=strrchr(s->path,'/');
    if (!end) { free(s); error=ERROR_PATH_NOT_FOUND; return INVALID_HANDLE_VALUE; }
    lstrcpyn(s->pattern,end+1,sizeof(s->pattern)); *end=0; s->dir=opendir(s->path);
    if (!s->dir) { Error(); free(s); return INVALID_HANDLE_VALUE; }
    if (!Next(s,result)) { closedir(s->dir); free(s); error=ERROR_FILE_NOT_FOUND; return INVALID_HANDLE_VALUE; }
    return s;
}
BOOL FindNextFile(HANDLE h,WIN32_FIND_DATA *result) { return Next(h,result); }
BOOL FindClose(HANDLE h) { Search *s=h; closedir(s->dir); free(s); return TRUE; }
int lstrcmpi(const char *a,const char *b) { return strcasecmp(a,b); }
char *lstrcpyn(char *a,const char *b,int max) { if (max>0) { strncpy(a,b,max-1); a[max-1]=0; } return a; }
