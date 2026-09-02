#define COBJMACROS
#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>   /* IFileOpenDialog: the modern folder picker */
#include <shlobj.h>
#include <windowsx.h>  /* GET_X_LPARAM */     /* SHGetFolderPath: default to Documents */
#include <string.h>
#include <stdio.h>

#include "project.h"
#include "resource.h"
#include "viewport.h"
#include "browser.h"
#include "rom.h"

#define GEDITOR_CLASS  "GEditorWindow"
#define GEDITOR_TITLE  "GEditor"
#define GEDITOR_WIDTH  1920
#define GEDITOR_HEIGHT  1080

static HWND g_Viewport;
static HWND g_Browser;

/* Content browser column: user-draggable via the splitter gutter. */
#define GEDITOR_SPLITTER_W    5
#define GEDITOR_BROWSER_MIN 160
#define GEDITOR_VIEWPORT_MIN 320
static int  g_BrowserWidth = 280;
static BOOL g_DraggingSplitter = FALSE;
static GEditorProject g_Project;

static void GEditorSetTitleForProject(HWND hwnd);


/**
  * The one place a project gets closed, however the user asks for it:
  * the Close Project menu item, or implicitly when creating or opening
  * another project.
  */
static void GEditorCloseProject(HWND hwnd)
{
    if (g_Project.name[0] == '\0')
    {
        return; /* Nothing open. */
    }

    ProjectClose(&g_Project);
    GEditorSetTitleForProject(hwnd);
}


/**
  * Menu command IDs. Every clickable has one and it is the number
  * that arrives in WM_COMMAND when the item is chosen.
  */
enum {
    ID_FILE_NEW_PROJECT = 40001,
    ID_FILE_OPEN_PROJECT,
    ID_FILE_CLOSE_PROJECT,
    ID_FILE_EXIT,

    ID_EDIT_UNDO,
    ID_EDIT_REDO
};


/* What the New Project dialog collects. */
typedef struct NewProjectInfo {
    char name[GEDITOR_NAME_MAX];
    char location[MAX_PATH];
    char rompath[MAX_PATH];
} NewProjectInfo;


/*
 * Builds the top menu bar.
 */
static HMENU GEditorCreateMenuBar(void)
{
    HMENU menubar;
    HMENU filemenu;
    HMENU editmenu;

    menubar = CreateMenu();
    filemenu = CreatePopupMenu();
    editmenu = CreatePopupMenu();

    /* MF_STRING items carry a command ID. '&' marks the Alt mnemonic. */
    AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_PROJECT, "&New Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_PROJECT, "&Open Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_CLOSE_PROJECT, "&Close Project");
    AppendMenu(filemenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(filemenu, MF_STRING, ID_FILE_EXIT, "E&xit");

    AppendMenu(editmenu, MF_STRING, ID_EDIT_UNDO, "&Undo");
    AppendMenu(editmenu, MF_STRING, ID_EDIT_REDO, "&Redo");

    AppendMenu(menubar, MF_POPUP, (UINT_PTR)filemenu, "&File");
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)editmenu, "&Edit");

    return menubar;
}


/*
 * Returns TRUE if name is usable as a project name. The rules are the
 * ones Windows imposes on file names, because the name becomes part of
 * the .gep file name. There must be text and the text can't use reserved characters.
 */
static BOOL GEditorNameIsValid(const char *name, const char **reasonout)
{
    static const char reserved[] = "\\/:*?\"<>|";
    const char *p;

    for (p = name; *p == ' ' || *p == '\t'; p++)
    {
        /* skip leading blanks */
    }

    if (*p == '\0')
    {
        *reasonout = "Enter a project name.";
        return FALSE;
    }

    for (p = name; *p != '\0'; p++)
    {
        if (strchr(reserved, *p) != NULL)
        {
            *reasonout = "A name cannot contain \\ / : * ? \" < > or |";
            return FALSE;
        }
    }

    *reasonout = "";
    return TRUE;
}


/* TRUE if path names an existing directory. */
static BOOL GEditorDirectoryExists(const char *path)
{
    DWORD attrs = GetFileAttributes(path);

    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}


/*
 * Folder picker. Fills pathout and returns TRUE if a folder was chosen.
 */
