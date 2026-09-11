/* Shared inspector controls. The setup API owns native encodings and edits;
 * this child window owns only its current values and uncommitted text. */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <src/propconstants.h>
#include "objectproperties.h"
#include "modelload.h"

#define OBJECTPROPERTIES_CLASS "GEditorObjectProperties"
#define OBJECT_CONTENTS_TEXT_MAX 4096
#define OBJECT_DOOR_FIELD_COUNT 6
#define OBJECT_CCTV_FIELD_COUNT 4
enum { OBJECT_TYPE, OBJECT_MODEL_LABEL, OBJECT_MODEL, OBJECT_MODEL_HELP,
       OBJECT_HEALTH_LABEL, OBJECT_HEALTH, OBJECT_HEALTH_HELP,
       OBJECT_CCTV_PAD_LABEL, OBJECT_CCTV_PAD, OBJECT_CCTV_PAD_HELP,
       OBJECT_CCTV_FIRST, OBJECT_CCTV_LAST = OBJECT_CCTV_FIRST + OBJECT_CCTV_FIELD_COUNT * 3 - 1,
       OBJECT_DOOR_TYPE_LABEL, OBJECT_DOOR_TYPE, OBJECT_DOOR_TYPE_HELP,
       OBJECT_DOOR_FIRST, OBJECT_DOOR_LAST = OBJECT_DOOR_FIRST + OBJECT_DOOR_FIELD_COUNT * 3 - 1,
       OBJECT_DOOR_SOUND_LABEL, OBJECT_DOOR_SOUND, OBJECT_DOOR_SOUND_HELP,
       OBJECT_DOOR_FLAGS_LABEL, OBJECT_DOOR_FLAG_FIRST, OBJECT_DOOR_FLAG_LAST = OBJECT_DOOR_FLAG_FIRST + 3,
       OBJECT_DOOR_FLAGS_HELP,
       OBJECT_KEY_LABEL, OBJECT_KEY_MASK, OBJECT_KEY_HELP,
       OBJECT_KEY_FIRST, OBJECT_KEY_LAST = OBJECT_KEY_FIRST + 31,
       OBJECT_AMMO_LABEL, OBJECT_AMMO_TYPE, OBJECT_AMMO_HELP,
       OBJECT_QUANTITY_LABEL, OBJECT_QUANTITY, OBJECT_CONTENTS,
       OBJECT_IDENTITY, OBJECT_STATUS, OBJECT_CONTROL_COUNT };

/* New specialized sections can reuse these controls and the property-edit
 * request, while complex fields remain free to supply their own UI. */
static const struct {
    SetupObjectProperty property;
    int label, input, help;
    const char *name, *description;
} g_SharedProperties[] = {
    {SETUP_OBJECT_MODEL, OBJECT_MODEL_LABEL, OBJECT_MODEL, OBJECT_MODEL_HELP,
     "Model", "Appearance only; the object keeps its type and behavior."},
    {SETUP_OBJECT_HEALTH, OBJECT_HEALTH_LABEL, OBJECT_HEALTH, OBJECT_HEALTH_HELP,
     "Health", "Damage needed to destroy the object. Its type and flags also affect destructibility."}
};

typedef struct ObjectPropertiesState {
    HWND controls[OBJECT_CONTROL_COUNT];
    DWORD objectindex;
    ULONG_PTR document;
    SetupObjectProperties properties;
    BOOL selected, updating, edited, committing;
    BOOL keyedited, quantityedited, multiplayer;
    BOOL dooredited[OBJECT_DOOR_FIELD_COUNT];
    BOOL cctvedited[OBJECT_CCTV_FIELD_COUNT];
    DWORD cctvpadcount, cctvboundpadcount;
    DWORD ammoslot;
    int scroll, wheelremainder;
    char projectdir[MAX_PATH];
} ObjectPropertiesState;

/* Designated native IDs keep names correct if the enum grows. */
static const char *g_AmmoNames[AMMOTYPE_MAX] = {
    [AMMO_NONE] = "None", [AMMO_9MM] = "9mm",
    [AMMO_RIFLE] = "Rifle rounds", [AMMO_SHOTGUN] = "Shotgun shells",
    [AMMO_GRENADE] = "Grenades", [AMMO_ROCKETS] = "Rockets",
    [AMMO_REMOTEMINE] = "Remote mines", [AMMO_PROXMINE] = "Proximity mines",
    [AMMO_TIMEDMINE] = "Timed mines", [AMMO_KNIFE] = "Throwing knives",
    [AMMO_GRENADEROUND] = "Grenade rounds", [AMMO_MAGNUM] = "Magnum rounds",
    [AMMO_GGUN] = "Golden Gun rounds", [AMMO_DARTS] = "Darts",
    [AMMO_EXPLOSIVEPEN] = "Explosive pen", [AMMO_BOMBCASE] = "Bomb case",
    [AMMO_FLARE] = "Flares", [AMMO_PITON] = "Pitons", [AMMO_DYNAMITE] = "Dynamite",
    [AMMO_BUG] = "Bugs", [AMMO_MICRO_CAMERA] = "Micro camera", [AMMO_GEKEY] = "GoldenEye key",
    [AMMO_PLASTIQUE] = "Plastique", [AMMO_WATCH_LASER] = "Watch laser",
    [AMMO_WATCH_MAGNET] = "Watch magnet", [AMMO_UNK] = "Unknown ammo (26)",
    [AMMO_CAMERA] = "Camera", [AMMO_TANK] = "Tank shells", [AMMO_TOKEN] = "Token"
};

/* Each numeric door field has a label, edit and optional explanation. */
static const struct {
    SetupObjectProperty property;
    const char *name, *help;
} g_DoorFields[OBJECT_DOOR_FIELD_COUNT] = {
    {SETUP_OBJECT_DOOR_TRAVEL, "Open travel", ""},
    {SETUP_OBJECT_DOOR_CLEARANCE, "Collision clearance", "Collision stops at this travel. Set above open travel to keep collision while open."},
    {SETUP_OBJECT_DOOR_SPEED, "Maximum speed", ""},
    {SETUP_OBJECT_DOOR_ACCEL, "Acceleration", ""},
    {SETUP_OBJECT_DOOR_DECEL, "Deceleration", ""},
    {SETUP_OBJECT_DOOR_CLOSE_DELAY, "Auto-close delay (seconds)", "Time fully open before closing. 0 closes without waiting. Start open in Flags can hold a door open."}
};
static const char *g_DoorTypes[] = {
    [DOORTYPE_SLIDING] = "Sliding", [DOORTYPE_FLEXI1] = "Folding 1 (Bunker)",
    [DOORTYPE_FLEXI2] = "Folding 2", [DOORTYPE_FLEXI3] = "Folding 3",
    [DOORTYPE_VERTICAL] = "Vertical sliding", [DOORTYPE_SWINGING] = "Swinging",
    [DOORTYPE_EYE] = "Eye (Caverns)", [DOORTYPE_IRIS] = "Iris (Caverns)",
    [DOORTYPE_FALLAWAY] = "Fall-away panel", [DOORTYPE_AZTECCHAIR] = "Aztec chair"
};
static const char *g_DoorSounds[] = {
    [DOOR_OPEN_SOUND_NONE] = "Silent", [DOOR_OPEN_SOUND_01] = "Smart sliding",
    [DOOR_OPEN_SOUND_02] = "Train sliding with latch", [DOOR_OPEN_SOUND_METAL] = "Metal sliding",
    [DOOR_OPEN_SOUND_04] = "Heavy sliding", [DOOR_OPEN_SOUND_WOOD] = "Wooden swing",
    [DOOR_OPEN_SOUND_06] = "Train sliding", [DOOR_OPEN_SOUND_WOOD_2] = "Wooden sliding",
    [DOOR_OPEN_SOUND_WOOD_3] = "Wood / train sliding", [DOOR_OPEN_SOUND_09] = "Shutter",
    [DOOR_OPEN_SOUND_METAL_2] = "Metal swing", [DOOR_OPEN_SOUND_11] = "Train / metal",
    [DOOR_OPEN_SOUND_METAL_3] = "Metal swing 2", [DOOR_OPEN_SOUND_13] = "Train sliding (loop)",
    [DOOR_OPEN_SOUND_HYDROLIC] = "Hydraulic", [DOOR_OPEN_SOUND_STONE] = "Stone sliding",
    [DOOR_OPEN_SOUND_16] = "Heavy sliding (short)", [DOOR_OPEN_SOUND_METAL_4] = "Metal / train sliding"
};
static const struct { unsigned short bit; const char *name; } g_DoorFlags[] = {
    {DOORFLAG_EXTENDEDY, "Extend collision above / below"},
    {DOORFLAG_WINDOWED, "Windowed (distance transparency)"},
    {DOORFLAG_CLIP_TO_BBOX, "Clip sliding mesh to bounds"},
    {DOORFLAG_FLIP, "Mirror model front / back"}
};

