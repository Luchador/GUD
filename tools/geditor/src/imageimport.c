#include <windows.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "imageimport.h"
#include "imageedits.h"

typedef struct ImageImportDialog {
    const char *project;
    char path[MAX_PATH];
    TexPixel *pixels;
    int width,height;
    DWORD id;
    BOOL replacing;
    TexImportOptions options;
} ImageImportDialog;

static void Refresh(HWND dialog,ImageImportDialog *state)
{
    char text[160];int i;
    DWORD used,limit;
    for(i=0;i<13;i++) if(IsDlgButtonChecked(dialog,IDC_IMAGE_FORMAT_FIRST+i)==BST_CHECKED)
    { state->options.format=i;break; }
    state->options.mipmaps=(unsigned char)SendDlgItemMessage(dialog,IDC_IMAGE_MIPMAPS,CB_GETCURSEL,0,0);
    state->options.hitsound=(unsigned char)SendDlgItemMessage(dialog,IDC_IMAGE_SOUND,CB_GETCURSEL,0,0);
    state->options.hittexture=(unsigned char)SendDlgItemMessage(dialog,IDC_IMAGE_BULLET,CB_GETCURSEL,0,0);
    used=TexImportTmemBytes(state->width,state->height,state->options.format,state->options.mipmaps);
    limit=TexImportTmemLimit(state->options.format);
    snprintf(text,sizeof(text),"TMEM pixels + mipmaps: %lu / %lu bytes.%s",(unsigned long)used,(unsigned long)limit,
             state->options.format>=9 ? "\r\nPaletted textures reserve the other 2048 bytes for the palette." : "");
    SetDlgItemText(dialog,IDC_IMAGE_TMEM,text);
    SetDlgItemText(dialog,IDC_IMAGE_WARNING,used>limit
        ? "Too large for TMEM. Choose a smaller texture type, reduce mipmaps, or resize the BMP before importing." : "");
    EnableWindow(GetDlgItem(dialog,IDC_IMAGE_IMPORT),used!=0 && used<=limit);
}
static INT_PTR CALLBACK DialogProc(HWND dialog,UINT message,WPARAM wparam,LPARAM lparam)
{
    ImageImportDialog *state=(ImageImportDialog *)GetWindowLongPtr(dialog,DWLP_USER);
    if(message==WM_INITDIALOG)
    {
        char text[MAX_PATH+80];int i,maximum;
        state=(ImageImportDialog *)lparam;SetWindowLongPtr(dialog,DWLP_USER,(LONG_PTR)state);
        snprintf(text,sizeof(text),"%s\r\n%d x %d pixels   -   %s image %04lX",state->path,state->width,state->height,state->replacing ? "Replace" : "New",(unsigned long)state->id);
        SetDlgItemText(dialog,IDC_IMAGE_SUMMARY,text);
        if(state->replacing)
        {
            SetWindowText(dialog,"Replace Image");
            SetDlgItemText(dialog,IDC_IMAGE_IMPORT,"Replace");
        }
        for(i=0;i<13;i++)
        {
            SetDlgItemText(dialog,IDC_IMAGE_FORMAT_FIRST+i,TexInfoFormatName(i));
            SendDlgItemMessage(dialog,IDC_IMAGE_SOUND,CB_ADDSTRING,0,(LPARAM)TexInfoSurfaceName(i));
            SendDlgItemMessage(dialog,IDC_IMAGE_BULLET,CB_ADDSTRING,0,(LPARAM)TexInfoSurfaceName(i));
        }
        CheckRadioButton(dialog,IDC_IMAGE_FORMAT_FIRST,IDC_IMAGE_FORMAT_LAST,IDC_IMAGE_FORMAT_FIRST+1);
        SendDlgItemMessage(dialog,IDC_IMAGE_MIPMAPS,CB_ADDSTRING,0,(LPARAM)"None (base image only)");
        maximum=TexImportMaxMipmaps(state->width,state->height);
        for(i=1;i<=maximum;i++)
        {
            snprintf(text,sizeof(text),"Generate %d mipmap%s (%d total levels)",i,i==1 ? "" : "s",i+1);
            SendDlgItemMessage(dialog,IDC_IMAGE_MIPMAPS,CB_ADDSTRING,0,(LPARAM)text);
        }
        SendDlgItemMessage(dialog,IDC_IMAGE_MIPMAPS,CB_SETCURSEL,0,0);
        SendDlgItemMessage(dialog,IDC_IMAGE_SOUND,CB_SETCURSEL,1,0);
        SendDlgItemMessage(dialog,IDC_IMAGE_BULLET,CB_SETCURSEL,1,0);
        Refresh(dialog,state);return TRUE;
    }
    if(!state) { return FALSE; }
    if(message==WM_COMMAND)
    {
        int id=LOWORD(wparam);
        if(id==IDCANCEL) { EndDialog(dialog,0);return TRUE; }
        if(id==IDC_IMAGE_IMPORT)
        {
            const char *why="";
            Refresh(dialog,state);
            if(!IsWindowEnabled(GetDlgItem(dialog,IDC_IMAGE_IMPORT))) { return TRUE; }
            SetCursor(LoadCursor(NULL,IDC_WAIT));
            if(state->replacing
                ? ImageEditsReplace(state->project,state->id,state->pixels,state->width,state->height,&state->options,state->path,&why)
                : ImageEditsImport(state->project,state->pixels,state->width,state->height,&state->options,state->path,&state->id,&why))
            { SetCursor(LoadCursor(NULL,IDC_ARROW));EndDialog(dialog,1); }
            else { SetCursor(LoadCursor(NULL,IDC_ARROW));MessageBox(dialog,why,state->replacing ? "Replace Image" : "Import Image",MB_OK|MB_ICONERROR); }
            return TRUE;
        }
        if((id>=IDC_IMAGE_FORMAT_FIRST && id<=IDC_IMAGE_FORMAT_LAST && HIWORD(wparam)==BN_CLICKED)
            || HIWORD(wparam)==CBN_SELCHANGE) { Refresh(dialog,state);return TRUE; }
    }
    if(message==WM_CTLCOLORSTATIC && (HWND)lparam==GetDlgItem(dialog,IDC_IMAGE_WARNING))
    { SetTextColor((HDC)wparam,RGB(180,0,0));SetBkMode((HDC)wparam,TRANSPARENT);return (INT_PTR)GetSysColorBrush(COLOR_3DFACE); }
    if(message==WM_CLOSE) { EndDialog(dialog,0);return TRUE; }
    return FALSE;
}
static BOOL Show(HWND owner,const char *projectdir,DWORD *id,BOOL replacing)
{
    ImageImportDialog state={0};OPENFILENAME ofn={0};const char *why="";INT_PTR result;
    const char *title=replacing ? "Replace Image" : "Import Image";
    state.project=projectdir;state.replacing=replacing;
    if(replacing) { state.id=*id; }
    if(!(replacing ? ImageEditsCanEdit(projectdir,state.id,&why) : ImageEditsNextId(projectdir,&state.id,&why)))
    { MessageBox(owner,why,title,MB_ICONERROR);return FALSE; }
    ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=owner;ofn.lpstrTitle=title;
    ofn.lpstrFile=state.path;ofn.nMaxFile=sizeof(state.path);
    ofn.lpstrFilter="Bitmap images (*.bmp)\0*.bmp\0";ofn.nFilterIndex=1;
    ofn.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR;
    if(!GetOpenFileName(&ofn)) { return FALSE; }
    if(!TexReadImportBmp(state.path,&state.pixels,&state.width,&state.height,&why))
    { MessageBox(owner,why,title,MB_ICONERROR);return FALSE; }
    result=DialogBoxParam((HINSTANCE)GetWindowLongPtr(owner,GWLP_HINSTANCE),MAKEINTRESOURCE(IDD_IMPORT_IMAGE),owner,DialogProc,(LPARAM)&state);
    free(state.pixels);
    if(result==-1) { MessageBox(owner,"The image settings dialog could not be opened.",title,MB_ICONERROR); }
    if(result==1) { *id=state.id;return TRUE; }
    return FALSE;
}

BOOL ImageImportShow(HWND owner,const char *projectdir,DWORD *id)
{ return Show(owner,projectdir,id,FALSE); }
BOOL ImageReplaceShow(HWND owner,const char *projectdir,DWORD id)
{ return Show(owner,projectdir,&id,TRUE); }

BOOL ImageReimportShow(HWND owner,const char *projectdir,DWORD id)
{
    char source[MAX_PATH],error[MAX_PATH+640];const char *why="";BOOL ok;
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    ok=ImageEditsReimport(projectdir,id,source,&why);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
    if(!ok)
    {
        snprintf(error,sizeof(error),"Image %04lX%s%s\r\n\r\n%s\r\n\r\nThe current image has not been changed.",
            (unsigned long)id,source[0] ? "\r\n" : "",source,why);
        MessageBox(owner,error,"Reimport Image",MB_OK|MB_ICONERROR);
    }
    return ok;
}
