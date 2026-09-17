#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

typedef intptr_t HWND, LRESULT, LPARAM;
typedef uintptr_t WPARAM, UINT_PTR, DWORD_PTR;
typedef unsigned int UINT, DWORD;
typedef int BOOL;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
#define CALLBACK
#define TRUE 1
#define FALSE 0
#undef NULL
#define NULL 0
#define MAX_PATH 260
#define CB_ERR (-1)
#define MAKELPARAM(lo, hi) ((LPARAM)((uint16_t)(lo) | (uint32_t)(uint16_t)(hi) << 16))
enum {CB_FINDSTRINGEXACT, CB_FINDSTRING, CB_GETEDITSEL, CB_SETCURSEL, CB_SETEDITSEL,
      CB_GETCURSEL, CB_GETITEMDATA, CB_SHOWDROPDOWN, WM_KEYDOWN, WM_CHAR, WM_CUT,
      WM_CLEAR, WM_UNDO, EM_UNDO, WM_NCDESTROY, VK_DELETE, VK_BACK, VK_RETURN,
      IDC_MODEL_STATUS, WM_PASTE};
static const int g_ModelCombos[] = {0, 1, 2};
static HWND g_ModelEditor = 10;
static BOOL g_ModelCompleting;
static int g_ModelCount = 6, g_ModelSelected = -1, loads, removed;
static BOOL lastframe;
static char status[200];
typedef struct {
    const char *names[4];
    int assets[4], count, row;
    char text[MAX_PATH];
    DWORD start, end;
    BOOL dropped;
} Combo;
/* Alphabetical rows deliberately differ from asset indices. */
static Combo combos[3] = {
    {{"CguardZ"}, {4}, 1, -1, "", 0, 0, FALSE},
    {{"Gpp7Z"}, {2}, 1, -1, "", 0, 0, FALSE},
    {{"PboxZ", "PpendantZ", "PpendantZ_large", "PtreeZ"}, {5, 1, 3, 0}, 4, -1, "", 0, 0, FALSE}
};
static void ModelEditorCompleteName(HWND combo);
static HWND GetDlgItem(HWND parent, int id) { return id + 1; }
static BOOL IsChild(HWND parent, HWND child) { return child == parent + 100; }
static int GetWindowText(HWND hwnd, char *text, int size)
{
    snprintf(text, size, "%s", combos[hwnd - 1].text);
    return strlen(text);
}
static void SetDlgItemText(HWND parent, int id, const char *text)
{ assert(id == IDC_MODEL_STATUS); snprintf(status, sizeof(status), "%s", text); }
static LRESULT SendMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    Combo *combo = &combos[hwnd - 1];
    switch (message)
    {
    case CB_FINDSTRINGEXACT:
    case CB_FINDSTRING:
        assert(wparam == (WPARAM)-1);
        for (int i = 0; i < combo->count; i++)
        {
            const char *text = (const char *)lparam;
            if (message == CB_FINDSTRINGEXACT ? !strcasecmp(combo->names[i], text)
                : !strncasecmp(combo->names[i], text, strlen(text))) { return i; }
        }
        return CB_ERR;
    case CB_GETEDITSEL:
        *(DWORD *)wparam = combo->start; *(DWORD *)lparam = combo->end; return 0;
    case CB_SETCURSEL:
        combo->row = (int)wparam;
        snprintf(combo->text, sizeof(combo->text), "%s", combo->row < 0 ? "" : combo->names[combo->row]);
        combo->start = combo->end = strlen(combo->text);
        return combo->row;
    case CB_SETEDITSEL:
        combo->start = (uint16_t)lparam;
        combo->end = (uint16_t)(lparam >> 16) == UINT16_MAX
            ? strlen(combo->text) : (uint16_t)(lparam >> 16);
        return TRUE;
    case CB_GETCURSEL: return combo->row;
    case CB_GETITEMDATA: return combo->assets[wparam];
    case CB_SHOWDROPDOWN: combo->dropped = wparam; return TRUE;
    default: assert(0); return 0;
    }
}
static LRESULT SendDlgItemMessage(HWND parent, int id, UINT msg, WPARAM wparam, LPARAM lparam)
{ return SendMessage(GetDlgItem(parent, id), msg, wparam, lparam); }
static void ModelEditorLoad(int index, BOOL framecamera)
{ assert(index >= 0 && index < g_ModelCount); loads++; g_ModelSelected = index; lastframe = framecamera; }
static LRESULT DefSubclassProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    Combo *combo = &combos[hwnd - 101];
    if (message == WM_NCDESTROY) { return 17; }
    if (message == WM_UNDO || message == EM_UNDO)
    {
        strcpy(combo->text, "P"); combo->start = combo->end = 1;
    }
    else if (message == WM_CHAR || message == WM_KEYDOWN || message == WM_CUT || message == WM_CLEAR)
    {
        DWORD first = combo->start, last = combo->end;
        if (first == last)
        {
            if (message == WM_CHAR && wparam == VK_BACK && first) { first--; }
            else if (message == WM_KEYDOWN && wparam == VK_DELETE && combo->text[last]) { last++; }
        }
        memmove(combo->text + first, combo->text + last, strlen(combo->text + last) + 1);
        combo->start = combo->end = first;
    }
    ModelEditorCompleteName(hwnd - 100); /* Synchronous CBN_EDITCHANGE. */
    return 17;
}
static void RemoveWindowSubclass(HWND hwnd, void *proc, UINT_PTR id) { removed++; }
#include "logic.inc"

