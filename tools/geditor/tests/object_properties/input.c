#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"
#include <src/propconstants.h>

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
    assert(edit->objectindex == 4 && edit->sourceoffset == 200 && edit->type == state.properties.object.type);
    commits++;
    /* A synchronous callback can move focus: its nested commit must be ignored. */
    ObjectPropertiesApplyHealth(0, &state);
    ObjectPropertiesApplyExtra(0, &state, OBJECT_KEY_MASK);
    ObjectPropertiesApplyExtra(0, &state, OBJECT_QUANTITY);
    if (reject) { return FALSE; }
    if (edit->property == SETUP_OBJECT_KEY_FLAGS) { state.properties.keyflags = (DWORD)edit->value; }
    else if (edit->property == SETUP_OBJECT_AMMO_QUANTITY)
    {
        assert(edit->slot == state.ammoslot);
        state.properties.ammo[edit->slot].quantity = (unsigned short)edit->value;
    }
    else
    {
        assert(edit->property == SETUP_OBJECT_HEALTH);
        state.properties.health = floor(edit->value * 65536.0 + 0.5) / 65536.0;
    }
    return TRUE;
}
static void Type(const char *value)
{ snprintf(text, sizeof(text), "%s", value); state.edited = TRUE; canundo = TRUE; }
static BOOL Key(WPARAM key)
{ MSG msg = {WM_KEYDOWN, key}; return ObjectPropertiesHandleMessage(0, &msg); }

static void CheckContents(void)
{
    SetupObjectProperties properties = {0}, before;
    char contents[OBJECT_CONTENTS_TEXT_MAX], small[12];
    properties.ammo[AMMO_REMOTEMINE - 1].quantity = 6;
    before = properties;
    assert(ObjectPropertiesFormatContents(&properties, FALSE, contents, sizeof(contents)));
    assert(strstr(contents, "Remote mines\r\nAgent: 12\r\nSecret Agent: 9\r\n00 Agent: 6\r\n007 Mode: 6"));
    assert(!memcmp(&before, &properties, sizeof(properties))); /* Preview never edits setup values. */
    properties.ammo[AMMO_REMOTEMINE - 1].quantity = 5;
    assert(ObjectPropertiesFormatContents(&properties, FALSE, contents, sizeof(contents)));
    assert(strstr(contents, "Agent: 10\r\nSecret Agent: 7\r\n00 Agent: 5\r\n007 Mode: 5"));
    assert(ObjectPropertiesFormatContents(&properties, TRUE, contents, sizeof(contents)));
    assert(strstr(contents, "Remote mines: 5") && !strstr(contents, "Secret Agent"));
    memset(&properties, 0, sizeof(properties));
    assert(ObjectPropertiesFormatContents(&properties, FALSE, contents, sizeof(contents)));
    assert(strstr(contents, "Empty"));
    properties.ammo[AMMO_9MM - 1].quantity = 1;
    assert(ObjectPropertiesFormatContents(&properties, FALSE, contents, sizeof(contents)));
    assert(strstr(contents, "9mm\r\nAgent: 2\r\nSecret Agent: 1\r\n00 Agent: 1\r\n007 Mode: 1"));
    for (int slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++) { properties.ammo[slot].quantity = 65535; }
    assert(ObjectPropertiesFormatContents(&properties, FALSE, contents, sizeof(contents)));
    assert(strlen(contents) > 512 && strstr(contents, "Golden Gun rounds"));
    assert(strstr(contents, "9mm\r\nAgent: 131070\r\nSecret Agent: 98302"));
    assert(!strstr(contents, "Inventory limits"));
    assert(!ObjectPropertiesFormatContents(&properties, FALSE, small, sizeof(small)));
    assert(small[sizeof(small) - 1] == 0);
    assert(!ObjectPropertiesFormatContents(&properties, FALSE, small, 0));
    puts("PASS: all four difficulty amounts, fractional truncation, single 9mm slot, multiplayer, empty/full crates and complete long summaries.");
}

int main(void)
{
    CheckContents();
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
    state.controls[OBJECT_KEY_MASK] = 3; state.controls[OBJECT_QUANTITY] = 4;
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
    state.edited = FALSE;
    DWORD mask;
    assert(ObjectPropertiesParseUnsigned("0xFFFFFFFF", TRUE, 0xffffffffu, &mask) && mask == 0xffffffffu);
    assert(ObjectPropertiesParseUnsigned("80000000", TRUE, 0xffffffffu, &mask) && mask == 0x80000000u);
    assert(ObjectPropertiesParseUnsigned(" 0010 ", FALSE, 65535, &mask) && mask == 10); /* Not octal. */
    const char *badmask[] = {"", "0x", "0x100000000", "-1", "+1", "1.5", "FFFFFFFFF", "G123"};
    for (unsigned int i = 0; i < sizeof(badmask) / sizeof(*badmask); i++)
    { assert(!ObjectPropertiesParseUnsigned(badmask[i], TRUE, 0xffffffffu, &mask)); }
    const char *badquantity[] = {"", "-1", "+1", "1.5", "65536", "1e3", "0x10", "10 ammo"};
    for (unsigned int i = 0; i < sizeof(badquantity) / sizeof(*badquantity); i++)
    { assert(!ObjectPropertiesParseUnsigned(badquantity[i], FALSE, 65535, &mask)); }
    state.properties.object.type = PROPDEF_KEY; focus = state.controls[OBJECT_KEY_MASK];
    strcpy(text, "0x80000001"); state.keyedited = TRUE;
    assert(Key(VK_RETURN) && state.properties.keyflags == 0x80000001u && commits == 4 && !state.keyedited);
    assert(!strcmp(text, "0x80000001"));
    ObjectPropertiesApplyExtra(0, &state, OBJECT_KEY_MASK); assert(commits == 4);
    strcpy(text, "invalid"); state.keyedited = TRUE; assert(Key(VK_RETURN));
    assert(commits == 4 && state.keyedited);
    assert(Key(VK_ESCAPE) && !state.keyedited && !strcmp(text, "0x80000001"));
    state.properties.object.type = PROPDEF_AMMO; state.ammoslot = AMMOTYPE_GLOBAL_MAX - 1;
    assert(!Key(VK_RETURN)); /* Hidden key field cannot commit. */
    focus = state.controls[OBJECT_QUANTITY]; strcpy(text, "65535"); state.quantityedited = TRUE;
    assert(Key(VK_RETURN) && state.properties.ammo[state.ammoslot].quantity == 65535 && commits == 5 && !state.quantityedited);
    assert(state.properties.ammo[0].quantity == 0);
    strcpy(text, "-1"); state.quantityedited = TRUE; assert(Key(VK_RETURN));
    assert(commits == 5 && state.quantityedited);
    assert(Key(VK_ESCAPE) && !state.quantityedited && !strcmp(text, "65535"));
    assert(ObjectPropertiesControlVisible(&state, OBJECT_QUANTITY));
    assert(!ObjectPropertiesControlVisible(&state, OBJECT_KEY_FIRST));
    state.properties.object.type = PROPDEF_MAGAZINE;
    assert(!ObjectPropertiesControlVisible(&state, OBJECT_QUANTITY));
    assert(ObjectPropertiesControlVisible(&state, OBJECT_AMMO_TYPE));
    puts("PASS: input validation, Enter/blur commits, Escape, text undo, rejected edits and synchronous reentrancy.");
    return 0;
}