static BOOL GEditorPromptForFolder(HWND owner, char *pathout, int pathmax)
{
    IFileOpenDialog *dlg = NULL;
    IShellItem *item = NULL;
    PWSTR wpath = NULL;
    DWORD opts = 0;
    BOOL ok = FALSE;

    if (FAILED(CoCreateInstance(&CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IFileOpenDialog, (void **)&dlg)))
    {
        return FALSE;
    }

    if (SUCCEEDED(IFileOpenDialog_GetOptions(dlg, &opts)))
    {
        /* FORCEFILESYSTEM refuses virtual folders (Libraries, This PC)
           that have no path we could write files into. */
        IFileOpenDialog_SetOptions(dlg, opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    }
    IFileOpenDialog_SetTitle(dlg, L"Choose Project Location");

    if (SUCCEEDED(IFileOpenDialog_Show(dlg, owner))
        && SUCCEEDED(IFileOpenDialog_GetResult(dlg, &item)))
    {
        /* The shell speaks UTF-16; the rest of GEditor is ANSI. */
        if (SUCCEEDED(IShellItem_GetDisplayName(item, SIGDN_FILESYSPATH, &wpath)))
        {
            ok = WideCharToMultiByte(CP_ACP, 0, wpath, -1, pathout, pathmax, NULL, NULL) > 0;
            CoTaskMemFree(wpath);
        }
        IShellItem_Release(item);
    }

    IFileOpenDialog_Release(dlg);
    return ok;
}


/*
 * Ask the user for a GUD ROM.
 */
static BOOL GEditorPromptForRom(HWND hwnd, char *pathout, DWORD pathmax)
{
    OPENFILENAME ofn;

    /* The dialog uses this buffer as the initial contents of the File
       name box, so it must be a valid (here, empty) string. */
    pathout[0] = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;            /* modal to our window */
    ofn.lpstrFile   = pathout;
    ofn.nMaxFile    = pathmax;
    ofn.lpstrTitle  = "Open Project";
    ofn.lpstrDefExt = "z64";           /* appended if the user types no extension */

    /* Filter: pairs of "description\0pattern\0", terminated by an extra
       \0. The literal below already ends in one implicit terminator, so
       the explicit \0 at the end supplies the required double null. */
    ofn.lpstrFilter = "N64 ROMs (*.z64)\0*.z64\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              /* 1-based: start on the .gep filter */

    ofn.Flags = OFN_EXPLORER          /* Modern Explorer-style dialog. */
              | OFN_FILEMUSTEXIST     /* Refuse names that do not exist. */
              | OFN_PATHMUSTEXIST
              | OFN_HIDEREADONLY      /* Hide the vestigial read-only box. */
              | OFN_NOCHANGEDIR;      /* Do not move our working directory. */

    return GetOpenFileName(&ofn);
}


/*
 * Re-checks the edit box and updates the two things that depend on it:
 * whether Create Project is clickable, and what the warning line says.
 * Called once when the dialog opens and again on every keystroke, so
 * the button is never enabled for a name we would reject.
 */
static void GEditorUpdateNameValidity(HWND hdlg)
{
    char name[GEDITOR_NAME_MAX];
    char location[MAX_PATH];
    const char *reason;
    BOOL valid;

    GetDlgItemText(hdlg, IDC_PROJECT_NAME, name, sizeof(name));
    valid = GEditorNameIsValid(name, &reason);

    /* The name check reports first; the location check only runs on
       a good name, so the warning line shows one problem at a time. */
    if (valid)
    {
        GetDlgItemText(hdlg, IDC_PROJECT_LOCATION, location, sizeof(location));

        if (location[0] == '\0')
        {
            reason = "Choose a location for the project.";
            valid = FALSE;
        }
        else if (!GEditorDirectoryExists(location))
        {
            reason = "That location does not exist.";
            valid = FALSE;
        }
    }

    if (valid)
    {
        char rompath[MAX_PATH];
        DWORD attrs;

        GetDlgItemText(hdlg, IDC_PROJECT_ROM, rompath, sizeof(rompath));
        attrs = GetFileAttributes(rompath);

        if (rompath[0] == '\0')
        {
            reason = "Choose the GUD ROM to extract assets from.";
            valid = FALSE;
        }
        else if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY))
        {
            reason = "That ROM file does not exist.";
            valid = FALSE;
        }
    }

    EnableWindow(GetDlgItem(hdlg, IDC_CREATE_PROJECT), valid);
    SetDlgItemText(hdlg, IDC_NAME_WARNING, reason);
}