static const struct {
    SetupObjectProperty property;
    const char *name, *help;
    double minimum, maximum;
} g_CctvFields[OBJECT_CCTV_FIELD_COUNT] = {
    {SETUP_OBJECT_CCTV_SWEEP_MIN, "Sweep minimum (degrees)",
     "Angles are relative to the look-at direction. Minimum must not exceed maximum.", -360, 360},
    {SETUP_OBJECT_CCTV_SWEEP_MAX, "Sweep maximum (degrees)",
     "The camera starts at maximum and sweeps toward minimum. Equal angles hold it still.", -360, 360},
    {SETUP_OBJECT_CCTV_SPEED, "Maximum turn speed (degrees/s)",
     "The game accelerates and brakes automatically. 0 stops the sweep.", 0, 2147483647.0 * (21600.0 / 65536.0)},
    {SETUP_OBJECT_CCTV_RANGE, "Detection range (world units)",
     "0 means unlimited distance. Line of sight and the camera's viewing angle still apply. Detection flags are in the Flags tab.", 0, 2147483647.0}
};
static int ObjectPropertiesCctvField(int id)
{
    return id >= OBJECT_CCTV_FIRST && id <= OBJECT_CCTV_LAST
        && (id - OBJECT_CCTV_FIRST) % 3 == 1 ? (id - OBJECT_CCTV_FIRST) / 3 : -1;
}
static BOOL ObjectPropertiesCctvPending(const ObjectPropertiesState *state)
{
    for (int i = 0; i < OBJECT_CCTV_FIELD_COUNT; i++) { if (state->cctvedited[i]) { return TRUE; } }
    return FALSE;
}
static double ObjectPropertiesCctvValue(const SetupCctvProperties *cctv, int field)
{
    switch (g_CctvFields[field].property)
    {
    case SETUP_OBJECT_CCTV_SWEEP_MIN: return cctv->sweepmin;
    case SETUP_OBJECT_CCTV_SWEEP_MAX: return cctv->sweepmax;
    case SETUP_OBJECT_CCTV_SPEED: return cctv->speed;
    default: return cctv->range;
    }
}
static void ObjectPropertiesResetCctv(ObjectPropertiesState *state, int field)
{
    int id = OBJECT_CCTV_FIRST + field * 3 + 1;
    char text[64] = "";
    if (state->selected)
    { snprintf(text, sizeof(text), "%.15g", ObjectPropertiesCctvValue(&state->properties.cctv, field)); }
    state->updating = TRUE;
    SetWindowText(state->controls[id], text);
    SendMessage(state->controls[id], EM_EMPTYUNDOBUFFER, 0, 0);
    state->updating = FALSE;
    state->cctvedited[field] = FALSE;
}

static int ObjectPropertiesDoorField(int id)
{
    return id >= OBJECT_DOOR_FIRST && id <= OBJECT_DOOR_LAST
        && (id - OBJECT_DOOR_FIRST) % 3 == 1 ? (id - OBJECT_DOOR_FIRST) / 3 : -1;
}
static BOOL ObjectPropertiesDoorPending(const ObjectPropertiesState *state)
{
    for (int i = 0; i < OBJECT_DOOR_FIELD_COUNT; i++) { if (state->dooredited[i]) { return TRUE; } }
    return FALSE;
}
static double ObjectPropertiesDoorValue(const SetupDoorProperties *door, int field)
{
    switch (g_DoorFields[field].property)
    {
    case SETUP_OBJECT_DOOR_TRAVEL: return door->travel;
    case SETUP_OBJECT_DOOR_CLEARANCE: return door->clearance;
    case SETUP_OBJECT_DOOR_SPEED: return door->speed;
    case SETUP_OBJECT_DOOR_ACCEL: return door->accel;
    case SETUP_OBJECT_DOOR_DECEL: return door->decel;
    default: return door->closeframes / 60.0;
    }
}
static int ObjectPropertiesDoorUnits(unsigned short type)
{
    if (type == DOORTYPE_SWINGING || type == DOORTYPE_AZTECCHAIR) { return 1; }
    if (type == DOORTYPE_EYE || type == DOORTYPE_IRIS || type > DOORTYPE_AZTECCHAIR) { return 2; }
    return 0;
}
static double ObjectPropertiesDoorFactor(unsigned short type, int field)
{
    SetupObjectProperty property = g_DoorFields[field].property;
    double factor = ObjectPropertiesDoorUnits(type) == 0 ? 100.0 : 1.0;
    if (property == SETUP_OBJECT_DOOR_CLOSE_DELAY) { return 1.0; }
    if (property == SETUP_OBJECT_DOOR_SPEED) { factor *= 60.0; }
    if (property == SETUP_OBJECT_DOOR_ACCEL || property == SETUP_OBJECT_DOOR_DECEL) { factor *= 3600.0; }
    return factor;
}
static void ObjectPropertiesResetDoor(ObjectPropertiesState *state, int field)
{
    int id = OBJECT_DOOR_FIRST + field * 3 + 1;
    char text[64] = "";
    if (state->selected)
    { snprintf(text, sizeof(text), "%.15g", ObjectPropertiesDoorValue(&state->properties.door, field)
        * ObjectPropertiesDoorFactor(state->properties.door.type, field)); }
    state->updating = TRUE;
    SetWindowText(state->controls[id], text);
    SendMessage(state->controls[id], EM_EMPTYUNDOBUFFER, 0, 0);
    state->updating = FALSE;
    state->dooredited[field] = FALSE;
}

static BOOL ObjectPropertiesIsCombo(int id)
{ return id == OBJECT_MODEL || id == OBJECT_AMMO_TYPE || id == OBJECT_DOOR_TYPE || id == OBJECT_DOOR_SOUND || id == OBJECT_CCTV_PAD; }
static BOOL ObjectPropertiesIsEdit(int id)
{ return id == OBJECT_HEALTH || id == OBJECT_KEY_MASK || id == OBJECT_QUANTITY
    || ObjectPropertiesDoorField(id) >= 0 || ObjectPropertiesCctvField(id) >= 0; }
static BOOL ObjectPropertiesControlVisible(const ObjectPropertiesState *state, int id)
{
    unsigned char type = state->properties.object.type;
    if (id >= OBJECT_CCTV_PAD_LABEL && id <= OBJECT_CCTV_LAST) { return state->selected && type == PROPDEF_CCTV; }
    if (id >= OBJECT_KEY_LABEL && id <= OBJECT_KEY_LAST) { return state->selected && (type == PROPDEF_KEY || type == PROPDEF_DOOR); }
    if (id >= OBJECT_DOOR_TYPE_LABEL && id <= OBJECT_DOOR_FLAGS_HELP) { return state->selected && type == PROPDEF_DOOR; }
    if (id >= OBJECT_AMMO_LABEL && id <= OBJECT_AMMO_HELP)
    { return state->selected && (type == PROPDEF_AMMO || type == PROPDEF_MAGAZINE); }
    if (id >= OBJECT_QUANTITY_LABEL && id <= OBJECT_CONTENTS) { return state->selected && type == PROPDEF_AMMO; }
    return TRUE;
}

