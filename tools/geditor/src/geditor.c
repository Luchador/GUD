/*
 * GEditor - GoldenEye level editor for GUD.
 *
 * Step 1: the shell. One resizable window with the standard system
 * buttons and a working close. Everything later (menus, viewport,
 * asset panes) is added onto this skeleton.
 */

#include <windows.h>

#define GEDITOR_CLASS  "GEditorWindow"
#define GEDITOR_TITLE  "GEditor"
#define GEDITOR_WIDTH  1024
#define GEDITOR_HEIGHT  720

enum {
    ID_FILE_NEW_PROJECT = 40001,
    ID_FILE_OPEN_PROJECT,
    ID_FILE_EXIT
};


static HMENU GEditorCreateMenuBar(void)
{
    HMENU menubar;
    HMENU filemenu;

    menubar = CreateMenu();
    filemenu = CreatePopupMenu();

    AppendMenu(filemenu, MF_STRING, ID_FILE_NEW_PROJECT, "&New Project");
    AppendMenu(filemenu, MF_STRING, ID_FILE_OPEN_PROJECT, "&Open Project");
    AppendMenu(filemenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(filemenu, MF_STRING, ID_FILE_EXIT, "E&xit");
    AppendMenu(menubar, MF_POPUP, (UINT_PTR)filemenu, "&File");

    return menubar;
}


static BOOL GEditorPromptForProject(HWND hwnd, char *pathout, DWORD pathmax)
{
    OPENFILENAME ofn;

    pathout[0] = '\0';

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = pathout;
    ofn.nMaxFile = pathmax;
    ofn.lpstrTitle = "Open Project";
    ofn.lpstrDefExt = "gep"; /* Appended if the user types no extension. */

    ofn.lpstrFilter = "GEditor Projects (*.gep)\0*.gep\0All Files (*.*)\0*.*\0";
    ofn.nFilterIndex = 1; /* start on the .gep filter */

    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    return GetOpenFileName(&ofn);
}


/**
  * Windows calls this for every message aimed at our window.
  * We handle the ones we care about and hand the rest to
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
                MessageBox(hwnd, "New Project: not implemented yet.", GEDITOR_TITLE, MB_OK | MB_ICONINFORMATION);
                return 0;
            case ID_FILE_OPEN_PROJECT:
            {
                char path[MAX_PATH];

                if(GEditorPromptForProject(hwnd, path, sizeof(path)))
                {
                    MessageBox(hwnd, path, GEDITOR_TITLE, MB_OK | MB_ICONINFORMATION);
                }
                return 0;
            }
            case ID_FILE_EXIT:
                /**
                 * Send the same message the close button sends, so both
                 * routes share one shutdown path.
                 */
                SendMessage(hwnd, WM_CLOSE, 0, 0);
                return 0;
        }
        break; /* anything else falls through to DefWindowProc */
 
    case WM_DESTROY:
        // Stop the process.
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

    // Register the Window class.
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

    /**
     * Create one instance. WS_OVERLAPPEDWINDOW is the combination
     * that gives a title bar, a resizable frame, and the standard
     * minimize / maximize / close buttons. 
     */
    hwnd = CreateWindowEx(
        0,
        GEDITOR_CLASS,
        GEDITOR_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        GEDITOR_WIDTH, GEDITOR_HEIGHT,
        NULL, menubar, hinstance, NULL);

    if (hwnd == NULL)
    {
        MessageBox(NULL, "CreateWindowEx failed", GEDITOR_TITLE, MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, showcmd);
    UpdateWindow(hwnd);

    /**
     * The message loop. GetMessage blocks until something happens,
     * returns 0 when WM_QUIT arrives, and -1 on error. 
     */
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