static INT_PTR CALLBACK GEditorNewProjectProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
    NewProjectInfo *info;

    switch (msg)
    {
    case WM_INITDIALOG:
        /* Remember where to write the chosen name. */
        SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)lparam);

        SendDlgItemMessage(hdlg, IDC_PROJECT_NAME, EM_LIMITTEXT, GEDITOR_NAME_MAX - 1, 0);
        SetDlgItemText(hdlg, IDC_PROJECT_NAME, "GE Project");

        /* Default the location to the user's Documents folder. */
        {
            char docs[MAX_PATH];

            SendDlgItemMessage(hdlg, IDC_PROJECT_LOCATION, EM_LIMITTEXT, MAX_PATH - 1, 0);
            if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_MYDOCUMENTS, NULL, 0, docs)))
            {
                SetDlgItemText(hdlg, IDC_PROJECT_LOCATION, docs);
            }
        }

        /* Select the default so typing replaces it. */
        SendDlgItemMessage(hdlg, IDC_PROJECT_NAME, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hdlg, IDC_PROJECT_NAME));

        GEditorUpdateNameValidity(hdlg);
        return FALSE; /* FALSE because we set the focus ourselves */

    case WM_CTLCOLORSTATIC:
        /* Paint the warning line red. lparam is the control's HWND. */
        if (GetDlgCtrlID((HWND)lparam) == IDC_NAME_WARNING)
        {
            HDC hdc = (HDC)wparam;
            SetTextColor(hdc, RGB(192, 0, 0));
            SetBkMode(hdc, TRANSPARENT);
            return (INT_PTR)GetSysColorBrush(COLOR_3DFACE);
        }
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
        case IDC_PROJECT_NAME:
        case IDC_PROJECT_LOCATION:
            /* EN_CHANGE arrives on every edit, so validation is live
               for both boxes - including a path pasted into Location. */
            if (HIWORD(wparam) == EN_CHANGE)
            {
                GEditorUpdateNameValidity(hdlg);
            }
            return TRUE;

        case IDC_BROWSE_LOCATION:
        {
            char folder[MAX_PATH];

            if (GEditorPromptForFolder(hdlg, folder, sizeof(folder)))
            {
                /* Setting the text fires EN_CHANGE, which revalidates. */
                SetDlgItemText(hdlg, IDC_PROJECT_LOCATION, folder);
            }
            return TRUE;
        }

        case IDC_BROWSE_ROM:
        {
            char folder[MAX_PATH];

            if (GEditorPromptForRom(hdlg, folder, sizeof(folder)))
            {
                /* Setting the text fires EN_CHANGE, which revalidates. */
                SetDlgItemText(hdlg, IDC_PROJECT_ROM, folder);
            }
            return TRUE;
        }

        case IDC_CREATE_PROJECT:
        {
            char rompath[MAX_PATH];
            RomInfo rominfo;
            const char *reason;

            /* The expensive checks run once, here - reading the whole
               ROM and walking the manifest. A failure reports on the
               warning line and keeps the dialog open for correction. */
            GetDlgItemText(hdlg, IDC_PROJECT_ROM, rompath, sizeof(rompath));
            if (!RomValidate(rompath, &rominfo, &reason))
            {
                SetDlgItemText(hdlg, IDC_NAME_WARNING, reason);
                return TRUE;
            }
        }
            /* fall through to the original create path */
            info = (NewProjectInfo *)GetWindowLongPtr(hdlg, DWLP_USER);
            GetDlgItemText(hdlg, IDC_PROJECT_NAME, info->name, sizeof(info->name));
            GetDlgItemText(hdlg, IDC_PROJECT_LOCATION, info->location, sizeof(info->location));
            GetDlgItemText(hdlg, IDC_PROJECT_ROM, info->rompath, sizeof(info->rompath));
            EndDialog(hdlg, IDOK);
            return TRUE;

        case IDCANCEL:
            /* Escape key and the title bar's close button. */
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
        }
        break;
    }

    return FALSE;
}


/*
 * Shows the New Project dialog. Fills nameout (GEDITOR_NAME_MAX chars)
 * and returns TRUE if the user created a project.
 */
static BOOL GEditorPromptForNewProject(HWND hwnd, NewProjectInfo *info)
{
    INT_PTR result;

    ZeroMemory(info, sizeof(*info));

    result = DialogBoxParam(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(IDD_NEW_PROJECT),
                            hwnd,
                            GEditorNewProjectProc,
                            (LPARAM)info);

    if (result == -1)
    {
        MessageBox(hwnd, "Dialog resource missing (build problem).",
                   GEDITOR_TITLE, MB_ICONERROR);
    }

    return (result == IDOK);
}