static ObjectPropertiesState *ObjectPropertiesGetState(HWND hwnd)
{ return (ObjectPropertiesState *)GetWindowLongPtr(hwnd, GWLP_USERDATA); }

static void ObjectPropertiesLayout(HWND hwnd, ObjectPropertiesState *state)
{
    RECT client, bounds[OBJECT_CONTROL_COUNT] = {{0}};
    SCROLLINFO info = {0};
    int width, y = 4;
    HDC dc = GetDC(hwnd);
    HFONT old = (HFONT)SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    GetClientRect(hwnd, &client);
    width = max(1, client.right - 8);
    for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
    {
        int height = 24;
        if (!ObjectPropertiesControlVisible(state, i)) { continue; }
        if (i >= OBJECT_KEY_FIRST && i <= OBJECT_KEY_LAST)
        {
            int column = (i - OBJECT_KEY_FIRST) % 4;
            SetRect(&bounds[i], 4 + width * column / 4, y, 4 + width * (column + 1) / 4, y + height);
            if (column == 3) { y += height; }
            continue;
        }
        if (!ObjectPropertiesIsCombo(i) && !ObjectPropertiesIsEdit(i))
        {
            BOOL checkbox = i >= OBJECT_DOOR_FLAG_FIRST && i <= OBJECT_DOOR_FLAG_LAST;
            char text[OBJECT_CONTENTS_TEXT_MAX]; RECT rect = {0, 0, max(1, width - (checkbox ? 20 : 0)), 0};
            GetWindowText(state->controls[i], text, sizeof(text));
            DrawText(dc, text, -1, &rect, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            height = checkbox ? max(24, rect.bottom + 4) : rect.bottom;
        }
        SetRect(&bounds[i], 4, y, width + 4, y + height);
        y += height + (i == OBJECT_MODEL_HELP || i == OBJECT_HEALTH_HELP ? 16 : 6);
    }
    SelectObject(dc, old); ReleaseDC(hwnd, dc);
    state->scroll = max(0, min(state->scroll, y - client.bottom));
    info.cbSize = sizeof(info); info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL;
    info.nMax = y - 1; info.nPage = max(0, client.bottom); info.nPos = state->scroll;
    SetScrollInfo(hwnd, SB_VERT, &info, FALSE);
    for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
    {
        const RECT *r = &bounds[i];
        ShowWindow(state->controls[i], ObjectPropertiesControlVisible(state, i) ? SW_SHOWNA : SW_HIDE);
        /* Do not copy stale child pixels while scrolling or resizing. */
        SetWindowPos(state->controls[i], NULL, r->left, r->top - state->scroll,
            r->right - r->left, ObjectPropertiesIsCombo(i) ? 320 : r->bottom - r->top,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOREDRAW);
    }
    RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static void ObjectPropertiesStatus(HWND hwnd, ObjectPropertiesState *state, const char *text)
{
    SetWindowText(state->controls[OBJECT_STATUS], text);
    ObjectPropertiesLayout(hwnd, state);
}

static void ObjectPropertiesResetHealth(ObjectPropertiesState *state)
{
    char text[64] = "";
    if (state->selected) { snprintf(text, sizeof(text), "%.15g", state->properties.health); }
    state->updating = TRUE;
    SetWindowText(state->controls[OBJECT_HEALTH], text);
    SendMessage(state->controls[OBJECT_HEALTH], EM_EMPTYUNDOBUFFER, 0, 0);
    state->updating = FALSE;
    state->edited = FALSE;
}

static BOOL ObjectPropertiesParseHealth(const char *text, double *value)
{
    char *end;
    errno = 0;
    *value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    return !*end && *value >= 0 && *value <= 2147483647.0 / 65536.0;
}

static void ObjectPropertiesApply(HWND hwnd, ObjectPropertiesState *state,
                                   SetupObjectProperty property, double value)
{
    SetupObjectPropertyEdit edit = {0};
    if (!state->selected || state->updating || state->committing) { return; }
    edit.objectindex = state->objectindex;
    edit.sourceoffset = state->properties.object.sourceoffset;
    edit.type = state->properties.object.type;
    edit.property = property;
    edit.value = value;
    edit.slot = state->ammoslot;
    state->committing = TRUE;
    SendMessage(GetParent(hwnd), OBJECTPROPERTIES_WM_CHANGED, 0, (LPARAM)&edit);
    state->committing = FALSE;
}

static BOOL ObjectPropertiesParseCctv(const ObjectPropertiesState *state, int field, const char *text, double *value)
{
    char *end;
    SetupObjectProperty property = g_CctvFields[field].property;
    errno = 0;
    *value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end || *value < g_CctvFields[field].minimum || *value > g_CctvFields[field].maximum) { return FALSE; }
    if (property == SETUP_OBJECT_CCTV_RANGE) { return floor(*value) == *value; }
    double angle = round(*value * (65536.0 / 360.0)) * (360.0 / 65536.0);
    if (property == SETUP_OBJECT_CCTV_SWEEP_MIN && angle > state->properties.cctv.sweepmax) { return FALSE; }
    if (property == SETUP_OBJECT_CCTV_SWEEP_MAX && angle < state->properties.cctv.sweepmin) { return FALSE; }
    return TRUE;
}
static void ObjectPropertiesApplyCctv(HWND hwnd, ObjectPropertiesState *state, int field)
{
    char text[64]; double value;
    if (!state->selected || state->properties.object.type != PROPDEF_CCTV
        || !state->cctvedited[field] || state->updating || state->committing) { return; }
    GetWindowText(state->controls[OBJECT_CCTV_FIRST + field * 3 + 1], text, sizeof(text));
    if (!ObjectPropertiesParseCctv(state, field, text, &value))
    {
        char message[256];
        snprintf(message, sizeof(message), "Enter %s from %.15g to %.15g.%s The camera has not changed.",
            g_CctvFields[field].name, g_CctvFields[field].minimum, g_CctvFields[field].maximum,
            field < 2 ? " Minimum must not exceed maximum." : field == 3 ? " Use whole world units." : "");
        ObjectPropertiesStatus(hwnd, state, message);
        return;
    }
    ObjectPropertiesApply(hwnd, state, g_CctvFields[field].property, value);
    ObjectPropertiesResetCctv(state, field);
}

static BOOL ObjectPropertiesParseDoor(const ObjectPropertiesState *state, int field, const char *text, double *value)
{
    char *end;
    SetupObjectProperty property = g_DoorFields[field].property;
    double limit = property == SETUP_OBJECT_DOOR_CLOSE_DELAY ? 2147483647.0 / 60.0 : 2147483647.0 / 65536.0;
    errno = 0;
    *value = strtod(text, &end);
    if (end == text || errno == ERANGE || !isfinite(*value)) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    *value /= ObjectPropertiesDoorFactor(state->properties.door.type, field);
    if (*end || *value < 0 || *value > limit) { return FALSE; }
    if ((property == SETUP_OBJECT_DOOR_ACCEL || property == SETUP_OBJECT_DOOR_DECEL)
        && floor(*value * 65536.0 + 0.5) == 0) { return FALSE; }
    return TRUE;
}
static void ObjectPropertiesApplyDoor(HWND hwnd, ObjectPropertiesState *state, int field)
{
    char text[64]; double value;
    if (!state->selected || state->properties.object.type != PROPDEF_DOOR
        || !state->dooredited[field] || state->updating || state->committing) { return; }
    GetWindowText(state->controls[OBJECT_DOOR_FIRST + field * 3 + 1], text, sizeof(text));
    if (!ObjectPropertiesParseDoor(state, field, text, &value))
    {
        SetupObjectProperty property = g_DoorFields[field].property;
        double factor = ObjectPropertiesDoorFactor(state->properties.door.type, field);
        double limit = property == SETUP_OBJECT_DOOR_CLOSE_DELAY ? 2147483647.0 / 60.0 : 2147483647.0 / 65536.0;
        double minimum = property == SETUP_OBJECT_DOOR_ACCEL || property == SETUP_OBJECT_DOOR_DECEL ? 1.0 / 65536.0 : 0;
        char message[256];
        snprintf(message, sizeof(message), "Enter %s from %.15g to %.15g in the displayed units. The door has not changed.",
            g_DoorFields[field].name, minimum * factor, limit * factor);
        ObjectPropertiesStatus(hwnd, state, message);
        return;
    }
    ObjectPropertiesApply(hwnd, state, g_DoorFields[field].property, value);
    ObjectPropertiesResetDoor(state, field);
}
static SetupObjectProperty ObjectPropertiesKeyProperty(const ObjectPropertiesState *state)
{ return state->properties.object.type == PROPDEF_DOOR ? SETUP_OBJECT_DOOR_KEY_FLAGS : SETUP_OBJECT_KEY_FLAGS; }

static void ObjectPropertiesApplyHealth(HWND hwnd, ObjectPropertiesState *state)
{
    char text[64]; double value;
    if (!state->selected || !state->edited || state->updating || state->committing) { return; }
    GetWindowText(state->controls[OBJECT_HEALTH], text, sizeof(text));
    if (!ObjectPropertiesParseHealth(text, &value))
    {
        ObjectPropertiesStatus(hwnd, state, "Enter health from 0 to 32767.99998474121. Health has not changed.");
        return;
    }
    ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_HEALTH, value);
    /* Selection refresh supplies the actual quantized value, even on a no-op. */
    ObjectPropertiesResetHealth(state);
}

