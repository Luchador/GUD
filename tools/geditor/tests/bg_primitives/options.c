/* Run the real modal dialog procedure with Win32 message/control stubs. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "windows.h"
typedef intptr_t HWND, INT_PTR, LONG_PTR, LPARAM, WPARAM;
typedef unsigned UINT;
#define CALLBACK
#define DWLP_USER 1
#define WM_INITDIALOG 2
#define WM_COMMAND 3
#define WM_CLOSE 4
#define IDOK 1
#define IDCANCEL 2
#define MB_OK 0
#define MB_ICONINFORMATION 0
#define MB_ICONERROR 0
#define EM_LIMITTEXT 5
#define EM_SETSEL 6
#define LOWORD(v) ((unsigned)(v) & 0xffff)
#define MAKEINTRESOURCE(v) (v)
#include "primitiveoptions.h"
#include "resource.h"

static LONG_PTR userdata;
static char fields[3][64];
static int result, errors, activecontrol, mode;
enum { DEFAULTS, CUSTOM, BAD_RADIUS, BAD_SIDES, BAD_HEIGHT, CANCEL, CLOSE, MISSING };
static LONG_PTR GetWindowLongPtr(HWND dialog, int index) { return userdata; }
static void SetWindowLongPtr(HWND dialog, int index, LONG_PTR value) { userdata=value; }
static void SetDlgItemText(HWND dialog, int control, const char *text)
{ snprintf(fields[control-IDC_PRIMITIVE_RADIUS],64,"%s",text); }
static void GetDlgItemText(HWND dialog, int control, char *text, int size)
{ snprintf(text,size,"%s",fields[control-IDC_PRIMITIVE_RADIUS]); }
static void SendDlgItemMessage(HWND dialog, int control, UINT msg, WPARAM wparam, LPARAM lparam) {}
static HWND GetDlgItem(HWND dialog, int control) { return control; }
static void SetFocus(HWND control) { activecontrol=control; }
static void EndDialog(HWND dialog, int value) { assert(!result); result=value; }
static void MessageBox(HWND dialog, const char *text, const char *title, UINT flags) { errors++; }
static HINSTANCE GetModuleHandle(const char *name) { return NULL; }
static INT_PTR DialogBoxParam(HINSTANCE instance, int resource, HWND parent,
    INT_PTR (*proc)(HWND,UINT,WPARAM,LPARAM), LPARAM state);
#include "primitiveoptions.c"

static INT_PTR DialogBoxParam(HINSTANCE instance, int resource, HWND parent,
    INT_PTR (*proc)(HWND,UINT,WPARAM,LPARAM), LPARAM state)
{
    BOOL cylinder=resource==IDD_ADD_CYLINDER;
    assert(resource==IDD_ADD_CIRCLE || cylinder);
    result=errors=0; userdata=0; memset(fields,0,sizeof(fields));
    if (mode==MISSING) { return -1; }
    assert(!proc(1,WM_INITDIALOG,0,state));
    assert(activecontrol==IDC_PRIMITIVE_RADIUS);
    assert(!strcmp(fields[0],"1") && !strcmp(fields[1],"8"));
    assert(!strcmp(fields[2],cylinder ? "1" : ""));
    if (mode==CANCEL || mode==CLOSE)
    { proc(1,mode==CLOSE ? WM_CLOSE : WM_COMMAND,IDCANCEL,0); return result; }
    if (mode==CUSTOM)
    { strcpy(fields[0],"2.5"); strcpy(fields[1],"64"); strcpy(fields[2],"0.75"); }
    if (mode>=BAD_RADIUS && mode<=BAD_HEIGHT)
    { strcpy(fields[mode-BAD_RADIUS], mode==BAD_SIDES ? "8.5" : "nan"); }
    assert(proc(1,WM_COMMAND,IDOK,0));
    if (mode>=BAD_RADIUS && mode<=BAD_HEIGHT)
    {
        assert(!result && errors==1 && activecontrol==IDC_PRIMITIVE_RADIUS+mode-BAD_RADIUS);
        /* Bad input keeps the dialog open; it can be corrected and submitted. */
        strcpy(fields[mode-BAD_RADIUS],mode==BAD_SIDES ? "3" : "0.5");
        proc(1,WM_COMMAND,IDOK,0);
    }
    assert(result==IDOK);
    return result;
}

int main(void)
{
    for (BOOL cylinder=FALSE; cylinder<=TRUE; cylinder++)
    {
        PrimitiveOptions options={9,9,9}, before=options;
        for (mode=CANCEL; mode<=MISSING; mode++)
        { assert(!PrimitiveOptionsPrompt(1,cylinder,&options)); assert(!memcmp(&before,&options,sizeof(options))); }
        mode=DEFAULTS; assert(PrimitiveOptionsPrompt(1,cylinder,&options));
        assert(options.radius==1 && options.height==1 && options.sides==8);
        mode=CUSTOM; assert(PrimitiveOptionsPrompt(1,cylinder,&options));
        assert(options.radius==2.5 && options.height==(cylinder ? .75 : 1) && options.sides==64);
        for (mode=BAD_RADIUS; mode<=(cylinder ? BAD_HEIGHT : BAD_SIDES); mode++)
        { assert(PrimitiveOptionsPrompt(1,cylinder,&options) && errors==1); }
    }
    double value;
    const char *bad[]={"", " ", "0", "-1", "nan", "inf", "1e999", "1e-999", "10m", "1 2", "1e308"};
    for (unsigned i=0; i<sizeof(bad)/sizeof(*bad); i++)
    { assert(!PrimitiveOptionsNumber(bad[i],FALSE,&value)); }
    const char *badsides[]={"2", "65", "8.1", "4294967296"};
    for (unsigned i=0; i<sizeof(badsides)/sizeof(*badsides); i++)
    { assert(!PrimitiveOptionsNumber(badsides[i],TRUE,&value)); }
    assert(PrimitiveOptionsNumber(" 0.125 ",FALSE,&value) && value==.125);
    puts("PASS: primitive dialog defaults, meter inputs, side limits, corrections, cancel/close, and missing resource.");
    return 0;
}