static void Type(int category, const char *text)
{
    Combo *combo = &combos[category];
    strcpy(combo->text, text); combo->start = combo->end = strlen(text);
    ModelEditorCompleteName(category + 1);
}
static void Enter(int category)
{
    MSG msg = {category + 101, WM_KEYDOWN, VK_RETURN, 0};
    assert(ModelEditorNameKey(&msg));
}
int main(void)
{
    Combo *props = &combos[2];
    Type(2, "p"); assert(!strcmp(props->text, "PboxZ") && props->start == 1 && props->end == 5);
    assert(loads == 0 && g_ModelSelected == -1);
    Type(2, "pp"); assert(!strcmp(props->text, "PpendantZ") && props->start == 2);
    Type(2, "pPEn"); assert(!strcmp(props->text, "PpendantZ") && props->start == 4);
    props->dropped = TRUE; Enter(2);
    assert(loads == 1 && g_ModelSelected == 1 && lastframe && !props->dropped);
    /* More typing can select a different suggestion, without changing the open asset. */
    Type(2, "pt"); assert(props->row == 3 && loads == 1 && g_ModelSelected == 1);
    ModelEditorRefreshImages(); assert(loads == 2 && g_ModelSelected == 1 && !lastframe);
    Type(0, "cg"); assert(!strcmp(combos[0].text, "CguardZ") && loads == 2);
    ModelEditorRefreshImages(); assert(loads == 3 && g_ModelSelected == 1);
    Enter(0); assert(loads == 4 && g_ModelSelected == 4 && props->row == -1);
    Type(1, "gp"); Enter(1); assert(loads == 5 && g_ModelSelected == 2 && combos[0].row == -1);
    /* An exact name wins over longer names with that prefix. */
    Type(2, "PpendantZ"); Enter(2); assert(loads == 6 && g_ModelSelected == 1);
    Type(2, "PpendantZ_"); Enter(2); assert(loads == 7 && g_ModelSelected == 3);
    /* Even with a stale list selection, invalid/empty text cannot load an asset. */
    Type(2, "NoSuchModel"); Enter(2);
    assert(loads == 7 && g_ModelSelected == 3 && strstr(status, "No matching model"));
    Type(2, ""); Enter(2); assert(loads == 7 && g_ModelSelected == 3);
    /* Mid-string edits do not replace the user's text or caret. */
    strcpy(props->text, "Ppend"); props->start = props->end = 2;
    ModelEditorCompleteName(3); assert(!strcmp(props->text, "Ppend") && props->start == 2);
    props->start = 1; props->end = 3;
    ModelEditorCompleteName(3); assert(!strcmp(props->text, "Ppend") && props->end == 3);
    /* Removing an appended suffix must stay removed, including context-menu edits. */
    const UINT messages[] = {WM_CHAR, WM_KEYDOWN, WM_CUT, WM_CLEAR, WM_UNDO, EM_UNDO};
    for (unsigned i = 0; i < sizeof(messages) / sizeof(*messages); i++)
    {
        Type(2, "p");
        assert(ModelEditorNameEditProc(103, messages[i], i == 0 ? VK_BACK : VK_DELETE, 0, 1, 0) == 17);
        assert(!strcasecmp(props->text, "p") && !g_ModelCompleting);
    }
    ModelEditorNameEditProc(103, WM_CHAR, VK_BACK, 0, 1, 0);
    assert(!props->text[0]);
    Type(2, "pt"); assert(!strcmp(props->text, "PtreeZ")); /* Completion resumes after deletion. */
    strcpy(props->text, "pp"); props->start = props->end = 2;
    ModelEditorNameEditProc(103, WM_PASTE, 0, 0, 1, 0);
    assert(!strcmp(props->text, "PpendantZ"));
    /* Enter elsewhere and ordinary typing remain available to dialog navigation. */
    MSG outside = {50, WM_KEYDOWN, VK_RETURN, 0}, letter = {103, WM_KEYDOWN, 'P', 0};
    assert(!ModelEditorNameKey(&outside) && !ModelEditorNameKey(&letter));
    ModelEditorNameEditProc(103, WM_NCDESTROY, 0, 0, 1, 0); assert(removed == 1);
    assert(loads == 7);
    puts("PASS: all model categories, case-insensitive/exact/prefix completion, Enter, sorted asset IDs,");
    puts("uncommitted-name refresh, invalid/empty names, caret edits, deletion/cut/undo and paste (ASan + UBSan).");
    return 0;
}