static BOOL ObjectPropertiesParseUnsigned(const char *text, BOOL hex, DWORD limit, DWORD *value)
{
    char *end;
    unsigned long long parsed;
    while (isspace((unsigned char)*text)) { text++; }
    if (!*text || *text == '-' || *text == '+') { return FALSE; }
    errno = 0;
    parsed = strtoull(text, &end, hex ? 16 : 10);
    if (end == text || errno == ERANGE || parsed > limit) { return FALSE; }
    while (isspace((unsigned char)*end)) { end++; }
    if (*end) { return FALSE; }
    *value = (DWORD)parsed;
    return TRUE;
}

static void ObjectPropertiesResetExtra(ObjectPropertiesState *state, int id)
{
    char text[64] = "";
    if (state->selected)
    {
        if (id == OBJECT_KEY_MASK) { snprintf(text, sizeof(text), "0x%08lX", (unsigned long)state->properties.keyflags); }
        else { snprintf(text, sizeof(text), "%u", state->properties.ammo[state->ammoslot].quantity); }
    }
    state->updating = TRUE;
    SetWindowText(state->controls[id], text);
    SendMessage(state->controls[id], EM_EMPTYUNDOBUFFER, 0, 0);
    state->updating = FALSE;
    if (id == OBJECT_KEY_MASK) { state->keyedited = FALSE; }
    else { state->quantityedited = FALSE; }
}

static void ObjectPropertiesApplyExtra(HWND hwnd, ObjectPropertiesState *state, int id)
{
    char text[64]; DWORD value;
    BOOL key = id == OBJECT_KEY_MASK;
    if (!state->selected || state->updating || state->committing
        || (key ? !state->keyedited : !state->quantityedited)) { return; }
    GetWindowText(state->controls[id], text, sizeof(text));
    if (!ObjectPropertiesParseUnsigned(text, key, key ? 0xffffffffu : 65535, &value))
    {
        ObjectPropertiesStatus(hwnd, state, key ? "Enter a hexadecimal key mask from 00000000 to FFFFFFFF."
            : "Enter a whole quantity from 0 to 65535. Zero removes this ammo type from the contents.");
        return;
    }
    ObjectPropertiesApply(hwnd, state, key ? ObjectPropertiesKeyProperty(state) : SETUP_OBJECT_AMMO_QUANTITY, value);
    ObjectPropertiesResetExtra(state, id);
}

static int ObjectPropertiesModelChoice(HWND combo, int modelid)
{
    int count = (int)SendMessage(combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; i++)
    { if ((int)SendMessage(combo, CB_GETITEMDATA, i, 0) == modelid) { return i; } }
    return -1;
}

static void ObjectPropertiesRefreshCctv(ObjectPropertiesState *state)
{
    HWND combo = state->controls[OBJECT_CCTV_PAD];
    int choice = ObjectPropertiesModelChoice(combo, state->properties.cctv.lookpad);
    state->updating = TRUE;
    if (choice < 0)
    {
        char text[80];
        snprintf(text, sizeof(text), "Unavailable pad %ld (preserved)", (long)state->properties.cctv.lookpad);
        choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (choice >= 0) { SendMessage(combo, CB_SETITEMDATA, choice, state->properties.cctv.lookpad); }
    }
    SendMessage(combo, CB_SETCURSEL, choice, 0);
    state->updating = FALSE;
    for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
    { if (!state->cctvedited[field]) { ObjectPropertiesResetCctv(state, field); } }
}
static void ObjectPropertiesLoadCctvPads(ObjectPropertiesState *state, const SetupFile *setup, BOOL same)
{
    if (same && state->cctvpadcount == setup->padcount && state->cctvboundpadcount == setup->boundpadcount) { return; }
    HWND combo = state->controls[OBJECT_CCTV_PAD];
    state->updating = TRUE;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (int bound = 0; bound < 2; bound++)
    {
        DWORD count = bound ? setup->boundpadcount : min(setup->padcount, 10000);
        for (DWORD index = 0; index < count; index++)
        {
            char text[64];
            snprintf(text, sizeof(text), "%s %lu", bound ? "Bound pad" : "Pad", (unsigned long)index);
            int choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
            if (choice >= 0) { SendMessage(combo, CB_SETITEMDATA, choice, index + (bound ? 10000 : 0)); }
        }
    }
    state->cctvpadcount = setup->padcount; state->cctvboundpadcount = setup->boundpadcount;
    state->updating = FALSE;
}
static void ObjectPropertiesApplyCctvPad(HWND hwnd, ObjectPropertiesState *state)
{
    HWND combo = state->controls[OBJECT_CCTV_PAD];
    int choice = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    if (!state->selected || state->properties.object.type != PROPDEF_CCTV
        || state->updating || state->committing || choice < 0) { return; }
    LONG value = (LONG)SendMessage(combo, CB_GETITEMDATA, choice, 0);
    if (value != state->properties.cctv.lookpad)
    { ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_CCTV_LOOK_PAD, value); }
}