/*
 * Ask the user for a .gep project file.
 */
static BOOL GEditorPromptForProject(HWND hwnd, char *pathout, DWORD pathmax)
{
    OPENFILENAME ofn;

    /* The dialog uses this buffer as the initial contents of the File
       name box, so it must be a valid (here, empty) string. */
    pathout[0] = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;            /* modal to our window */
    ofn.lpstrFile   = pathout;
    ofn.nMaxFile    = pathmax;
    ofn.lpstrTitle  = "Open Project";
    ofn.lpstrDefExt = "gep";           /* appended if the user types no extension */

    /* Filter: pairs of "description\0pattern\0", terminated by an extra
       \0. The literal below already ends in one implicit terminator, so
       the explicit \0 at the end supplies the required double null. */
    ofn.lpstrFilter = "GEditor Projects (*.gep)\0*.gep\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              /* 1-based: start on the .gep filter */

    ofn.Flags = OFN_EXPLORER          /* Modern Explorer-style dialog. */
              | OFN_FILEMUSTEXIST     /* Refuse names that do not exist. */
              | OFN_PATHMUSTEXIST
              | OFN_HIDEREADONLY      /* Hide the vestigial read-only box. */
              | OFN_NOCHANGEDIR;      /* Do not move our working directory. */

    return GetOpenFileName(&ofn);
}


static void GEditorSetTitleForProject(HWND hwnd)
{
    char title[GEDITOR_NAME_MAX + 32];
    
    if (g_Project.name[0] != '\0')
    {
        snprintf(title, sizeof(title), "%s - %s", GEDITOR_TITLE, g_Project.name);
    }
    else
    {
        snprintf(title, sizeof(title), "%s", GEDITOR_TITLE);
    }

    SetWindowText(hwnd, title);
}


static void GEditorLayout(HWND hwnd)
{
    RECT rc;

    GetClientRect(hwnd, &rc);

    /* Clamp the split so neither pane can be dragged out of existence,
       and so a narrow window still shows something of both. */
    if (g_BrowserWidth > rc.right - GEDITOR_SPLITTER_W - GEDITOR_VIEWPORT_MIN)
    {
        g_BrowserWidth = rc.right - GEDITOR_SPLITTER_W - GEDITOR_VIEWPORT_MIN;
    }
    if (g_BrowserWidth < GEDITOR_BROWSER_MIN)
    {
        g_BrowserWidth = GEDITOR_BROWSER_MIN;
    }

    if (g_Browser != NULL)
    {
        MoveWindow(g_Browser, 0, 0, g_BrowserWidth, rc.bottom, TRUE);
    }

    if (g_Viewport != NULL)
    {
        MoveWindow(g_Viewport,
                   g_BrowserWidth + GEDITOR_SPLITTER_W, 0,
                   rc.right - g_BrowserWidth - GEDITOR_SPLITTER_W, rc.bottom,
                   TRUE);
    }

    /* The strip between them is bare main-window client area - the
       splitter gutter the mouse handlers below watch for. */
}

/* TRUE when x (main-window client coords) is inside the gutter. */
static BOOL GEditorInSplitter(int x)
{
    return x >= g_BrowserWidth && x < g_BrowserWidth + GEDITOR_SPLITTER_W;
}


