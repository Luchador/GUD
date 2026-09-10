#undef fopen
#include "windows.h"
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <errno.h>
#include <stdlib.h>
#include <strings.h>
static DWORD error;
int test_fail_move;
static void Path(char *dst,const char *src) { while(*src) { *dst++=*src=='\\' ? '/' : *src;src++; } *dst=0; }
FILE *TestFopen(const char *path,const char *mode) { char p[1024];Path(p,path);return fopen(p,mode); }
DWORD GetLastError(void) { return error; }
DWORD GetFileAttributes(const char *path) { char p[1024];struct stat st;Path(p,path);if(stat(p,&st)) { error=errno==ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED;return INVALID_FILE_ATTRIBUTES; }return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL; }
BOOL CreateDirectory(const char *path,void *unused) { char p[1024];(void)unused;Path(p,path);if(!mkdir(p,0700)) return TRUE;error=errno==EEXIST ? ERROR_ALREADY_EXISTS : ERROR_ACCESS_DENIED;return FALSE; }
BOOL DeleteFile(const char *path) { char p[1024];Path(p,path);return unlink(p)==0; }
BOOL MoveFileEx(const char *src,const char *dst,DWORD flags) { char a[1024],b[1024];(void)flags;Path(a,src);Path(b,dst);if(test_fail_move && --test_fail_move==0) {return FALSE;}return rename(a,b)==0; }
HANDLE CreateFile(const char *p,DWORD mode,DWORD share,void *security,DWORD creation,DWORD attrs,HANDLE tmpl) { FILE *f;(void)share;(void)security;(void)creation;(void)attrs;(void)tmpl;f=TestFopen(p,mode==GENERIC_READ ? "rb" : "wb");if(!f) { error=errno==ENOENT ? ERROR_FILE_NOT_FOUND : ERROR_ACCESS_DENIED; } return f ? f : INVALID_HANDLE_VALUE; }
DWORD GetFileSize(HANDLE h,void *high) { long pos=ftell(h),end;(void)high;fseek(h,0,SEEK_END);end=ftell(h);fseek(h,pos,SEEK_SET);return end; }
DWORD SetFilePointer(HANDLE h,LONG off,void *high,DWORD base) {(void)high;(void)base;return fseek(h,off,SEEK_SET) ? (DWORD)-1 : (DWORD)off;}
BOOL ReadFile(HANDLE h,void *p,DWORD size,DWORD *got,void *o) {(void)o;*got=fread(p,1,size,h);return !ferror(h);}
BOOL WriteFile(HANDLE h,const void *p,DWORD size,DWORD *got,void *o) {(void)o;*got=fwrite(p,1,size,h);return !ferror(h);}
BOOL CloseHandle(HANDLE h) {return fclose(h)==0;}
typedef struct {glob_t g;size_t index;} FindState;
static void Found(FindState *s,WIN32_FIND_DATA *data) { const char *p=s->g.gl_pathv[s->index],*base=strrchr(p,'/');data->dwFileAttributes=GetFileAttributes(p);lstrcpyn(data->cFileName,base ? base+1 : p,MAX_PATH); }
HANDLE FindFirstFile(const char *pattern,WIN32_FIND_DATA *data) {char p[1024];FindState *s=calloc(1,sizeof(*s));Path(p,pattern);if(glob(p,0,NULL,&s->g)) {globfree(&s->g);free(s);error=ERROR_FILE_NOT_FOUND;return INVALID_HANDLE_VALUE;}Found(s,data);return s;}
BOOL FindNextFile(HANDLE h,WIN32_FIND_DATA *data) {FindState *s=h;if(++s->index>=s->g.gl_pathc) {error=ERROR_NO_MORE_FILES;return FALSE;}Found(s,data);return TRUE;}
BOOL FindClose(HANDLE h) {FindState *s=h;globfree(&s->g);free(s);return TRUE;}
int lstrcmpi(const char *a,const char *b) {return strcasecmp(a,b);}
char *lstrcpyn(char *a,const char *b,int n) {if(n>0){strncpy(a,b,n-1);a[n-1]=0;}return a;}