static void ObjectPropertiesDoorChoice(HWND combo, DWORD value)
{
    int choice = ObjectPropertiesModelChoice(combo, (int)value);
    if (choice < 0)
    {
        char text[64]; snprintf(text, sizeof(text), "Unsupported value %lu (preserved)", (unsigned long)value);
        choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
        if (choice >= 0) { SendMessage(combo, CB_SETITEMDATA, choice, value); }
    }
    SendMessage(combo, CB_SETCURSEL, choice, 0);
}
static void ObjectPropertiesRefreshDoor(ObjectPropertiesState *state)
{
    const SetupDoorProperties *door = &state->properties.door;
    int units = ObjectPropertiesDoorUnits(door->type);
    state->updating = TRUE;
    ObjectPropertiesDoorChoice(state->controls[OBJECT_DOOR_TYPE], door->type);
    ObjectPropertiesDoorChoice(state->controls[OBJECT_DOOR_SOUND], door->sound);
    SetWindowText(state->controls[OBJECT_DOOR_TYPE_HELP], units == 0
        ? "Travel is a percentage of the door's width (height for vertical / fall-away doors). Special folding types need compatible models."
        : units == 1 ? "Travel is an angle in degrees. The model and pad determine the hinge."
        : "Travel uses the model's animation units. This movement needs a compatible model.");
    for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
    {
        SetupObjectProperty property = g_DoorFields[field].property;
        const char *unit = units == 0 ? "%" : units == 1 ? "degrees" : "units";
        const char *rate = property == SETUP_OBJECT_DOOR_SPEED ? "/s"
            : property == SETUP_OBJECT_DOOR_ACCEL || property == SETUP_OBJECT_DOOR_DECEL ? "/s squared" : "";
        char text[96];
        if (property == SETUP_OBJECT_DOOR_CLOSE_DELAY) { snprintf(text, sizeof(text), "%s", g_DoorFields[field].name); }
        else { snprintf(text, sizeof(text), "%s (%s%s)", g_DoorFields[field].name, unit, rate); }
        SetWindowText(state->controls[OBJECT_DOOR_FIRST + field * 3], text);
    }
    for (int flag = 0; flag < 4; flag++)
    { SendMessage(state->controls[OBJECT_DOOR_FLAG_FIRST + flag], BM_SETCHECK,
        door->flags & g_DoorFlags[flag].bit ? BST_CHECKED : BST_UNCHECKED, 0); }
    state->updating = FALSE;
    for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
    { if (!state->dooredited[field]) { ObjectPropertiesResetDoor(state, field); } }
}
static void ObjectPropertiesApplyDoorChoice(HWND hwnd, ObjectPropertiesState *state, int id)
{
    HWND combo = state->controls[id];
    int choice = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    DWORD value, current = id == OBJECT_DOOR_TYPE ? state->properties.door.type : state->properties.door.sound;
    if (!state->selected || state->properties.object.type != PROPDEF_DOOR
        || state->updating || state->committing || choice < 0) { return; }
    value = (DWORD)SendMessage(combo, CB_GETITEMDATA, choice, 0);
    if (value != current)
    { ObjectPropertiesApply(hwnd, state, id == OBJECT_DOOR_TYPE ? SETUP_OBJECT_DOOR_TYPE : SETUP_OBJECT_DOOR_SOUND, value); }
}

static void ObjectPropertiesApplyModel(HWND hwnd, ObjectPropertiesState *state)
{
    HWND combo = state->controls[OBJECT_MODEL];
    int choice = (int)SendMessage(combo, CB_GETCURSEL, 0, 0), model;
    if (choice == CB_ERR || !state->selected) { return; }
    model = (int)SendMessage(combo, CB_GETITEMDATA, choice, 0);
    if (model != state->properties.object.modelid)
    { ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_MODEL, model); }
}

/* Match propobj.c: multiply each slot's base quantity and truncate. */
static BOOL ObjectPropertiesFormatContents(const SetupObjectProperties *properties, BOOL multiplayer, char *text, size_t capacity)
{
    static const float multipliers[] = {
        DEFAULT_AGENT_SOLO_AMMO_MULTIPLIER, DEFAULT_SECRET_AGENT_SOLO_AMMO_MULTIPLIER,
        DEFAULT_00_AGENT_SOLO_AMMO_MULTIPLIER, DEFAULT_007_SOLO_AMMO_MULTIPLIER
    };
    size_t used;
    BOOL any = FALSE;
    int length;
    if (!text || !capacity) { return FALSE; }
    length = snprintf(text, capacity, multiplayer ? "Contents (multiplayer setup amounts):" : "Contents (pickup amounts):");
    if (length < 0 || (size_t)length >= capacity) { return FALSE; }
    used = (size_t)length;
    for (DWORD ammo = 1; ammo <= AMMOTYPE_GLOBAL_MAX; ammo++)
    {
        DWORD base = properties->ammo[ammo - 1].quantity;
        DWORD amount[4];
        if (!base) { continue; }
        any = TRUE;
        for (int difficulty = 0; difficulty < 4; difficulty++)
        {
            amount[difficulty] = (DWORD)((float)base * multipliers[difficulty]);
        }
        if (multiplayer)
        { length = snprintf(text + used, capacity - used, "\r\n%s: %lu", g_AmmoNames[ammo], (unsigned long)base); }
        else
        {
            length = snprintf(text + used, capacity - used,
                "\r\n\r\n%s\r\nAgent: %lu\r\nSecret Agent: %lu\r\n00 Agent: %lu\r\n007 Mode: %lu",
                g_AmmoNames[ammo], (unsigned long)amount[0], (unsigned long)amount[1],
                (unsigned long)amount[2], (unsigned long)amount[3]);
        }

        if (length < 0 || (size_t)length >= capacity - used) 
        { 
            return FALSE; 
        }

        used += (size_t)length;
    }

    if (any) { return TRUE; }
    length = snprintf(text + used, capacity - used, "\r\nEmpty");

    return length >= 0 && (size_t)length < capacity - used;
}

static void ObjectPropertiesRefreshAmmo(ObjectPropertiesState *state)
{
    char text[OBJECT_CONTENTS_TEXT_MAX];
    BOOL crate = state->properties.object.type == PROPDEF_AMMO;
    state->updating = TRUE;
    SendMessage(state->controls[OBJECT_AMMO_TYPE], CB_SETCURSEL,
        ObjectPropertiesModelChoice(state->controls[OBJECT_AMMO_TYPE],
            crate ? (int)state->ammoslot + 1 : (int)state->properties.ammotype), 0);
    if (crate)
    {
        if (!ObjectPropertiesFormatContents(&state->properties, state->multiplayer, text, sizeof(text)))
        { strcpy(text, "Contents could not be displayed."); }
        SetWindowText(state->controls[OBJECT_CONTENTS], text);
    }
    state->updating = FALSE;
    if (!state->quantityedited) { ObjectPropertiesResetExtra(state, OBJECT_QUANTITY); }
}

static void ObjectPropertiesApplyAmmoChoice(HWND hwnd, ObjectPropertiesState *state, int id)
{
    HWND combo = state->controls[id];
    int choice = (int)SendMessage(combo, CB_GETCURSEL, 0, 0);
    DWORD value;
    if (!state->selected || state->updating || state->committing || choice < 0) { return; }
    value = (DWORD)SendMessage(combo, CB_GETITEMDATA, choice, 0);
    if (state->properties.object.type == PROPDEF_MAGAZINE)
    { ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_AMMO_TYPE, value); }
    else if (value >= 1 && value <= AMMOTYPE_GLOBAL_MAX && state->ammoslot != value - 1)
    {
        /* Choosing which slot to inspect is UI state, not an authored change. */
        state->ammoslot = value - 1;
        state->quantityedited = FALSE;
        ObjectPropertiesRefreshAmmo(state);
        ObjectPropertiesStatus(hwnd, state, "Enter or leave a field to apply. Escape cancels typing.");
    }
}

static BOOL ObjectPropertiesLoadModels(ObjectPropertiesState *state, const char *projectdir)
{
    HWND combo = state->controls[OBJECT_MODEL];
    if (!projectdir) { projectdir = ""; }
    if (!lstrcmpi(projectdir, state->projectdir)) { return TRUE; }
    state->updating = TRUE;
    SendMessage(combo, CB_RESETCONTENT, 0, 0);
    for (int model = 0; *projectdir; model++)
    {
        const char *name;
        char path[MAX_PATH];
        DWORD attributes;
        int choice;
        if (!ModelGetPropDefinition(model, &name, NULL)) { break; }
        if (!name || !*name) { continue; }
        if (snprintf(path, sizeof(path), "%s\\models\\objects\\%s.gltf", projectdir, name) >= (int)sizeof(path)) { continue; }
        attributes = GetFileAttributes(path);
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) { continue; }
        choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)name);
        if (choice < 0) { state->updating = FALSE; state->projectdir[0] = '\0'; return FALSE; }
        SendMessage(combo, CB_SETITEMDATA, choice, model);
    }
    lstrcpyn(state->projectdir, projectdir, sizeof(state->projectdir));
    state->updating = FALSE;
    return TRUE;
}

