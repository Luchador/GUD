#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"

typedef uintptr_t HWND, ULONG_PTR, WPARAM;
typedef intptr_t LPARAM, LRESULT;
typedef struct { unsigned int message; WPARAM wParam; } MSG;
enum { WM_KEYDOWN = 1, EM_EMPTYUNDOBUFFER, EM_CANUNDO, WM_UNDO,
       OBJECTPROPERTIES_WM_CHANGED, VK_CONTROL, VK_RETURN, VK_ESCAPE };
#include "input-types.inc"
static ObjectPropertiesState state;
static char text[64], status[256];
static HWND focus;
static int commits, textundos;
static BOOL control, canundo, reject;
static ObjectPropertiesState *ObjectPropertiesGetState(HWND hwnd) { return &state; }
static HWND GetFocus(void) { return focus; }
static HWND GetParent(HWND hwnd) { return 1; }
static short GetKeyState(int key) { return control ? (short)0x8000 : 0; }
static void GetWindowText(HWND hwnd, char *out, size_t size) { snprintf(out, size, "%s", text); }
static void SetWindowText(HWND hwnd, const char *value) { snprintf(text, sizeof(text), "%s", value); }
static void ObjectPropertiesStatus(HWND hwnd, ObjectPropertiesState *s, const char *value)
{ snprintf(status, sizeof(status), "%s", value); }
static LRESULT SendMessage(HWND hwnd, unsigned int msg, WPARAM wparam, LPARAM lparam);
#include "input-logic.inc"

static LRESULT SendMessage(HWND hwnd, unsigned int msg, WPARAM wparam, LPARAM lparam)
{
    if (msg == EM_EMPTYUNDOBUFFER) { canundo = FALSE; return 0; }
    if (msg == EM_CANUNDO) { return canundo; }
    if (msg == WM_UNDO) { textundos++; canundo = FALSE; return 0; }
    assert(msg == OBJECTPROPERTIES_WM_CHANGED);
    const SetupObjectPropertyEdit *edit = (const SetupObjectPropertyEdit *)lparam;
    assert(edit->objectindex == 4 && edit->sourceoffset == 200 && edit->type == 3);
    assert(edit->property == SETUP_OBJECT_HEALTH);
    commits++;
    /* A synchronous callback can move focus: its nested commit must be ignored. */
    ObjectPropertiesApplyHealth(0, &state);
    if (reject) { return FALSE; }
    state.properties.health = floor(edit->value * 65536.0 + 0.5) / 65536.0;
    return TRUE;
}
static void Type(const char *value)
{ snprintf(text, sizeof(text), "%s", value); state.edited = TRUE; canundo = TRUE; }
static BOOL Key(WPARAM key)
{ MSG msg = {WM_KEYDOWN, key}; return ObjectPropertiesHandleMessage(0, &msg); }

int main(void)
{
    double value;
    const char *invalid[] = {"", " ", "-1", "nan", "inf", "1e999", "12 units", "3 + 4", "32768"};
    for (unsigned int i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
    { assert(!ObjectPropertiesParseHealth(invalid[i], &value)); }
    assert(ObjectPropertiesParseHealth("  42.25 \t", &value) && value == 42.25);
    assert(ObjectPropertiesParseHealth("2e3", &value) && value == 2000);
    state.selected = TRUE; state.objectindex = 4;
    state.properties.object.sourceoffset = 200; state.properties.object.type = 3;
    state.properties.health = 1000;
    state.controls[OBJECT_HEALTH] = focus = 2;
    ObjectPropertiesResetHealth(&state);
    assert(!state.edited && !canundo && !strcmp(text, "1000"));
    Type("0.1"); assert(Key(VK_RETURN));
    assert(commits == 1 && !state.edited && !canundo);
    assert(fabs(strtod(text, NULL) - 0.1) <= 0.5 / 65536.0);
    ObjectPropertiesApplyHealth(0, &state); assert(commits == 1); /* Subsequent blur. */
    Type("bad"); assert(Key(VK_RETURN));
    assert(commits == 1 && state.edited && strstr(status, "has not changed"));
    assert(Key(VK_ESCAPE) && !state.edited && !canundo && commits == 1);
    Type("42.25"); ObjectPropertiesApplyHealth(0, &state); /* Valid focus-loss commit. */
    assert(commits == 2 && state.properties.health == 42.25);
    reject = TRUE; Type("50"); assert(Key(VK_RETURN));
    assert(commits == 3 && !strcmp(text, "42.25") && !state.edited);
    reject = FALSE;
    Type("123"); control = TRUE;
    assert(Key('Z') && textundos == 1 && commits == 3);
    assert(!Key('Z')); /* No pending text undo: let the editor handle history. */
    control = FALSE; state.updating = TRUE;
    ObjectPropertiesApplyHealth(0, &state); assert(commits == 3);
    state.updating = FALSE; state.selected = FALSE;
    assert(!Key(VK_RETURN)); ObjectPropertiesApplyHealth(0, &state); assert(commits == 3);
    state.selected = TRUE; focus = 0;
    assert(!Key(VK_RETURN) && !Key(VK_ESCAPE));
    puts("PASS: input validation, Enter/blur commits, Escape, text undo, rejected edits and synchronous reentrancy.");
    return 0;
}
