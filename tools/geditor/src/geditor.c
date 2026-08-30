/*
 * GEditor - GoldenEye level editor for GUD.
 *
 * Step 2: the shell plus a menu bar. One resizable window with the
 * standard system buttons, and a File menu whose single Exit item
 * routes through the same shutdown path as the close button.
 */

#include <windows.h>
#include <commdlg.h>
#include <string.h>

#include "resource.h"

#define GEDITOR_CLASS  "GEditorWindow"
#define GEDITOR_TITLE  "GEditor"
#define GEDITOR_WIDTH  1024
#define GEDITOR_HEIGHT  720

/*
 * Menu command IDs. Every clickable item needs one; it is the number
 * that arrives in WM_COMMAND when the item is chosen. Start high -
 * low values are reserved for standard controls (IDOK is 1).
 */
enum {
    ID_FILE_NEW_PROJECT = 40001,
    ID_FILE_OPEN_PROJECT,
    ID_FILE_EXIT
};

/*
 * Builds the menu bar. The menu is a tree: CreateMenu makes the
 * horizontal bar, CreatePopupMenu makes each drop-down, and
 * AppendMenu hangs items and popups onto them.
 */
static HMENU GEditorCreateMenuBar(void)
{
    HMENU menubar;
    HMENU filemenu;

    menubar = CreateMenu();
    filemenu = CreatePopupMenu();

    /* MF_STRING items carry a command ID. '&' marks the Alt mnemonic. */
    AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_PROJECT, "&New Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_PROJECT, "&Open Project");
    AppendMenu(filemenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(filemenu, MF_STRING, ID_FILE_EXIT, "E&xit");

    /* MF_POPUP items carry a submenu handle instead - the cast is required. */
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)filemenu, "&File");

    return menubar;
}

/*
 * Returns TRUE if name is usable as a project name. The rules are the
 * ones Windows imposes on file names, because the name becomes part of
 * the .gep file name: something must be there, and none of the
 * reserved characters may appear.
 *
 * reasonout receives a message to show the user when the name is bad.
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

/*
 * Re-checks the edit box and updates the two things that depend on it:
 * whether Create Project is clickable, and what the warning line says.
 * Called once when the dialog opens and again on every keystroke, so
 * the button is never enabled for a name we would reject.
 */
static void GEditorUpdateNameValidity(HWND hdlg)
{
    char name[GEDITOR_NAME_MAX];
    const char *reason;
    BOOL valid;

    GetDlgItemText(hdlg, IDC_PROJECT_NAME, name, sizeof(name));
    valid = GEditorNameIsValid(name, &reason);

    EnableWindow(GetDlgItem(hdlg, IDC_CREATE_PROJECT), valid);
    SetDlgItemText(hdlg, IDC_NAME_WARNING, reason);
}

/*
 * Dialog procedure for IDD_NEW_PROJECT.
 *
 * Unlike a window procedure this returns TRUE when it handled the
 * message and FALSE to let the default dialog handling run - it does
 * not call DefWindowProc.
 *
 * The caller passes a char buffer as the DialogBoxParam parameter; we
 * stash it and fill it in if the user creates a project.
 */
static INT_PTR CALLBACK GEditorNewProjectProc(HWND hdlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
    char *nameout;

    switch (msg)
    {
    case WM_INITDIALOG:
        /* Remember where to write the chosen name. */
        SetWindowLongPtr(hdlg, DWLP_USER, (LONG_PTR)lparam);

        SendDlgItemMessage(hdlg, IDC_PROJECT_NAME, EM_LIMITTEXT, GEDITOR_NAME_MAX - 1, 0);
        SetDlgItemText(hdlg, IDC_PROJECT_NAME, "GE Project");

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
            /* EN_CHANGE arrives on every edit, so validation is live. */
            if (HIWORD(wparam) == EN_CHANGE)
            {
                GEditorUpdateNameValidity(hdlg);
            }
            return TRUE;

        case IDC_CREATE_PROJECT:
            nameout = (char *)GetWindowLongPtr(hdlg, DWLP_USER);
            GetDlgItemText(hdlg, IDC_PROJECT_NAME, nameout, GEDITOR_NAME_MAX);
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
static BOOL GEditorPromptForNewProject(HWND hwnd, char *nameout)
{
    INT_PTR result;

    nameout[0] = '\0';

    result = DialogBoxParam(GetModuleHandle(NULL),
                            MAKEINTRESOURCE(IDD_NEW_PROJECT),
                            hwnd,
                            GEditorNewProjectProc,
                            (LPARAM)nameout);

    return (result == IDOK);
}

/*
 * Asks the user for a .gep project file.
 *
 * Fills pathout (a buffer of at least MAX_PATH chars) and returns
 * TRUE if a file was chosen, FALSE if the user cancelled.
 *
 * This is the Explorer-style common dialog, so the File name box
 * accepts a pasted full path - to a file or to a folder - and the
 * address bar can be clicked and typed into.
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

    ofn.Flags = OFN_EXPLORER          /* modern Explorer-style dialog */
              | OFN_FILEMUSTEXIST     /* refuse names that do not exist */
              | OFN_PATHMUSTEXIST
              | OFN_HIDEREADONLY      /* hide the vestigial read-only box */
              | OFN_NOCHANGEDIR;      /* do not move our working directory */

    return GetOpenFileName(&ofn);
}

/*
 * Window procedure: Windows calls this for every message aimed at our
 * window. We handle the ones we care about and hand the rest to
 * DefWindowProc, which supplies the standard behaviour (dragging,
 * resizing, the min/max/close buttons, and so on).
 */
static LRESULT CALLBACK GEditorWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_COMMAND:
        /* Menu selections arrive here with the command ID in the low word. */
        switch (LOWORD(wparam))
        {
        case ID_FILE_NEW_PROJECT:
        {
            char name[GEDITOR_NAME_MAX];

            if (GEditorPromptForNewProject(hwnd, name))
            {
                /* Nothing is created yet - show what we would make. */
                MessageBox(hwnd, name, GEDITOR_TITLE, MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        case ID_FILE_OPEN_PROJECT:
        {
            char path[MAX_PATH];

            if (GEditorPromptForProject(hwnd, path, sizeof(path)))
            {
                /* Nothing loads yet - show what we would open. */
                MessageBox(hwnd, path, GEDITOR_TITLE, MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        case ID_FILE_EXIT:
            /* Send the same message the close button sends, so both
               routes share one shutdown path. When GEditor later needs
               to ask about unsaved changes, that prompt goes in the
               WM_CLOSE handler and covers the menu, the X button and
               Alt+F4 at once. */
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

    /* 1. Describe the kind of window we want, and register it. */
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = GEditorWndProc;
    wc.hInstance     = hinstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1); /* paints the blank client area */
    wc.lpszClassName = GEDITOR_CLASS;

    if (!RegisterClass(&wc))
    {
        MessageBox(NULL, "RegisterClass failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    menubar = GEditorCreateMenuBar();

    hwnd = CreateWindowEx(
        0,
        GEDITOR_CLASS,
        GEDITOR_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,       /* let Windows place it */
        GEDITOR_WIDTH, GEDITOR_HEIGHT,
        NULL, menubar, hinstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "CreateWindowEx failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showcmd);
    UpdateWindow(hwnd);

    /* 3. The message loop. GetMessage blocks until something happens,
          returns 0 when WM_QUIT arrives, and -1 on error. */
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