static void ObjectPropertiesRevealControl(HWND hwnd, ObjectPropertiesState *state, HWND control)
{
    RECT client, rect;
    GetClientRect(hwnd, &client);
    GetWindowRect(control, &rect); MapWindowPoints(NULL, hwnd, (POINT *)&rect, 2);
    if (rect.top < 0) { state->scroll += rect.top - 4; }
    else if (rect.bottom > client.bottom) { state->scroll += rect.bottom - client.bottom + 4; }
    else { return; }
    ObjectPropertiesLayout(hwnd, state);
}

static LRESULT CALLBACK ObjectPropertiesWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(hwnd);
    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCT *cs = (CREATESTRUCT *)lparam;
        HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        state = calloc(1, sizeof(*state));
        if (!state) { return -1; }
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
        {
            BOOL combo = ObjectPropertiesIsCombo(i), edit = ObjectPropertiesIsEdit(i);
            BOOL key = (i >= OBJECT_KEY_FIRST && i <= OBJECT_KEY_LAST)
                || (i >= OBJECT_DOOR_FLAG_FIRST && i <= OBJECT_DOOR_FLAG_LAST);
            state->controls[i] = CreateWindowEx(edit ? WS_EX_CLIENTEDGE : 0,
                combo ? "COMBOBOX" : edit ? "EDIT" : key ? "BUTTON" : "STATIC", "",
                WS_CHILD | WS_VISIBLE | (combo ? WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL
                    : edit ? WS_TABSTOP | ES_AUTOHSCROLL : key ? WS_TABSTOP | BS_AUTOCHECKBOX | BS_MULTILINE : SS_NOPREFIX),
                0, 0, 1, 1, hwnd, (HMENU)(INT_PTR)(100 + i), cs->hInstance, NULL);
            if (!state->controls[i]) { return -1; }
            SendMessage(state->controls[i], WM_SETFONT, (WPARAM)font, FALSE);
        }
        for (unsigned int i = 0; i < sizeof(g_SharedProperties) / sizeof(g_SharedProperties[0]); i++)
        {
            SetWindowText(state->controls[g_SharedProperties[i].label], g_SharedProperties[i].name);
            SetWindowText(state->controls[g_SharedProperties[i].help], g_SharedProperties[i].description);
        }
        for (int i = 0; i < OBJECT_CONTROL_COUNT; i++)
        {
            if (ObjectPropertiesIsCombo(i)) { SendMessage(state->controls[i], CB_SETDROPPEDWIDTH, 360, 0); }
            if (ObjectPropertiesIsEdit(i)) { SendMessage(state->controls[i], EM_SETLIMITTEXT, 63, 0); }
        }
        SetWindowText(state->controls[OBJECT_DOOR_TYPE_LABEL], "Door movement");
        SetWindowText(state->controls[OBJECT_CCTV_PAD_LABEL], "Look-at pad");
        SetWindowText(state->controls[OBJECT_CCTV_PAD_HELP], "Sets the camera head's base aim, including its tilt. This is separate from the pad that places the camera body.");
        for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
        {
            SetWindowText(state->controls[OBJECT_CCTV_FIRST + field * 3], g_CctvFields[field].name);
            SetWindowText(state->controls[OBJECT_CCTV_FIRST + field * 3 + 2], g_CctvFields[field].help);
        }
        SetWindowText(state->controls[OBJECT_DOOR_SOUND_LABEL], "Door sounds");
        SetWindowText(state->controls[OBJECT_DOOR_SOUND_HELP], "Preset for opening, moving and closing sounds.");
        SetWindowText(state->controls[OBJECT_DOOR_FLAGS_LABEL], "Door flags");
        SetWindowText(state->controls[OBJECT_DOOR_FLAGS_HELP], "Extended collision increases the vertical bounds. Mesh clipping requires a compatible sliding model. Windowed uses the door's existing fade distances.");
        for (int i = 0; i <= DOORTYPE_AZTECCHAIR; i++)
        {
            int choice = (int)SendMessage(state->controls[OBJECT_DOOR_TYPE], CB_ADDSTRING, 0, (LPARAM)g_DoorTypes[i]);
            if (choice >= 0) { SendMessage(state->controls[OBJECT_DOOR_TYPE], CB_SETITEMDATA, choice, i); }
        }
        for (int i = 0; i <= DOOR_OPEN_SOUND_METAL_4; i++)
        {
            char text[96]; snprintf(text, sizeof(text), "%d - %s", i, g_DoorSounds[i]);
            int choice = (int)SendMessage(state->controls[OBJECT_DOOR_SOUND], CB_ADDSTRING, 0, (LPARAM)text);
            if (choice >= 0) { SendMessage(state->controls[OBJECT_DOOR_SOUND], CB_SETITEMDATA, choice, i); }
        }
        for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
        { SetWindowText(state->controls[OBJECT_DOOR_FIRST + field * 3 + 2], g_DoorFields[field].help); }
        for (int flag = 0; flag < 4; flag++)
        { SetWindowText(state->controls[OBJECT_DOOR_FLAG_FIRST + flag], g_DoorFlags[flag].name); }
        SetWindowText(state->controls[OBJECT_KEY_LABEL], "Unlock flags (hexadecimal mask)");
        SetWindowText(state->controls[OBJECT_KEY_HELP], "Choose the flags this key supplies. Doors require all their flags, which can come from several keys. Numbers below are flag positions, not door IDs.");
        for (int bit = 0; bit < 32; bit++)
        {
            char text[16]; snprintf(text, sizeof(text), "%d", bit + 1);
            SetWindowText(state->controls[OBJECT_KEY_FIRST + bit], text);
        }
        SetWindowText(state->controls[OBJECT_QUANTITY_LABEL], "Base quantity (0 removes this ammo type)");
        return 0;
    }
    case WM_SIZE:
        if (state) { ObjectPropertiesLayout(hwnd, state); }
        return 0;
    case WM_COMMAND:
        if (!state || state->updating) { return 0; }
        if ((HWND)lparam == state->controls[OBJECT_HEALTH])
        {
            if (HIWORD(wparam) == EN_CHANGE) { state->edited = TRUE; }
            if (HIWORD(wparam) == EN_KILLFOCUS) { ObjectPropertiesApplyHealth(hwnd, state); }
            if (HIWORD(wparam) == EN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
        {
            if ((HWND)lparam != state->controls[OBJECT_CCTV_FIRST + field * 3 + 1]) { continue; }
            if (HIWORD(wparam) == EN_CHANGE) { state->cctvedited[field] = TRUE; }
            if (HIWORD(wparam) == EN_KILLFOCUS) { ObjectPropertiesApplyCctv(hwnd, state, field); }
            if (HIWORD(wparam) == EN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        if ((HWND)lparam == state->controls[OBJECT_CCTV_PAD])
        {
            if (HIWORD(wparam) == CBN_SELENDOK || (HIWORD(wparam) == CBN_SELCHANGE
                && !SendMessage((HWND)lparam, CB_GETDROPPEDSTATE, 0, 0)))
            { ObjectPropertiesApplyCctvPad(hwnd, state); }
            if (HIWORD(wparam) == CBN_SELENDCANCEL) { ObjectPropertiesRefreshCctv(state); }
            if (HIWORD(wparam) == CBN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
        {
            if ((HWND)lparam != state->controls[OBJECT_DOOR_FIRST + field * 3 + 1]) { continue; }
            if (HIWORD(wparam) == EN_CHANGE) { state->dooredited[field] = TRUE; }
            if (HIWORD(wparam) == EN_KILLFOCUS) { ObjectPropertiesApplyDoor(hwnd, state, field); }
            if (HIWORD(wparam) == EN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        for (int which = 0; which < 2; which++)
        {
            int id = which ? OBJECT_DOOR_SOUND : OBJECT_DOOR_TYPE;
            if ((HWND)lparam != state->controls[id]) { continue; }
            if (HIWORD(wparam) == CBN_SELENDOK || (HIWORD(wparam) == CBN_SELCHANGE
                && !SendMessage((HWND)lparam, CB_GETDROPPEDSTATE, 0, 0)))
            { ObjectPropertiesApplyDoorChoice(hwnd, state, id); }
            if (HIWORD(wparam) == CBN_SELENDCANCEL) { ObjectPropertiesRefreshDoor(state); }
            if (HIWORD(wparam) == CBN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        if (LOWORD(wparam) >= 100 + OBJECT_DOOR_FLAG_FIRST && LOWORD(wparam) <= 100 + OBJECT_DOOR_FLAG_LAST
            && HIWORD(wparam) == BN_CLICKED && state->properties.object.type == PROPDEF_DOOR)
        {
            DWORD mask = state->properties.door.flags ^ g_DoorFlags[LOWORD(wparam) - 100 - OBJECT_DOOR_FLAG_FIRST].bit;
            ObjectPropertiesApply(hwnd, state, SETUP_OBJECT_DOOR_FLAGS, mask);
        }
        for (int id = OBJECT_KEY_MASK; id <= OBJECT_QUANTITY; id++)
        {
            if (id != OBJECT_KEY_MASK && id != OBJECT_QUANTITY) { continue; }
            if ((HWND)lparam != state->controls[id]) { continue; }
            if (HIWORD(wparam) == EN_CHANGE)
            { if (id == OBJECT_KEY_MASK) { state->keyedited = TRUE; } else { state->quantityedited = TRUE; } }
            if (HIWORD(wparam) == EN_KILLFOCUS) { ObjectPropertiesApplyExtra(hwnd, state, id); }
            if (HIWORD(wparam) == EN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        if (LOWORD(wparam) >= 100 + OBJECT_KEY_FIRST && LOWORD(wparam) <= 100 + OBJECT_KEY_LAST
            && HIWORD(wparam) == BN_CLICKED
            && (state->properties.object.type == PROPDEF_KEY || state->properties.object.type == PROPDEF_DOOR))
        {
            DWORD mask = state->properties.keyflags ^ ((DWORD)1 << (LOWORD(wparam) - 100 - OBJECT_KEY_FIRST));
            ObjectPropertiesApply(hwnd, state, ObjectPropertiesKeyProperty(state), mask);
            ObjectPropertiesResetExtra(state, OBJECT_KEY_MASK);
        }
        if ((HWND)lparam == state->controls[OBJECT_AMMO_TYPE])
        {
            if (HIWORD(wparam) == CBN_SELENDOK || (HIWORD(wparam) == CBN_SELCHANGE
                && !SendMessage((HWND)lparam, CB_GETDROPPEDSTATE, 0, 0)))
            { ObjectPropertiesApplyAmmoChoice(hwnd, state, OBJECT_AMMO_TYPE); }
            if (HIWORD(wparam) == CBN_SELENDCANCEL) { ObjectPropertiesRefreshAmmo(state); }
            if (HIWORD(wparam) == CBN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        if ((HWND)lparam == state->controls[OBJECT_MODEL])
        {
            if (HIWORD(wparam) == CBN_SELENDOK || (HIWORD(wparam) == CBN_SELCHANGE
                && !SendMessage((HWND)lparam, CB_GETDROPPEDSTATE, 0, 0)))
            { ObjectPropertiesApplyModel(hwnd, state); }
            if (HIWORD(wparam) == CBN_SELENDCANCEL)
            { SendMessage((HWND)lparam, CB_SETCURSEL, ObjectPropertiesModelChoice((HWND)lparam, state->properties.object.modelid), 0); }
            if (HIWORD(wparam) == CBN_SETFOCUS) { ObjectPropertiesRevealControl(hwnd, state, (HWND)lparam); }
        }
        return 0;
    case WM_VSCROLL:
        if (state)
        {
            SCROLLINFO info = {0};
            info.cbSize = sizeof(info); info.fMask = SIF_ALL;
            GetScrollInfo(hwnd, SB_VERT, &info);
            switch (LOWORD(wparam))
            {
            case SB_LINEUP: state->scroll -= 24; break;
            case SB_LINEDOWN: state->scroll += 24; break;
            case SB_PAGEUP: state->scroll -= (int)info.nPage; break;
            case SB_PAGEDOWN: state->scroll += (int)info.nPage; break;
            case SB_TOP: state->scroll = 0; break;
            case SB_BOTTOM: state->scroll = info.nMax; break;
            case SB_THUMBTRACK: case SB_THUMBPOSITION: state->scroll = info.nTrackPos; break;
            }
            ObjectPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (state)
        {
            state->wheelremainder += GET_WHEEL_DELTA_WPARAM(wparam);
            state->scroll -= 72 * (state->wheelremainder / WHEEL_DELTA);
            state->wheelremainder %= WHEEL_DELTA;
            ObjectPropertiesLayout(hwnd, state);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkColor((HDC)wparam, GetSysColor(COLOR_WINDOW));
        SetBkMode((HDC)wparam, TRANSPARENT);
        if (state && (HWND)lparam == state->controls[OBJECT_STATUS])
        { SetTextColor((HDC)wparam, GetSysColor(COLOR_GRAYTEXT)); }
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_NCDESTROY:
        free(state); SetWindowLongPtr(hwnd, GWLP_USERDATA, 0); break;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

BOOL ObjectPropertiesRegisterClass(HINSTANCE instance)
{
    WNDCLASS wc = {0};
    wc.lpfnWndProc = ObjectPropertiesWndProc; wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszClassName = OBJECTPROPERTIES_CLASS;
    return RegisterClass(&wc) != 0;
}

HWND ObjectPropertiesCreate(HWND parent, HINSTANCE instance)
{
    return CreateWindowEx(WS_EX_CONTROLPARENT, OBJECTPROPERTIES_CLASS, NULL,
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 1, 1, parent, NULL, instance, NULL);
}

BOOL ObjectPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index, const char *projectdir)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(panel);
    SetupObjectProperties properties;
    const char *why;
    char text[512], placement[64];
    BOOL same;
    int choice;
    if (!state) { return FALSE; }
    if (!setup || !SetupFileGetObjectProperties(setup, index, &properties, &why))
    {
        state->selected = FALSE; state->edited = FALSE; state->document = 0;
        state->keyedited = state->quantityedited = FALSE;
        memset(state->dooredited, 0, sizeof(state->dooredited));
        memset(state->cctvedited, 0, sizeof(state->cctvedited));
        ObjectPropertiesResetHealth(state);
        return FALSE;
    }
    same = state->selected && state->objectindex == index && state->document == (ULONG_PTR)setup->data
        && state->properties.object.sourceoffset == properties.object.sourceoffset
        && state->properties.object.type == properties.object.type;
    if (!same)
    {
        state->scroll = 0; state->edited = state->keyedited = state->quantityedited = FALSE;
        state->ammoslot = 0;
        for (DWORD slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++)
        { if (properties.ammo[slot].quantity) { state->ammoslot = slot; break; } }
    }
    if (state->properties.keyflags != properties.keyflags) { state->keyedited = FALSE; }
    if (state->properties.ammo[state->ammoslot].quantity != properties.ammo[state->ammoslot].quantity)
    { state->quantityedited = FALSE; }
    if (state->properties.health != properties.health) { state->edited = FALSE; }
    for (int field = 0; field < OBJECT_CCTV_FIELD_COUNT; field++)
    {
        if (!same || ObjectPropertiesCctvValue(&state->properties.cctv, field) != ObjectPropertiesCctvValue(&properties.cctv, field))
        { state->cctvedited[field] = FALSE; }
    }
    for (int field = 0; field < OBJECT_DOOR_FIELD_COUNT; field++)
    {
        if (!same || state->properties.door.type != properties.door.type
            || ObjectPropertiesDoorValue(&state->properties.door, field) != ObjectPropertiesDoorValue(&properties.door, field))
        { state->dooredited[field] = FALSE; }
    }
    state->selected = TRUE; state->objectindex = index; state->document = (ULONG_PTR)setup->data;
    state->properties = properties;
    state->multiplayer = strncmp(setup->name, "Ump_", 4) == 0;
    if (!ObjectPropertiesLoadModels(state, projectdir)) { return FALSE; }
    snprintf(text, sizeof(text), "Type: %s", SetupObjectTypeName(properties.object.type));
    SetWindowText(state->controls[OBJECT_TYPE], text);
    state->updating = TRUE;
    choice = ObjectPropertiesModelChoice(state->controls[OBJECT_MODEL], properties.object.modelid);
    if (choice < 0)
    {
        const char *name = NULL;
        ModelGetPropDefinition(properties.object.modelid, &name, NULL);
        if (name) { snprintf(text, sizeof(text), "%s (unavailable)", name); }
        else { snprintf(text, sizeof(text), "Unknown model %d", properties.object.modelid); }
        choice = (int)SendMessage(state->controls[OBJECT_MODEL], CB_ADDSTRING, 0, (LPARAM)text);
        if (choice >= 0) { SendMessage(state->controls[OBJECT_MODEL], CB_SETITEMDATA, choice, properties.object.modelid); }
    }
    SendMessage(state->controls[OBJECT_MODEL], CB_SETCURSEL, choice, 0);
    state->updating = FALSE;
    if (properties.object.type == PROPDEF_DOOR) { ObjectPropertiesRefreshDoor(state); }
    if (properties.object.type == PROPDEF_CCTV)
    {
        ObjectPropertiesLoadCctvPads(state, setup, same);
        ObjectPropertiesRefreshCctv(state);
    }
    SetWindowText(state->controls[OBJECT_KEY_LABEL], properties.object.type == PROPDEF_DOOR
        ? "Required keys (hexadecimal mask)" : "Unlock flags (hexadecimal mask)");
    SetWindowText(state->controls[OBJECT_KEY_HELP], properties.object.type == PROPDEF_DOOR
        ? "0 needs no key. The player needs all checked flags, supplied by one or more keys. Numbers are flag positions, not key IDs."
        : "Choose the flags this key supplies. Doors require all their flags, which can come from several keys. Numbers below are flag positions, not door IDs.");
    if (!state->edited) { ObjectPropertiesResetHealth(state); }
    if (!state->keyedited) { ObjectPropertiesResetExtra(state, OBJECT_KEY_MASK); }
    state->updating = TRUE;
    for (int bit = 0; bit < 32; bit++)
    { SendMessage(state->controls[OBJECT_KEY_FIRST + bit], BM_SETCHECK, properties.keyflags & ((DWORD)1 << bit) ? BST_CHECKED : BST_UNCHECKED, 0); }
    BOOL crate = properties.object.type == PROPDEF_AMMO;
    if (!same)
    {
        HWND combo = state->controls[OBJECT_AMMO_TYPE];
        SendMessage(combo, CB_RESETCONTENT, 0, 0);
        for (int ammo = crate ? 1 : 0; ammo < (crate ? AMMOTYPE_GLOBAL_MAX + 1 : AMMOTYPE_MAX); ammo++)
        {
            choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)g_AmmoNames[ammo]);
            if (choice >= 0) { SendMessage(combo, CB_SETITEMDATA, choice, ammo); }
        }
        if (!crate && properties.ammotype >= AMMOTYPE_MAX)
        {
            snprintf(text, sizeof(text), "Unknown ammo ID %lu", (unsigned long)properties.ammotype);
            choice = (int)SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)text);
            if (choice >= 0) { SendMessage(combo, CB_SETITEMDATA, choice, properties.ammotype); }
        }
    }
    SetWindowText(state->controls[OBJECT_AMMO_LABEL], crate ? "Edit ammo slot" : "Ammo type");
    SetWindowText(state->controls[OBJECT_AMMO_HELP], crate
        ? (state->multiplayer
            ? "Each type has its own slot. Multiplayer can override quantities using the chosen weapon set."
            : "Each type has its own slot. Contents shows pickup amounts for each difficulty.")
        : "Pickup quantity is set by the game for this ammo type, with a solo multiplier where applicable.");
    state->updating = FALSE;
    ObjectPropertiesRefreshAmmo(state);
    if (properties.object.flags & PROPFLAG_ASSIGNEDTOCHR)
    { snprintf(placement, sizeof(placement), "Character: %d", properties.object.pad); }
    else if (properties.object.pad < 0)
    { snprintf(placement, sizeof(placement), "Placement reference: %d", properties.object.pad); }
    else if (properties.object.type == PROPDEF_DOOR || properties.object.pad >= 10000)
    { snprintf(placement, sizeof(placement), "Bound pad: %d", properties.object.pad >= 10000 ? properties.object.pad - 10000 : properties.object.pad); }
    else { snprintf(placement, sizeof(placement), "Pad: %d", properties.object.pad); }
    snprintf(text, sizeof(text), "Object index: %lu\r\n%s\r\nExtra scale: %.6g",
        (unsigned long)index, placement, properties.object.extrascale / 256.0);
    SetWindowText(state->controls[OBJECT_IDENTITY], text);
    if (!state->edited && !state->keyedited && !state->quantityedited
        && !ObjectPropertiesDoorPending(state) && !ObjectPropertiesCctvPending(state))
    { SetWindowText(state->controls[OBJECT_STATUS], "Enter or leave a field to apply. Escape cancels typing."); }
    ObjectPropertiesLayout(panel, state);
    return TRUE;
}

BOOL ObjectPropertiesHandleMessage(HWND panel, MSG *message)
{
    ObjectPropertiesState *state = ObjectPropertiesGetState(panel);
    HWND focus = GetFocus();
    int id;
    if (!state || !state->selected || message->message != WM_KEYDOWN) { return FALSE; }
    for (id = 0; id < OBJECT_CONTROL_COUNT; id++)
    { if (ObjectPropertiesIsEdit(id) && focus == state->controls[id]) { break; } }
    if (id == OBJECT_CONTROL_COUNT || !ObjectPropertiesControlVisible(state, id)) { return FALSE; }
    if (message->wParam == VK_RETURN)
    {
        if (id == OBJECT_HEALTH) { ObjectPropertiesApplyHealth(panel, state); }
        else if (ObjectPropertiesCctvField(id) >= 0) { ObjectPropertiesApplyCctv(panel, state, ObjectPropertiesCctvField(id)); }
        else if (ObjectPropertiesDoorField(id) >= 0) { ObjectPropertiesApplyDoor(panel, state, ObjectPropertiesDoorField(id)); }
        else { ObjectPropertiesApplyExtra(panel, state, id); }
        return TRUE;
    }
    if (message->wParam == VK_ESCAPE)
    {
        if (id == OBJECT_HEALTH) { ObjectPropertiesResetHealth(state); }
        else if (ObjectPropertiesCctvField(id) >= 0) { ObjectPropertiesResetCctv(state, ObjectPropertiesCctvField(id)); }
        else if (ObjectPropertiesDoorField(id) >= 0) { ObjectPropertiesResetDoor(state, ObjectPropertiesDoorField(id)); }
        else { ObjectPropertiesResetExtra(state, id); }
        ObjectPropertiesStatus(panel, state, "Enter or leave a field to apply. Escape cancels typing.");
        return TRUE;
    }
    if (message->wParam == 'Z' && (GetKeyState(VK_CONTROL) & 0x8000)
        && SendMessage(focus, EM_CANUNDO, 0, 0))
    { SendMessage(focus, WM_UNDO, 0, 0); return TRUE; }
    return FALSE;
}
