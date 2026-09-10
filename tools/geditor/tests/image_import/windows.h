#ifndef TEST_WINDOWS_H
#define TEST_WINDOWS_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD; typedef int32_t LONG; typedef uint16_t WORD; typedef uint64_t ULONGLONG;
typedef int BOOL; typedef void *HANDLE; typedef void *HINSTANCE;
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define INVALID_FILE_SIZE ((DWORD)-1)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define GENERIC_READ 0x80000000u
#define GENERIC_WRITE 0x40000000u
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define CREATE_ALWAYS 2
#define FILE_BEGIN 0
#define BI_RGB 0
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ACCESS_DENIED 5
#define ERROR_NO_MORE_FILES 18
#define ERROR_ALREADY_EXISTS 183
#define MOVEFILE_REPLACE_EXISTING 1
#define MOVEFILE_WRITE_THROUGH 8
#define ZeroMemory(p,n) memset(p,0,n)
#define wsprintf sprintf
#pragma pack(push,1)
typedef struct { WORD bfType; DWORD bfSize; WORD bfReserved1,bfReserved2; DWORD bfOffBits; } BITMAPFILEHEADER;
#pragma pack(pop)
typedef struct { DWORD biSize; LONG biWidth,biHeight; WORD biPlanes,biBitCount; DWORD biCompression,biSizeImage; LONG biXPelsPerMeter,biYPelsPerMeter; DWORD biClrUsed,biClrImportant; } BITMAPINFOHEADER;
typedef struct { DWORD dwFileAttributes; char cFileName[MAX_PATH]; } WIN32_FIND_DATA;
DWORD GetFileAttributes(const char *); DWORD GetLastError(void);
BOOL CreateDirectory(const char *,void *); BOOL DeleteFile(const char *); BOOL MoveFileEx(const char *,const char *,DWORD);
HANDLE CreateFile(const char *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);
DWORD GetFileSize(HANDLE,void *); DWORD SetFilePointer(HANDLE,LONG,void *,DWORD);
BOOL ReadFile(HANDLE,void *,DWORD,DWORD *,void *); BOOL WriteFile(HANDLE,const void *,DWORD,DWORD *,void *); BOOL CloseHandle(HANDLE);
HANDLE FindFirstFile(const char *,WIN32_FIND_DATA *); BOOL FindNextFile(HANDLE,WIN32_FIND_DATA *); BOOL FindClose(HANDLE);
int lstrcmpi(const char *,const char *); char *lstrcpyn(char *,const char *,int);
FILE *TestFopen(const char *,const char *);
extern int test_fail_move;
#endif
