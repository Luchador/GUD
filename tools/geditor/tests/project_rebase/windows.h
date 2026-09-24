#ifndef REBASE_TEST_WINDOWS_H
#define REBASE_TEST_WINDOWS_H
#include "../image_import/windows.h"
#include <strings.h>
#define FILE_SHARE_WRITE 2
#define FILE_SHARE_DELETE 4
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000u
#define FILE_ATTRIBUTE_REPARSE_POINT 0x400u
#define FILE_NAME_NORMALIZED 0
#define _strnicmp strncasecmp
BOOL CopyFile(const char *, const char *, BOOL);
BOOL SetFileAttributes(const char *, DWORD);
BOOL RemoveDirectory(const char *);
DWORD GetFinalPathNameByHandle(HANDLE, char *, DWORD, DWORD);
DWORD GetFullPathName(const char *, DWORD, char *, char **);
DWORD GetCurrentProcessId(void);
DWORD GetTickCount(void);
DWORD GetTempFileName(const char *, const char *, unsigned int, char *);
extern int test_fail_copy, test_fail_write, test_publish_race;
#endif
