#include <stdio.h>
#include "studiooutput.h"
#include "project.h"
#include "editorpath.h"

static void StudioOutputU32(unsigned char *data,DWORD value)
{ for (int i=0;i<4;i++) { data[i]=(unsigned char)(value>>(i*8)); } }

static BOOL StudioOutputWrite(HANDLE file,const void *data,DWORD size)
{ DWORD written=0; return WriteFile(file,data,size,&written,NULL) && written==size; }

BOOL StudioOutputSave(const char *project,const TexPixel *pixels,const StudioRenderSettings *settings,
    char path[MAX_PATH],const char **why)
{
    char folder[MAX_PATH],temporary[MAX_PATH],leaf[32]; unsigned char header[122]={0},row[255*4];
    *why=""; path[0]=0;
    if (!pixels || !StudioRenderSettingsValid(settings))
    { *why="Render width and height must be whole numbers from 1 to 255."; return FALSE; }
    if (!ProjectEnsureStudioFolders(project,why)) { return FALSE; }
    if (!EditorPathJoin(folder,sizeof(folder),project,"studio\\output")
        || !EditorPathJoin(path,MAX_PATH,folder,"render0001.bmp"))
    { *why="The render output path is too long."; path[0]=0; return FALSE; }
    if (!GetTempFileName(folder,"rnd",0,temporary))
    { *why="Could not create a temporary render image."; path[0]=0; return FALSE; }
    HANDLE file=CreateFile(temporary,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    BOOL ok=file!=INVALID_HANDLE_VALUE;
    DWORD bytes=(DWORD)settings->width*settings->height*4;
    header[0]='B'; header[1]='M'; StudioOutputU32(header+2,sizeof(header)+bytes);
    StudioOutputU32(header+10,sizeof(header)); StudioOutputU32(header+14,108);
    StudioOutputU32(header+18,settings->width); StudioOutputU32(header+22,settings->height);
    header[26]=1; header[28]=32; StudioOutputU32(header+30,3); StudioOutputU32(header+34,bytes);
    StudioOutputU32(header+54,0x00ff0000); StudioOutputU32(header+58,0x0000ff00);
    StudioOutputU32(header+62,0x000000ff); StudioOutputU32(header+66,0xff000000);
    StudioOutputU32(header+70,0x73524742); /* LCS_sRGB. */
    if (ok) { ok=StudioOutputWrite(file,header,sizeof(header)); }
    for (int y=0;ok && y<settings->height;y++)
    {
        for (int x=0;x<settings->width;x++)
        {
            const TexPixel *p=&pixels[y*settings->width+x];
            row[4*x]=p->b; row[4*x+1]=p->g; row[4*x+2]=p->r; row[4*x+3]=p->a;
        }
        ok=StudioOutputWrite(file,row,(DWORD)settings->width*4);
    }
    if (file!=INVALID_HANDLE_VALUE)
    { if (ok && !FlushFileBuffers(file)) { ok=FALSE; } if (!CloseHandle(file)) { ok=FALSE; } }
    if (!ok) { *why="Could not write the render image. Check free space and folder permissions."; goto failed; }
    for (int number=1;number<=9999;number++)
    {
        snprintf(leaf,sizeof(leaf),"render%04d.bmp",number);
        if (!EditorPathJoin(path,MAX_PATH,folder,leaf)) { break; }
        if (GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES) { continue; }
        if (MoveFileEx(temporary,path,MOVEFILE_WRITE_THROUGH)) { return TRUE; }
        /* A competing render can claim this name after the existence check. */
        if (GetFileAttributes(path)!=INVALID_FILE_ATTRIBUTES) { continue; }
        *why="Could not publish the render image. Check folder permissions."; goto failed;
    }
    *why="All render filenames from render0001.bmp to render9999.bmp are in use.";
failed:
    DeleteFile(temporary); path[0]=0; return FALSE;
}