static LRESULT CALLBACK GEditorWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;

        g_Browser = BrowserCreate(hwnd, cs->hInstance);
        if (g_Browser == NULL)
        {
            return -1;
        }

        g_Viewport = ViewportCreate(hwnd, cs->hInstance);
        if (g_Viewport == NULL)
        {
            return -1;
        }
        return 0;
    }

    case WM_SIZE:
        GEditorLayout(hwnd);
        return 0;

    case WM_SETCURSOR:
        /* Mouse messages over the children go to the children, so the
           main window only hears about the cursor when it is over its
           own client area - which is exactly the splitter gutter. */
        if (LOWORD(lparam) == HTCLIENT)
        {
            POINT p;

            GetCursorPos(&p);
            ScreenToClient(hwnd, &p);
            if (GEditorInSplitter(p.x))
            {
                SetCursor(LoadCursor(NULL, IDC_SIZEWE));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
        if (GEditorInSplitter(GET_X_LPARAM(lparam)))
        {
            g_DraggingSplitter = TRUE;
            SetCapture(hwnd);
        }
        return 0;

    case WM_MOUSEMOVE:
        if (g_DraggingSplitter)
        {
            g_BrowserWidth = GET_X_LPARAM(lparam) - GEDITOR_SPLITTER_W / 2;
            GEditorLayout(hwnd); /* clamps, then repositions both panes */
        }
        return 0;

    case WM_LBUTTONUP:
        if (g_DraggingSplitter)
        {
            g_DraggingSplitter = FALSE;
            ReleaseCapture();
        }
        return 0;

    case WM_CAPTURECHANGED:
        /* Capture stolen (Alt+Tab, a dialog): abandon the drag the same
           way the viewport abandons flight. */
        g_DraggingSplitter = FALSE;
        return 0;

    case WM_INITMENUPOPUP:
        /* Sent just before a drop-down opens - the one moment the item
           states matter, so they can never be stale. Close Project is
           only clickable while a project is open. */
        EnableMenuItem((HMENU)wparam, ID_FILE_CLOSE_PROJECT, MF_BYCOMMAND | (g_Project.name[0] != '\0' ? MF_ENABLED : MF_GRAYED));
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wparam))
        {
            /**
             * Create a new project.
             */
            case ID_FILE_NEW_PROJECT:
            {
                NewProjectInfo info;

                if (GEditorPromptForNewProject(hwnd, &info))
                {
                    GEditorCloseProject(hwnd); /* one project at a time */

                    if (ProjectCreate(info.name, info.location, &g_Project))
                    {
                        GEditorSetTitleForProject(hwnd);
                    }
                    else
                    {
                        MessageBox(hwnd, "Could not create the project.", GEDITOR_TITLE, MB_ICONERROR);
                    }
                }
                return 0;
            }

            case ID_FILE_OPEN_PROJECT:
            {
                char path[MAX_PATH];
 
                if (GEditorPromptForProject(hwnd, path, sizeof(path)))
                {
                    GEditorCloseProject(hwnd); /* one project at a time */

                    if (ProjectRead(path, &g_Project))
                    {
                        GEditorSetTitleForProject(hwnd);
                    }
                    else
                    {
                        MessageBox(hwnd, "That file is not a readable GEditor project.", GEDITOR_TITLE, MB_ICONERROR);
                    }
                }
                return 0;
            }

            case ID_FILE_CLOSE_PROJECT:
                GEditorCloseProject(hwnd);
                return 0;

            case ID_FILE_EXIT:
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
            }

        break; /* anything else falls through to DefWindowProc */

    case WM_DESTROY:
        /* The window is gone; ask the message loop to stop. Without
           this the process keeps running after the window closes. */
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE hprev, LPSTR cmdline, int showcmd)
{
    WNDCLASS wc;
    HWND hwnd;
    MSG msg;
    HMENU menubar;

    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = GEditorWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(hinstance, MAKEINTRESOURCE(IDI_GEDITOR));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); /* Paints the blank client area. */
    wc.lpszClassName = GEDITOR_CLASS;

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, "RegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!BrowserRegisterClass(hinstance))
    {
        MessageBox(NULL, "BrowserRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    if (!ViewportRegisterClass(hinstance))
    {
        MessageBox(NULL, "ViewportRegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    menubar = GEditorCreateMenuBar();

    hwnd = CreateWindowEx(0, GEDITOR_CLASS, GEDITOR_TITLE, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, GEDITOR_WIDTH, GEDITOR_HEIGHT, NULL, menubar, hinstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "CreateWindowEx failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showcmd);
    UpdateWindow(hwnd);

    /**
     * The message loop. GetMessage blocks until something happens, returns 0 when WM_QUIT arrives, and -1 on error. 
     */
    for (;;)
    {
        if (g_Viewport != NULL && ViewportIsFlying(g_Viewport))
        {
            /* Flying: drain the queue, then render one frame ourselves.
               WM_PAINT and WM_TIMER are the lowest-priority messages
               Windows has, and a fast mouse floods the queue faster
               than they surface - the cause of the skipping camera.
               With vsync on, SwapBuffers inside ViewportFlyFrame
               blocks until the monitor is ready, pacing this loop to
               the refresh rate. */
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT)
                {
                    CoUninitialize();
                    return (int)msg.wParam;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            ViewportFlyFrame(g_Viewport);
        }
        else
        {
            /* Idle: block. The editor uses no CPU, exactly as before. */
            if (GetMessage(&msg, NULL, 0, 0) <= 0)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    CoUninitialize();
    return (int)msg.wParam;
}