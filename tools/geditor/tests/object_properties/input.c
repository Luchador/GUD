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
       OBJECTPROPERTIES_WM_CHANGED, VK_CONTROL, VK_RETURN, VK_ESCAPE,
       CB_RESETCONTENT, CB_GETCOUNT, CB_ADDSTRING, CB_SETITEMDATA, CB_GETITEMDATA,
       CB_SETCURSEL, CB_GETCURSEL };
#define min(a,b) ((a) < (b) ? (a) : (b))
#include "input-types.inc"
static ObjectPropertiesState state;
static char text[64], status[256];
static HWND focus;
static int commits, textundos;
static BOOL control, canundo, reject;
static struct { LONG value; char label[80]; } choices[32];
static int choicecount, chosen;
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
    if (hwnd == state.controls[OBJECT_CCTV_PAD] && msg >= CB_RESETCONTENT && msg <= CB_GETCURSEL)
    {
        switch (msg)
        {
        case CB_RESETCONTENT: choicecount = 0; chosen = -1; return 0;
        case CB_GETCOUNT: return choicecount;
        case CB_ADDSTRING:
            assert(choicecount < 32);
            snprintf(choices[choicecount].label, sizeof(choices[choicecount].label), "%s", (char *)lparam);
            return choicecount++;
        case CB_SETITEMDATA: assert(wparam < (WPARAM)choicecount); choices[wparam].value = (LONG)lparam; return 0;
        case CB_GETITEMDATA: assert(wparam < (WPARAM)choicecount); return choices[wparam].value;
        case CB_SETCURSEL: chosen = (int)wparam; return chosen;
        case CB_GETCURSEL: return chosen;
        }
    }
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
    for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++) { ObjectPropertiesApplyDoor(0, &state, field); }
    for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++) { ObjectPropertiesApplyCctv(0, &state, field); }
    ObjectPropertiesApplyCctvPad(0, &state);
    if (reject) { return FALSE; }
    if (edit->property == SETUP_OBJECT_KEY_FLAGS || edit->property == SETUP_OBJECT_DOOR_KEY_FLAGS) { state.properties.keyflags = (DWORD)edit->value; }
    else if (edit->property == SETUP_OBJECT_AMMO_QUANTITY)
    {
        assert(edit->slot == state.ammoslot);
        state.properties.ammo[edit->slot].quantity = (unsigned short)edit->value;
    }
    else if (edit->property >= SETUP_OBJECT_CCTV_LOOK_PAD && edit->property <= SETUP_OBJECT_CCTV_RANGE)
    {
        double angle = round(edit->value * (65536.0 / 360.0)) * (360.0 / 65536.0);
        switch (edit->property)
        {
        case SETUP_OBJECT_CCTV_LOOK_PAD: state.properties.cctv.lookpad = (LONG)edit->value; break;
        case SETUP_OBJECT_CCTV_SWEEP_MIN: state.properties.cctv.sweepmin = angle; break;
        case SETUP_OBJECT_CCTV_SWEEP_MAX: state.properties.cctv.sweepmax = angle; break;
        case SETUP_OBJECT_CCTV_SPEED:
            state.properties.cctv.speed = floor(edit->value * (65536.0 / 21600.0) + 0.5) * (21600.0 / 65536.0); break;
        default: state.properties.cctv.range = (DWORD)edit->value; break;
        }
    }
    else if (edit->property >= SETUP_OBJECT_DOOR_TRAVEL && edit->property <= SETUP_OBJECT_DOOR_CLOSE_DELAY)
    {
        double value = floor(edit->value * 65536.0 + 0.5) / 65536.0;
        switch (edit->property)
        {
        case SETUP_OBJECT_DOOR_TRAVEL: state.properties.door.travel = value; break;
        case SETUP_OBJECT_DOOR_CLEARANCE: state.properties.door.clearance = value; break;
        case SETUP_OBJECT_DOOR_ACCEL: state.properties.door.accel = value; break;
        case SETUP_OBJECT_DOOR_DECEL: state.properties.door.decel = value; break;
        case SETUP_OBJECT_DOOR_SPEED: state.properties.door.speed = value; break;
        default: state.properties.door.closeframes = (DWORD)floor(edit->value * 60.0 + 0.5); break;
        }
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

static void CheckCctv(void)
{
    SetupFile setup = {0}; double value;
    setup.padcount = 2; setup.boundpadcount = 1;
    state.controls[OBJECT_CCTV_PAD] = 20;
    state.properties.object.type = PROPDEF_CCTV;
    state.properties.cctv.sweepmin = -45; state.properties.cctv.sweepmax = 45;
    state.properties.cctv.lookpad = 10000;
    assert(ObjectPropertiesControlVisible(&state, OBJECT_CCTV_PAD));
    assert(!ObjectPropertiesControlVisible(&state, OBJECT_DOOR_TYPE));
    ObjectPropertiesLoadCctvPads(&state, &setup, FALSE);
    ObjectPropertiesRefreshCctv(&state);
    assert(choicecount == 3 && chosen == 2 && choices[chosen].value == 10000);
    assert(!strcmp(choices[0].label, "Pad 0") && !strcmp(choices[2].label, "Bound pad 0"));
    int before = commits;
    ObjectPropertiesApplyCctvPad(0, &state); assert(commits == before); /* Accept unchanged. */
    chosen = 1; ObjectPropertiesApplyCctvPad(0, &state);
    assert(commits == before + 1 && state.properties.cctv.lookpad == 1);
    state.properties.cctv.lookpad = -1; ObjectPropertiesRefreshCctv(&state);
    assert(choicecount == 4 && choices[chosen].value == -1 && strstr(choices[chosen].label, "preserved"));
    ObjectPropertiesLoadCctvPads(&state, &setup, TRUE); assert(choicecount == 4);
    setup.padcount = 3; ObjectPropertiesLoadCctvPads(&state, &setup, TRUE);
    assert(choicecount == 4 && choices[2].value == 2 && choices[3].value == 10000);
    state.properties.cctv.lookpad = 10000; ObjectPropertiesRefreshCctv(&state);
    assert(chosen == 3);
    for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
    {
        state.controls[OBJECT_CCTV_FIRST + field * 3 + 1] = 30 + field;
        focus = state.controls[OBJECT_CCTV_FIRST + field * 3 + 1];
        assert(ObjectPropertiesControlVisible(&state, OBJECT_CCTV_FIRST + field * 3 + 1));
        before = commits;
        const char *valid[] = {"-30.1", "30.1", "60", "1500"};
        strcpy(text, valid[field]); state.cctvedited[field] = TRUE; canundo = TRUE;
        assert(Key(VK_RETURN) && commits == before + 1 && !state.cctvedited[field] && !canundo);
        double tolerance = field == 2 ? 10800.0 / 65536.0 : field == 3 ? 0 : 180.0 / 65536.0;
        assert(fabs(ObjectPropertiesCctvValue(&state.properties.cctv, field) - strtod(valid[field], NULL)) <= tolerance);
        ObjectPropertiesApplyCctv(0, &state, field); assert(commits == before + 1); /* Blur after Enter. */
        strcpy(text, "bad"); state.cctvedited[field] = TRUE;
        assert(Key(VK_RETURN) && commits == before + 1 && state.cctvedited[field]);
        assert(Key(VK_ESCAPE) && !state.cctvedited[field]);
        reject = TRUE; strcpy(text, "1"); state.cctvedited[field] = TRUE;
        assert(Key(VK_RETURN) && commits == before + 2 && !state.cctvedited[field]);
        assert(fabs(strtod(text, NULL) - strtod(valid[field], NULL)) <= tolerance);
        reject = FALSE;
    }
    const char *bad[] = {"", " ", "nan", "inf", "1e999", "12 units", "3 + 4"};
    for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
    {
        for (unsigned int i = 0; i < sizeof(bad) / sizeof(*bad); i++)
        { assert(!ObjectPropertiesParseCctv(&state, field, bad[i], &value)); }
    }
    assert(ObjectPropertiesParseCctv(&state, 0, " -360 ", &value) && value == -360);
    assert(ObjectPropertiesParseCctv(&state, 1, "360", &value) && value == 360);
    assert(!ObjectPropertiesParseCctv(&state, 0, "31", &value));
    assert(!ObjectPropertiesParseCctv(&state, 1, "-31", &value));
    assert(!ObjectPropertiesParseCctv(&state, 0, "-361", &value));
    assert(!ObjectPropertiesParseCctv(&state, 1, "361", &value));
    assert(ObjectPropertiesParseCctv(&state, 2, "0", &value));
    assert(ObjectPropertiesParseCctv(&state, 3, "0", &value));
    assert(!ObjectPropertiesParseCctv(&state, 2, "-1", &value));
    assert(!ObjectPropertiesParseCctv(&state, 3, "0.5", &value));
    assert(!ObjectPropertiesParseCctv(&state, 3, "-100", &value));
    focus = state.controls[OBJECT_CCTV_FIRST + 1];
    strcpy(text, "0"); state.cctvedited[0] = TRUE; canundo = TRUE; control = TRUE;
    before = commits; assert(Key('Z') && !Key('Z') && commits == before);
    control = FALSE; assert(Key(VK_ESCAPE));
    strcpy(text, "0"); state.cctvedited[0] = TRUE;
    ObjectPropertiesApplyCctv(0, &state, 0); assert(commits == before + 1); /* Blur alone. */
    state.properties.object.type = PROPDEF_PROP; state.cctvedited[0] = TRUE;
    assert(!Key(VK_RETURN) && !ObjectPropertiesControlVisible(&state, OBJECT_CCTV_PAD));
    ObjectPropertiesApplyCctv(0, &state, 0); ObjectPropertiesApplyCctvPad(0, &state);
    assert(commits == before + 1);
    puts("PASS: CCTV input units, endpoint order, pad picker, unavailable references, Enter/blur, Escape, text undo, visibility and rejected edits.");
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
    state.properties.object.type = PROPDEF_DOOR;
    for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
    { state.controls[OBJECT_DOOR_FIRST + field * 3 + 1] = 10 + field; }
    assert(ObjectPropertiesControlVisible(&state, OBJECT_DOOR_TYPE));
    assert(ObjectPropertiesControlVisible(&state, OBJECT_KEY_FIRST));
    assert(!ObjectPropertiesControlVisible(&state, OBJECT_AMMO_TYPE));
    for (int type = DOORTYPE_SLIDING; type <= DOORTYPE_AZTECCHAIR; type++)
    {
        BOOL angle = type == DOORTYPE_SWINGING || type == DOORTYPE_AZTECCHAIR;
        BOOL animation = type == DOORTYPE_EYE || type == DOORTYPE_IRIS;
        double scale = angle || animation ? 1.0 : 100.0;
        state.properties.door.type = type;
        assert(ObjectPropertiesDoorFactor(type, 0) == scale);
        assert(ObjectPropertiesDoorFactor(type, 2) == scale * 60);
        assert(ObjectPropertiesDoorFactor(type, 3) == scale * 3600);
        assert(ObjectPropertiesDoorFactor(type, 4) == scale * 3600);
        assert(ObjectPropertiesDoorFactor(type, 5) == 1);
        for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
        {
            int before = commits;
            focus = state.controls[OBJECT_DOOR_FIRST + field * 3 + 1];
            strcpy(text, "180"); state.dooredited[field] = TRUE;
            assert(Key(VK_RETURN) && commits == before + 1 && !state.dooredited[field] && !canundo);
            double expected = 180 / ObjectPropertiesDoorFactor(type, field);
            assert(fabs(ObjectPropertiesDoorValue(&state.properties.door, field) - expected) <= 0.5 / 65536.0);
            ObjectPropertiesApplyDoor(0, &state, field); assert(commits == before + 1);
            strcpy(text, "bad"); state.dooredited[field] = TRUE;
            assert(Key(VK_RETURN) && commits == before + 1 && state.dooredited[field]);
            assert(Key(VK_ESCAPE) && !state.dooredited[field] && !canundo);
        }
    }
    state.properties.door.type = DOORTYPE_SWINGING;
    assert(ObjectPropertiesParseDoor(&state, 1, "1000", &value) && value == 1000); /* Not limited to travel. */
    assert(ObjectPropertiesParseDoor(&state, 2, "180", &value) && value == 3);
    assert(ObjectPropertiesParseDoor(&state, 3, "1080", &value) && value == 0.3);
    assert(ObjectPropertiesParseDoor(&state, 5, "15", &value) && value == 15);
    assert(ObjectPropertiesParseDoor(&state, 5, "0", &value) && value == 0);
    assert(!ObjectPropertiesParseDoor(&state, 3, "0", &value));
    assert(!ObjectPropertiesParseDoor(&state, 4, "0", &value));
    assert(!ObjectPropertiesParseDoor(&state, 5, "40000000", &value));
    focus = state.controls[OBJECT_KEY_MASK]; strcpy(text, "0x80000003"); state.keyedited = TRUE;
    assert(Key(VK_RETURN) && state.properties.keyflags == 0x80000003u && !state.keyedited);
    focus = state.controls[OBJECT_DOOR_FIRST + 1]; strcpy(text, "90"); state.dooredited[0] = TRUE;
    int before = commits;
    reject = TRUE; assert(Key(VK_RETURN) && commits == before + 1 && !state.dooredited[0]);
    reject = FALSE;
    state.properties.object.type = PROPDEF_PROP; state.dooredited[0] = TRUE;
    assert(!Key(VK_RETURN)); ObjectPropertiesApplyDoor(0, &state, 0); assert(commits == before + 1);
    puts("PASS: door percentages/degrees/animation units, per-second rates, timing, locks, field visibility and input transactions.");
    puts("PASS: input validation, Enter/blur commits, Escape, text undo, rejected edits and synchronous reentrancy.");
    CheckCctv();
    return 0;
}
