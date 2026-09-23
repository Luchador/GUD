#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bgdocument.h"

typedef uintptr_t HWND, WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef unsigned int UINT;
typedef struct { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; } MSG;
enum { WM_KEYDOWN = 256, VK_RETURN = 13, VK_ESCAPE = 27,
    CB_GETDROPPEDSTATE = 0x157, FACEPROPERTIES_WM_CHANGED = 0x8019, MB_ICONERROR = 16 };
#include "controls.inc"
typedef struct {
    HWND controls[FACE_CONTROL_COUNT];
    char roomtext[32], detailimagetext[32], detaillodtext[32], opacitytext[32];
} FacePropertiesState;
static FacePropertiesState state;
static char text[32];
static HWND focus;
static int requests, errors;
static BgFacePropertiesEdit last;
static FacePropertiesState *FacePropertiesGetState(HWND panel) { assert(panel == 10); return &state; }
static HWND GetFocus(void) { return focus; }
static BOOL IsChild(HWND parent, HWND child) { return FALSE; }
static HWND GetParent(HWND hwnd) { assert(hwnd == 10); return 1; }
static void GetWindowText(HWND hwnd, char *out, int size)
{ assert(hwnd == 20); snprintf(out, (size_t)size, "%s", text); }
static void SetWindowText(HWND hwnd, const char *value)
{ assert(hwnd == 20); snprintf(text, sizeof(text), "%s", value); }
static int lstrcmp(const char *a, const char *b) { return strcmp(a, b); }
static void MessageBox(HWND hwnd, const char *body, const char *title, unsigned flags)
{ assert(hwnd == 10 && body[0]); errors++; }
static LRESULT SendMessage(HWND hwnd, UINT msg, WPARAM w, LPARAM l)
{
    assert(hwnd == 1 && msg == FACEPROPERTIES_WM_CHANGED && !w);
    last = *(const BgFacePropertiesEdit *)l; requests++; return TRUE;
}
static void FacePropertiesApplyRoom(HWND hwnd, FacePropertiesState *s, BOOL fromlist) { assert(0); }
static void FacePropertiesApplyDetailNumber(HWND hwnd, FacePropertiesState *s, int id) { assert(0); }
#include "input.inc"

int main(void)
{
    DWORD opacity;
    for (DWORD value = 0; value < 256; value++)
    {
        snprintf(text, sizeof(text), "%.1f", value * 100.0 / 255.0);
        assert(FacePropertiesParseOpacity(text, &opacity) && opacity == value);
    }
    assert(FacePropertiesParseOpacity("50", &opacity) && opacity == 128);
    assert(FacePropertiesParseOpacity("12.5", &opacity) && opacity == 32);
    assert(FacePropertiesParseOpacity("0", &opacity) && opacity == 0);
    assert(FacePropertiesParseOpacity("100", &opacity) && opacity == 255);
    const char *invalid[] = {"", "Mixed", "-1", "100.1", "50%", "50abc", "nan", "inf", "1e309"};
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
    { assert(!FacePropertiesParseOpacity(invalid[i], &opacity)); }
    state.controls[FACE_OPACITY] = focus = 20;
    strcpy(state.opacitytext, "Mixed"); strcpy(text, "50");
    MSG message = {20, WM_KEYDOWN, VK_RETURN, 0};
    assert(FacePropertiesHandleMessage(10, &message));
    assert(requests == 1 && last.fields == BG_FACE_PROPERTY_OPACITY && last.opacity == 128);
    strcpy(state.opacitytext, "50.2"); strcpy(text, "bad");
    assert(FacePropertiesHandleMessage(10, &message));
    assert(requests == 1 && errors == 1 && !strcmp(text, "50.2"));
    strcpy(text, "75"); message.wParam = VK_ESCAPE;
    assert(FacePropertiesHandleMessage(10, &message) && !strcmp(text, "50.2"));
    message.wParam = VK_RETURN;
    assert(FacePropertiesHandleMessage(10, &message) && requests == 1);
    message.wParam = '5'; assert(!FacePropertiesHandleMessage(10, &message));
    puts("PASS: opacity percentage quantization, all 256 values, input validation, mixed-value edits, Enter/Escape and no-op handling.");
    return 0;
}
