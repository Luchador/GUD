#ifndef ROM_EXPORT_SETTINGS_TEST_WINDOWS_H
#define ROM_EXPORT_SETTINGS_TEST_WINDOWS_H
#include "../project_rebase/windows.h"
typedef void *HKEY;
typedef unsigned char BYTE;
#define HKEY_CURRENT_USER ((HKEY)1)
#define ERROR_SUCCESS 0
#define ERROR_MORE_DATA 234
#define REG_SZ 1
#define REG_OPTION_NON_VOLATILE 0
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define GetFullPathNameA GetFullPathName
LONG RegOpenKeyExA(HKEY,const char *,DWORD,DWORD,HKEY *);
LONG RegCreateKeyExA(HKEY,const char *,DWORD,char *,DWORD,DWORD,void *,HKEY *,DWORD *);
LONG RegQueryValueExA(HKEY,const char *,DWORD *,DWORD *,BYTE *,DWORD *);
LONG RegSetValueExA(HKEY,const char *,DWORD,DWORD,const BYTE *,DWORD);
LONG RegCloseKey(HKEY);
#endif
