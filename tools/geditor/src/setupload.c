#include "setupmeta.h"
/*
 * GEditor setup-file extraction and project loading.
 *
 * GUD stores setup binaries as ordinary obseg resources whose table
 * names omit the on-disk extension. A project gives those raw binaries
 * the .set extension used by the editor; their contents are otherwise
 * copied byte-for-byte.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <src/propconstants.h>

#include "setupload.h"
#include "modelload.h"
#include "actionblocks.h"

#define SETUP_FILE_MAX (16u * 1024u * 1024u)
#define SETUP_HEADER_SIZE       40u
#define SETUP_OBJECT_POINTER     12u
#define SETUP_PAD_POINTER       24u
#define SETUP_BOUNDPAD_POINTER  28u
#define SETUP_PAD_SIZE          44u
#define SETUP_BOUNDPAD_SIZE     68u
#define SETUP_PAD_LINK          36u
#define SETUP_BOUNDPAD_BBOX     44u
#define SETUP_PAD_MAX        65536u
#define SETUP_OBJECT_MAX     65536u

#define SETUP_PROP_END 48u

/* GoldenEye tests one of these bits for the selected single-player
 * difficulty or the multiplayer mode before creating an object. Setting
 * all five makes a command an inert tombstone while preserving its byte
 * size and index for relative setup references. */
#define SETUP_OBJECT_DELETED_FLAGS2 0x000000f8u

/* Guards have no spawn-exclusion flags. An unreferenced CameraPos command
 * occupies the same seven words and creates no entity in the game. Its
 * header is otherwise unused. "GED" identifies our deleted-guard payload,
 * retaining character IDs and command indices through saving and reparsing.
 * The game only converts its unused camera coordinates during setup load. */
#define SETUP_DELETED_CHARACTER_HEADER (0x47454400u | PROPDEF_CAMERAPOS)

/* "GEPD" in the runtime stan-pointer slot. setupLoadFiles overwrites that
 * slot through padAssignStanTile before using it. Keep plink and the slot
 * itself intact: a null plink would terminate the table and renumbering
 * would invalidate script, navigation and setup references. */
#define SETUP_DELETED_PAD_STAN 0x47455044u
/* Editor-owned placement pad. The game replaces this runtime stan pointer.
 * Unlike an authored standalone pad, it can be recycled once unreferenced. */
#define SETUP_PRIVATE_PAD_STAN 0x47455050u

typedef struct SetupScriptReferences {
    DWORD owners;
    unsigned char values[65536]; /* bit 0: pad, bit 1: character */
} SetupScriptReferences;

static DWORD SetupRead32(const unsigned char *p)
{
    return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16)
         | ((DWORD)p[2] << 8)  |  (DWORD)p[3];
}

static short SetupRead16(const unsigned char *p)
{
    return (short)(((unsigned int)p[0] << 8) | p[1]);
}

static void SetupWrite32(unsigned char *p, DWORD value)
{
    p[0] = (unsigned char)(value >> 24);
    p[1] = (unsigned char)(value >> 16);
    p[2] = (unsigned char)(value >> 8);
    p[3] = (unsigned char)value;
}

/* Cardinal rotations leave trig/round-trip residues instead of exact zero.
 * Repeated transforms can turn these into subnormal float operands, which
 * trap on the R4300 even with underflow flushing enabled. Direction vectors
 * are unit-sized; this tolerance is far below the editor's angular precision.
 * Do not apply it to positions, bounds, scale, or arbitrary setup words. */
static float SetupPadDirectionValue(float value)
{
    return fabsf(value) < 1.0e-12f ? 0.0f : value;
}

static void SetupCleanPadDirections(SetupPad *pad)
{
    for (int axis = 0; axis < 3; axis++)
    {
        pad->up[axis] = SetupPadDirectionValue(pad->up[axis]);
        pad->look[axis] = SetupPadDirectionValue(pad->look[axis]);
    }
}

/* Called only after native reachability validation and pointer relocation.
 * Load, save, history commits, and ROM export all use this packed copy, so
 * previously saved projects are repaired without requiring another rotation. */
static void SetupCleanNativePadDirections(unsigned char *data)
{
    for (DWORD root = 6; root <= 7; root++)
    {
        DWORD at = SetupRead32(data + root * 4);
        DWORD stride = root == 6 ? SETUP_PAD_SIZE : SETUP_BOUNDPAD_SIZE;
        if (!at) { continue; }
        for (; SetupRead32(data + at + SETUP_PAD_LINK); at += stride)
        {
            for (DWORD field = 12; field < 36; field += 4)
            {
                union { DWORD u; float f; } value;
                value.u = SetupRead32(data + at + field);
                value.f = SetupPadDirectionValue(value.f);
                SetupWrite32(data + at + field, value.u);
            }
        }
    }
}

/* Setup commands are variable length. These are their encoded source
   sizes; they must not use host sizeof because a 64-bit editor has
   different pointer sizes from the N64. */
static DWORD SetupObjectWordCount(unsigned char type)
{
    switch (type)
    {
    case 1:  return 64;  /* DoorRecord */
    case 2:  return 2;   /* GlobalDoorScaleRecord */
    case 3:  return 32;  /* ObjectRecord */
    case 4:  return 33;  /* KeyRecord */
    case 5:  return 32;
    case 6:  return 0x3b;
    case 7:  return 0x21;
    case 8:  return 0x22;
    case 9:  return 7;
    case 10: return 0x40;
    case 11: return 0x95;
    case 12: return 32;
    case 13: return 0x36;
    case 14: return 3;
    case 17: return 32;
    case 18: return 3;
    case 19: return 4;
    case 20: return 32 + AMMOTYPE_GLOBAL_MAX; /* MultiAmmoCrateRecord */
    case 21: return 0x22;
    case 22: return 4;
    case 23: return 4;
    case 24: return 1;
    case 25: return 2;
    case 26: return 2;
    case 27: return 2;
    case 28: return 2;
    case 29: return 2;
    case 30: return 4;
    case 31: return 1;
    case 32: return 4;
    case 33: return 5;
    case 34: return 3;   /* ObjectiveCopyItemRecord (three source words) */
    case 35: return 4;
    case 36: return 32;
    case 37: return 10;
    case 38: return 4;
    case 39: return 0x2c;
    case 40: return 0x2d;
    case 42: return 32;
    case 43: return 32;
    case 44: return 5;
    case 45: return 0x38;
    case 46: return 7;
    case 47: return 37;
    default: return 1;   /* PropDefHeaderRecord and unused types */
    }
}

BOOL SetupObjectRelativeTarget(const SetupFile *setup, DWORD sourceoffset, LONG relative, DWORD *objectindex)
{
    DWORD at, command = 0, source = (DWORD)-1, target, i;
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE) { return FALSE; }
    at = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    if (!at) { return FALSE; }
    while (at <= setup->size - 4)
    {
        DWORD bytes = SetupObjectWordCount(setup->data[at + 3]) * 4;
        if (setup->data[at + 3] == SETUP_PROP_END || bytes > setup->size - at) { break; }
        if (at == sourceoffset) { source = command; break; }
        at += bytes; command++;
    }
    if (source == (DWORD)-1 || (long long)source + relative < 0
        || (long long)source + relative > SETUP_OBJECT_MAX) { return FALSE; }
    target = source + relative;
    at = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    for (command = 0; at <= setup->size - 4; command++)
    {
        DWORD bytes = SetupObjectWordCount(setup->data[at + 3]) * 4;
        if (setup->data[at + 3] == SETUP_PROP_END || bytes > setup->size - at) { return FALSE; }
        if (command == target)
        {
            for (i = 0; i < setup->objectcount; i++)
            { if (setup->objects[i].sourceoffset == at) { *objectindex = i; return TRUE; } }
            return FALSE;
        }
        at += bytes;
    }
    return FALSE;
}

/* Intro commands have their own sizes, independent of propDefs. */
static DWORD SetupIntroWordCount(DWORD type)
{
    static const DWORD words[] = {3, 4, 4, 8, 2, 2, 10, 3, 2, 1};
    return type < sizeof(words) / sizeof(words[0]) ? words[type] : 0;
}

/* Native setups have ten root pointers. Placement/script edits append tables
 * within a transaction; commit, load, save and export discard abandoned data.
 * Keep 16-byte blocks so every retained byte preserves its
 * alignment, including overlapping strings and pointers into terminators. */
typedef struct SetupCompact {
    const unsigned char *data;
    DWORD size;
    unsigned char *used, *pointers;
    DWORD emptytarget;
} SetupCompact;

static BOOL SetupCompactKeep(SetupCompact *c, DWORD at, DWORD bytes)
{
    DWORD end;
    if (at > c->size || bytes > c->size - at || !bytes) { return FALSE; }
    end = (at + bytes - 1) / 16;
    for (DWORD i = at / 16; i <= end; i++) { c->used[i] = 1; }
    return TRUE;
}

static BOOL SetupCompactPointer(SetupCompact *c, DWORD field, DWORD *target)
{
    if ((field & 3) || !SetupCompactKeep(c, field, 4)) { return FALSE; }
    *target = SetupRead32(c->data + field);
    if (*target && (*target < SETUP_HEADER_SIZE || *target >= c->size)) { return FALSE; }
    c->pointers[field / 4] = 1;
    return TRUE;
}

static BOOL SetupCompactString(SetupCompact *c, DWORD at)
{
    const unsigned char *end;
    if (!at) { return TRUE; }
    if (at >= c->size) { return FALSE; }
    end = memchr(c->data + at, 0, c->size - at);
    return end && SetupCompactKeep(c, at, (DWORD)(end - c->data - at) + 1);
}

/* Waypoint indices and group neighbours end with a signed negative word. */
static BOOL SetupCompactIndices(SetupCompact *c, DWORD at)
{
    if (!at) { return TRUE; }
    if (at & 3) { return FALSE; }
    for (;; at += 4)
    {
        if (!SetupCompactKeep(c, at, 4)) { return FALSE; }
        if (SetupRead32(c->data + at) & 0x80000000u) { return TRUE; }
    }
}

static BOOL SetupCompactScript(SetupCompact *c, DWORD at)
{
    while (at < c->size)
    {
        DWORD op = c->data[at], bytes;
        if (op >= ACTION_OPCODE_COUNT) { return FALSE; }
        bytes = g_ActionOpcodes[op].size;
        if (op == 0xad) /* PRINT has a zero-terminated inline string. */
        {
            const unsigned char *end = memchr(c->data + at + 1, 0, c->size - at - 1);
            if (!end) { return FALSE; }
            bytes = (DWORD)(end - c->data - at) + 1;
        }
        if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
        at += bytes;
        if (op == 4) { return TRUE; }
    }
    return FALSE;
}

static BOOL SetupCompactRoot(SetupCompact *c, DWORD root, DWORD at)
{
    if (!at) { return TRUE; }
    if (at & 3) { return FALSE; }
    for (;;)
    {
        DWORD value, bytes, target;
        if (!SetupCompactKeep(c, at, 4)) { return FALSE; }
        value = SetupRead32(c->data + at);
        switch (root)
        {
        case 0: /* waypoint { pad, neighbours*, group, distance } */
            bytes = 16;
            if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
            if (value & 0x80000000u) { return TRUE; }
            if (!SetupCompactPointer(c, at + 4, &target)
                || !SetupCompactIndices(c, target)) { return FALSE; }
            break;
        case 1: /* waygroup { neighbours*, waypoints*, distance } */
            bytes = 12;
            if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
            if (!value) { return TRUE; }
            for (DWORD i = 0; i < 8; i += 4)
            {
                if (!SetupCompactPointer(c, at + i, &target)
                    || !SetupCompactIndices(c, target)) { return FALSE; }
            }
            break;
        case 2: /* intro commands, including the Cuba credits pointer */
            bytes = SetupIntroWordCount(value) * 4;
            if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
            if (value == 9) { return TRUE; }
            if (value == 8)
            {
                if (!SetupCompactPointer(c, at + 4, &target) || !target || (target & 3)) { return FALSE; }
                do
                {
                    if (!SetupCompactKeep(c, target, 12)) { return FALSE; }
                    value = SetupRead32(c->data + target);
                    target += 12;
                } while (value);
            }
            break;
        case 3: /* prop commands contain indices/IDs, not file pointers */
            value &= 255;
            if (value > SETUP_PROP_END) { return FALSE; }
            bytes = SetupObjectWordCount((unsigned char)value) * 4;
            if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
            if (value == SETUP_PROP_END) { return TRUE; }
            break;
        case 4: /* patrol { waypoint indices*, id/flags/length } */
        case 5: /* Action Block { bytecode*, id } */
            bytes = 8;
            if (!SetupCompactKeep(c, at, bytes)) { return FALSE; }
            if (!value) { return TRUE; }
            if (!SetupCompactPointer(c, at, &target)
                || !(root == 4 ? SetupCompactIndices(c, target) : SetupCompactScript(c, target))) { return FALSE; }
            break;
        case 6: /* pads and bound pads: plink is the only native pointer */
        case 7:
            bytes = root == 6 ? SETUP_PAD_SIZE : SETUP_BOUNDPAD_SIZE;
            if (!SetupCompactKeep(c, at, bytes)
                || !SetupCompactPointer(c, at + SETUP_PAD_LINK, &target)) { return FALSE; }
            if (!target) { c->emptytarget = at + SETUP_PAD_LINK; return TRUE; }
            /* Old editors pointed empty plinks into abandoned terminators.
             * Retaining each such byte would retain one block per edit even
             * after compaction. Share a current terminator's zero instead. */
            if (!c->data[target]) { c->pointers[(at + SETUP_PAD_LINK) / 4] = 2; }
            else if (!SetupCompactString(c, target)) { return FALSE; }
            break;
        default: /* optional pad-name pointer tables */
            bytes = 4;
            if (!SetupCompactPointer(c, at, &target)) { return FALSE; }
            if (!target) { return TRUE; }
            if (!SetupCompactString(c, target)) { return FALSE; }
            break;
        }
        at += bytes;
    }
}

BOOL SetupCompactNative(const unsigned char *data, DWORD size,
    unsigned char **out, DWORD *sizeout, const char **reasonout)
{
    SetupCompact c = {0};
    DWORD blocks, *map = NULL, total = 0;
    unsigned char *packed = NULL;
    BOOL ok = FALSE;
    *out = NULL; *sizeout = 0;
    *reasonout = "The setup contains an invalid native table or pointer.";
    if (!data || size < SETUP_HEADER_SIZE || size > SETUP_FILE_MAX) { return FALSE; }
    c.data = data; c.size = size; blocks = (size + 15) / 16;
    c.used = calloc(blocks, 1); c.pointers = calloc((size + 3) / 4, 1);
    map = malloc((size_t)blocks * sizeof(*map));
    if (!c.used || !c.pointers || !map) { goto memory; }
    SetupCompactKeep(&c, 0, SETUP_HEADER_SIZE);
    for (DWORD root = 0; root < 10; root++)
    {
        static const char *const errors[] = {
            "The setup waypoint table is invalid.", "The setup waypoint-group table is invalid.",
            "The setup intro table is invalid.", "The setup object table is invalid.",
            "The setup patrol table is invalid.", "The setup Action Block table is invalid.",
            "The setup pad table is invalid.", "The setup bound-pad table is invalid.",
            "The setup pad-name table is invalid.", "The setup bound-pad-name table is invalid."
        };
        DWORD target;
        if (!SetupCompactPointer(&c, root * 4, &target)
            || !SetupCompactRoot(&c, root, target)) { *reasonout = errors[root]; goto done; }
    }
    for (DWORD i = 0; i < blocks; i++)
    {
        map[i] = total;
        if (c.used[i]) { total += 16; }
    }
    packed = calloc(total, 1);
    if (!packed) { goto memory; }
    for (DWORD i = 0; i < blocks; i++) if (c.used[i])
    {
        DWORD bytes = size - i * 16;
        if (bytes > 16) { bytes = 16; }
        memcpy(packed + map[i], data + i * 16, bytes);
    }
    for (DWORD field = 0; field + 4 <= size; field += 4) if (c.pointers[field / 4])
    {
        DWORD target = c.pointers[field / 4] == 2 ? c.emptytarget : SetupRead32(data + field);
        if (target)
        {
            if (!c.used[target / 16]) { goto done; }
            SetupWrite32(packed + map[field / 16] + field % 16, map[target / 16] + target % 16);
        }
    }
    SetupCleanNativePadDirections(packed);
    *out = packed; packed = NULL; *sizeout = total; *reasonout = ""; ok = TRUE;
    goto done;
memory:
    *reasonout = "Out of memory compacting the setup.";
done:
    free(packed); free(map); free(c.used); free(c.pointers);
    return ok;
}

BOOL SetupFileCompact(SetupFile *setup, const char **why)
{
    unsigned char *data;
    DWORD size, oldcommands, newcommands;
    if (!setup || !SetupCompactNative(setup->data, setup->size, &data, &size, why)) { return FALSE; }
    oldcommands = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    newcommands = SetupRead32(data + SETUP_OBJECT_POINTER);
    /* The command stream is contiguous and its record order never changes. */
    for (DWORD i = 0; i < setup->objectcount + setup->charactercount; i++)
    {
        DWORD at = i < setup->objectcount ? setup->objects[i].sourceoffset
            : setup->characters[i - setup->objectcount].sourceoffset;
        if (!oldcommands || at < oldcommands || at >= setup->size
            || newcommands + (at - oldcommands) >= size)
        { free(data); *why = "A cached setup command offset is invalid."; return FALSE; }
    }
    for (DWORD i = 0; i < setup->objectcount; i++)
    { setup->objects[i].sourceoffset = newcommands + (setup->objects[i].sourceoffset - oldcommands); }
    for (DWORD i = 0; i < setup->charactercount; i++)
    { setup->characters[i].sourceoffset = newcommands + (setup->characters[i].sourceoffset - oldcommands); }
    for (DWORD i = 0; i < setup->padcount; i++)
    { SetupCleanPadDirections(&setup->pads[i]); }
    for (DWORD i = 0; i < setup->boundpadcount; i++)
    { SetupCleanPadDirections(&setup->boundpads[i].pad); }
    if (size == setup->size)
    { memcpy(setup->data, data, size); free(data); return TRUE; }
    free(setup->data); setup->data = data; setup->size = size;
    return TRUE;
}

/* Multiple script tags may use the same outro camera. Display identical
 * native camera records as one marker without removing commands or tags. */
static BOOL SetupOutroDuplicate(const SetupFile *setup, DWORD record)
{
    DWORD at = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    while (at < record)
    {
        unsigned char type = setup->data[at + 3];
        if (type == PROPDEF_CAMERAPOS && SetupRead32(setup->data + at) != SETUP_DELETED_CHARACTER_HEADER
            && !memcmp(setup->data + at + 4, setup->data + record + 4, 24)) { return TRUE; }
        at += SetupObjectWordCount(type) * 4;
    }
    return FALSE;
}

static BOOL SetupAppendMarker(SetupMarker **markers, DWORD *count, const SetupMarker *marker)
{
    SetupMarker *grown;
    if (*count >= 4096) { return FALSE; }
    grown = realloc(*markers, ((size_t)*count + 1) * sizeof(*grown));
    if (grown == NULL) { return FALSE; }
    *markers = grown;
    grown[(*count)++] = *marker;
    return TRUE;
}

static void SetupCameraMarker(const unsigned char *record, SetupMarkerKind kind, SetupMarker *marker)
{
    float yaw = (LONG)SetupRead32(record + 16) / 65536.0f;
    float pitch = (LONG)SetupRead32(record + 20) / 65536.0f;
    int axis;
    ZeroMemory(marker, sizeof(*marker));
    marker->kind = kind;
    marker->pad = SetupRead32(record + 24);
    /* Match setupLoadFiles and bondview's camera direction. Camera positions
     * are hundredths of world units, NOT scaled pad or BG coordinates. */
    for (axis = 0; axis < 3; axis++)
    { marker->position[axis] = (LONG)SetupRead32(record + 4 + axis * 4) / 100.0f; }
    marker->look[0] = cosf(pitch) * sinf(yaw);
    marker->look[1] = sinf(pitch);
    marker->look[2] = -cosf(pitch) * cosf(yaw);
    marker->up[0] = -sinf(pitch) * sinf(yaw);
    marker->up[1] = cosf(pitch);
    marker->up[2] = sinf(pitch) * cosf(yaw);
}

/* Patrol entries contain waypoint indices, not pad indices. Resolve through
 * the 16-byte waypoint table; do not draw the general navigation graph. */
static int SetupComparePatrolLinks(const void *left, const void *right)
{
    const SetupPatrolLink *a = left, *b = right;
    for (int i = 0; i < 2; i++)
    {
        if (a->pads[i] != b->pads[i]) { return a->pads[i] < b->pads[i] ? -1 : 1; }
    }
    return 0;
}

static BOOL SetupAppendPatrolLink(SetupPatrolLink **links, DWORD *count, DWORD *capacity,
                                  DWORD start, DWORD end, BOOL loop)
{
    SetupPatrolLink *link;
    if (start == end) { return TRUE; }
    if (*count == *capacity)
    {
        DWORD next = *capacity ? *capacity * 2 : 64;
        SetupPatrolLink *grown;
        if (next > SETUP_PAD_MAX) { return FALSE; }
        grown = realloc(*links, next * sizeof(*grown));
        if (!grown) { return FALSE; }
        *links = grown; *capacity = next;
    }
    link = &(*links)[(*count)++];
    link->pads[0] = start < end ? start : end;
    link->pads[1] = start < end ? end : start;
    /* Non-looping patrols reverse at each end (chrlvPatrolCalculateStep). */
    link->directions = !loop ? 3 : start < end ? 1 : 2;
    return TRUE;
}

BOOL SetupFileBuildPatrolLinks(const SetupFile *setup, SetupPatrolLink **links,
                               DWORD *count, unsigned char *pathpads, const char **reasonout)
{
    DWORD paths, waypoints, waypointcount = 0, capacity = 0, visited = 0, at;
    *links = NULL; *count = 0;
    *reasonout = "The setup's patrol paths or waypoint references are invalid.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE) { return FALSE; }
    if (pathpads) { memset(pathpads, 0, setup->padcount); }
    paths = SetupRead32(setup->data + 16);
    if (!paths) { return TRUE; }
    if (paths < SETUP_HEADER_SIZE || (paths & 3) || paths > setup->size - 8) { return FALSE; }
    if (!SetupRead32(setup->data + paths)) { return TRUE; }
    waypoints = SetupRead32(setup->data);
    if (waypoints < SETUP_HEADER_SIZE || (waypoints & 3) || waypoints > setup->size - 16) { return FALSE; }
    at = waypoints;
    while ((LONG)SetupRead32(setup->data + at) >= 0)
    {
        if (++waypointcount > SETUP_PAD_MAX || setup->size - at < 32) { return FALSE; }
        at += 16;
    }
    for (at = paths; ; at += 8)
    {
        DWORD steps, first = SETUP_PAD_INDEX_NONE, previous = SETUP_PAD_INDEX_NONE;
        BOOL loop;
        if (at > setup->size - 8) { goto fail; }
        steps = SetupRead32(setup->data + at);
        if (!steps) { break; }
        loop = (setup->data[at + 5] & 1) != 0;
        if (steps < SETUP_HEADER_SIZE || (steps & 3) || steps > setup->size - 4) { goto fail; }
        for (;; steps += 4)
        {
            DWORD waypoint, pad;
            /* Bound total work even if malformed paths repeatedly share a list. */
            if (++visited > SETUP_PAD_MAX || steps > setup->size - 4) { goto fail; }
            waypoint = SetupRead32(setup->data + steps);
            if ((LONG)waypoint < 0) { break; }
            if (waypoint >= waypointcount) { goto fail; }
            pad = SetupRead32(setup->data + waypoints + waypoint * 16);
            if (pad >= setup->padcount || !setup->pads) { goto fail; }
            if (pathpads) { pathpads[pad] = 1; }
            if (previous != SETUP_PAD_INDEX_NONE
                && !SetupAppendPatrolLink(links, count, &capacity, previous, pad, loop)) { goto allocation; }
            if (first == SETUP_PAD_INDEX_NONE) { first = pad; }
            previous = pad;
        }
        if (loop && first != SETUP_PAD_INDEX_NONE
            && !SetupAppendPatrolLink(links, count, &capacity, previous, first, TRUE)) { goto allocation; }
    }
    if (*count > 1)
    {
        DWORD unique = 1;
        qsort(*links, *count, sizeof(**links), SetupComparePatrolLinks);
        for (DWORD i = 1; i < *count; i++)
        {
            if (!SetupComparePatrolLinks(&(*links)[unique - 1], &(*links)[i]))
            { (*links)[unique - 1].directions |= (*links)[i].directions; }
            else { (*links)[unique++] = (*links)[i]; }
        }
        *count = unique;
    }
    return TRUE;
allocation:
    *reasonout = "The patrol preview is too large or could not be allocated.";
fail:
    free(*links); *links = NULL; *count = 0;
    if (pathpads) { memset(pathpads, 0, setup->padcount); }
    return FALSE;
}

BOOL SetupFileBuildMarkers(const SetupFile *setup, float levelscale,
                           SetupMarker **markers, DWORD *count, const char **reasonout)
{
    DWORD list, at, commands, bytes, type, i;
    BOOL multiplayer, hasspawn = FALSE;
    *markers = NULL; *count = 0;
    *reasonout = "The setup's spawn/camera records are invalid.";
    if (setup == NULL || setup->data == NULL || setup->size < SETUP_HEADER_SIZE
        || !isfinite(levelscale) || levelscale <= 0) { return FALSE; }
    multiplayer = strncmp(setup->name, "Ump_", 4) == 0;
    /* Header word 2 is the intro list, word 3 is propDefs. Parse the live
     * raw setup so adding props and undo/redo never leave stale offsets. */
    for (list = 0; list < 2; list++)
    {
        at = SetupRead32(setup->data + 8 + list * 4);
        if (at == 0) { continue; }
        if (at < SETUP_HEADER_SIZE || (at & 3) || at > setup->size) { goto fail; }
        for (commands = 0; commands < SETUP_OBJECT_MAX; commands++)
        {
            const unsigned char *record;
            SetupMarker marker;
            if (setup->size - at < 4) { goto fail; }
            record = setup->data + at;
            type = list == 0 ? SetupRead32(record) : record[3];
            if (type == (list == 0 ? 9u : SETUP_PROP_END)) { break; }
            bytes = (list == 0 ? SetupIntroWordCount(type) : SetupObjectWordCount((unsigned char)type)) * 4;
            if (bytes == 0 || bytes > setup->size - at) { goto fail; }
            if (list == 0 && type == 0 && SetupRead32(record + 8) == 0
                && (multiplayer || !hasspawn))
            {
                const SetupPad *pad;
                float length;
                ZeroMemory(&marker, sizeof(marker));
                marker.kind = SETUP_MARKER_SPAWN;
                marker.command = commands;
                marker.pad = SetupRead32(record + 4);
                if (marker.pad >= setup->padcount || setup->pads == NULL) { goto fail; }
                pad = &setup->pads[marker.pad];
                for (i = 0; i < 3; i++)
                {
                    marker.position[i] = pad->pos[i] / levelscale;
                    if (!isfinite(marker.position[i])) { goto fail; }
                }
                /* The game derives starting yaw from the horizontal look. */
                length = hypotf(pad->look[0], pad->look[2]);
                if (!isfinite(length)) { goto fail; }
                marker.look[0] = length > 0 ? pad->look[0] / length : 0;
                marker.look[2] = length > 0 ? pad->look[2] / length : 1;
                marker.up[1] = 1;
                if (!SetupAppendMarker(markers, count, &marker)) { goto memory; }
                hasspawn = TRUE;
            }
            else if ((list == 0 && type == 6)
                || (list == 1 && type == PROPDEF_CAMERAPOS
                    && SetupRead32(record) != SETUP_DELETED_CHARACTER_HEADER
                    && !SetupOutroDuplicate(setup, at)))
            {
                SetupCameraMarker(record, list == 0 ? SETUP_MARKER_INTRO : SETUP_MARKER_OUTRO, &marker);
                marker.command = commands;
                if (!SetupAppendMarker(markers, count, &marker)) { goto memory; }
            }
            at += bytes;
        }
        if (commands == SETUP_OBJECT_MAX) { goto fail; }
    }
    *reasonout = "";
    return TRUE;
memory:
    *reasonout = "Too many setup markers or not enough memory to display them.";
fail:
    free(*markers); *markers = NULL; *count = 0;
    return FALSE;
}

void SetupSwirlPathFree(SetupSwirlPath *path)
{
    free(path->points);
    free(path->curve);
    ZeroMemory(path, sizeof(*path));
}

static BOOL SetupNormalizeDirection(float direction[3])
{
    float length = hypotf(hypotf(direction[0], direction[1]), direction[2]);
    int axis;
    if (!isfinite(length) || length < 0.000001f) { return FALSE; }
    for (axis = 0; axis < 3; axis++) { direction[axis] /= length; }
    return TRUE;
}

static void SetupOrientSwirlPoints(SetupSwirlPath *path, DWORD samples)
{
    DWORD i;
    for (i = 1; i + 1 < path->pointcount; i++)
    {
        SetupSwirlPoint *point = &path->points[i];
        DWORD knot = (i - 1) * samples, sample;
        /* Hermite's derivative at the knot. Use the outgoing segment except
         * at the final travelled point, which uses the incoming segment. */
        float scale = path->points[i + 2 < path->pointcount ? i : i - 1].tangentscale;
        BOOL directed;
        int axis;
        for (axis = 0; axis < 3; axis++)
        {
            point->look[axis] = scale * (path->points[i + 1].position[axis]
                - path->points[i - 1].position[axis]);
        }
        directed = SetupNormalizeDirection(point->look);
        /* Zero tension or coincident controls can give a zero derivative.
         * Follow the first movement after the knot, or the arrival direction
         * if there is no later movement. Never reverse an end marker. */
        for (sample = knot + 1; !directed && sample < path->curvecount; sample++)
        {
            for (axis = 0; axis < 3; axis++)
            { point->look[axis] = path->curve[sample][axis] - point->position[axis]; }
            directed = SetupNormalizeDirection(point->look);
        }
        for (sample = knot; !directed && sample > 0; sample--)
        {
            for (axis = 0; axis < 3; axis++)
            { point->look[axis] = point->position[axis] - path->curve[sample - 1][axis]; }
            directed = SetupNormalizeDirection(point->look);
        }
        if (!directed) { point->look[0] = 1; point->look[1] = point->look[2] = 0; }
        /* Project world up onto the plane perpendicular to the tangent.
         * Near a vertical tangent use world Z to keep the frame nonsingular. */
        axis = fabsf(point->look[1]) > 0.99f ? 2 : 1;
        for (int component = 0; component < 3; component++)
        { point->up[component] = (component == axis ? 1.0f : 0.0f) - point->look[axis] * point->look[component]; }
        SetupNormalizeDirection(point->up);
    }
    /* Tangent-only end controls inherit the adjacent travelled frame. The
     * game's normal paths duplicate these positions at both ends. */
    memcpy(path->points[0].look, path->points[1].look, sizeof(path->points[0].look));
    memcpy(path->points[0].up, path->points[1].up, sizeof(path->points[0].up));
    i = path->pointcount - 1;
    memcpy(path->points[i].look, path->points[i - 1].look, sizeof(path->points[i].look));
    memcpy(path->points[i].up, path->points[i - 1].up, sizeof(path->points[i].up));
}

BOOL SetupFileBuildSwirlPath(const SetupFile *setup, const SetupMarker *spawn,
                            SetupSwirlPath *path, const char **reasonout)
{
    const DWORD samples = 32;
    DWORD at, first = 0, firstcommand = 0, count = 0, commands, i, step;
    int axis;
    BOOL ended = FALSE;
    ZeroMemory(path, sizeof(*path));
    *reasonout = "The setup's intro swirl path is invalid.";
    if (setup == NULL || setup->data == NULL || setup->size < SETUP_HEADER_SIZE) { return FALSE; }
    /* Multiplayer uses a procedural orbit, not the authored solo swirl. */
    if (spawn == NULL || strncmp(setup->name, "Ump_", 4) == 0) { goto empty; }
    if (spawn->kind != SETUP_MARKER_SPAWN) { return FALSE; }
    for (axis = 0; axis < 3; axis++)
    {
        if (!isfinite(spawn->position[axis]) || !isfinite(spawn->look[axis])) { return FALSE; }
    }
    at = SetupRead32(setup->data + 8);
    if (at == 0) { goto empty; }
    if (at < SETUP_HEADER_SIZE || (at & 3) || at > setup->size) { return FALSE; }
    /* The game uses the first contiguous INTROTYPE_SWIRL block. Bit 0 ends
     * that block; the terminating record is not a control point. */
    for (commands = 0; commands < SETUP_OBJECT_MAX; commands++)
    {
        DWORD type, bytes;
        if (setup->size - at < 4) { return FALSE; }
        type = SetupRead32(setup->data + at);
        if (type == 9 && first == 0) { goto empty; }
        bytes = SetupIntroWordCount(type) * 4;
        if (bytes == 0 || bytes > setup->size - at) { return FALSE; }
        if (type == 3)
        {
            if (first == 0) { first = at; firstcommand = commands; }
            if (SetupRead32(setup->data + at + 4) & 1) { ended = TRUE; break; }
            if (++count > 4096) { return FALSE; }
        }
        else if (first != 0) { return FALSE; }
        at += bytes;
    }
    /* Camera playback starts at index 1, using its neighbours for tangents. */
    if (!ended || count < 4) { return FALSE; }
    path->points = calloc(count, sizeof(*path->points));
    path->curvecount = (count - 3) * samples + 1;
    path->curve = malloc((size_t)path->curvecount * sizeof(*path->curve));
    if (path->points == NULL || path->curve == NULL)
    {
        *reasonout = "Not enough memory to display the intro swirl path.";
        SetupSwirlPathFree(path);
        return FALSE;
    }
    path->pointcount = count;
    for (i = 0; i < count; i++)
    {
        const unsigned char *record = setup->data + first + i * 32;
        float offset[3];
        SetupSwirlPoint *point = &path->points[i];
        point->command = firstcommand + i;
        for (axis = 0; axis < 3; axis++)
        { offset[axis] = (LONG)SetupRead32(record + 8 + axis * 4) / 65536.0f; }
        if (SetupRead32(record + 4) & 2)
        {
            /* Match bviewCalcIntroSwirlCamera's facing-relative offsets. */
            point->position[0] = offset[2] * spawn->look[0] + offset[0] * spawn->look[2];
            point->position[2] = offset[2] * spawn->look[2] - offset[0] * spawn->look[0];
        }
        else
        {
            point->position[0] = offset[0];
            point->position[2] = offset[2];
        }
        /* bviewPlayerBeginLife: 185 * normal perspective height - 10.
         * This is a static preview; animation/head bob is not simulated. */
        point->position[1] = offset[1] + 175.0f;
        for (axis = 0; axis < 3; axis++) { point->position[axis] += spawn->position[axis]; }
        point->tangentscale = (LONG)SetupRead32(record + 20) / 65536.0f;
    }
    at = 0;
    for (i = 1; i + 2 < count; i++)
    {
        const float scale = path->points[i].tangentscale;
        for (step = 0; step < samples; step++)
        {
            float t = (float)step / samples, square = t * t, cube = square * t;
            /* coord3dCubicSplineInterp's Hermite weights, including the
             * per-segment tension. First/last controls are not travelled. */
            float prev = (2 * square - t - cube) * scale;
            float start = (2 - scale) * cube + (scale - 3) * square + 1;
            float end = (scale - 2) * cube + (3 - 2 * scale) * square + t * scale;
            float next = (cube - square) * scale;
            for (axis = 0; axis < 3; axis++)
            {
                path->curve[at][axis] = prev * path->points[i - 1].position[axis]
                    + start * path->points[i].position[axis]
                    + end * path->points[i + 1].position[axis]
                    + next * path->points[i + 2].position[axis];
            }
            at++;
        }
    }
    memcpy(path->curve[at], path->points[count - 2].position, sizeof(*path->curve));
    SetupOrientSwirlPoints(path, samples);
empty:
    *reasonout = "";
    return TRUE;
}

static BOOL SetupTypeCreatesObject(unsigned char type)
{
    switch (type)
    {
    case 1:  /* door */
    case 3:  /* ordinary prop */
    case 4:  /* key */
    case 5:  /* alarm */
    case 6:  /* CCTV */
    case 7:  /* magazine */
    case 8:  /* collectable/weapon */
    case 10: /* monitor */
    case 11: /* multi-monitor */
    case 12: /* hanging monitor rack */
    case 13: /* autogun */
    case 17: /* hat */
    case 20: /* ammo crate */
    case 21: /* body armour */
    case 36: /* gas-releasing object */
    case 39: /* vehicle */
    case 40: /* aircraft */
    case 42: /* glass */
    case 43: /* safe */
    case 45: /* tank */
    case 47: /* tinted glass */
        return TRUE;
    }

    return FALSE;
}

static BOOL SetupParseObjects(SetupFile *setup, const char **reasonout)
{
    DWORD offset;
    DWORD at;
    DWORD commandcount;
    DWORD objectcount = 0;
    DWORD objectat = 0;
    DWORD charactercount = 0;
    DWORD characterat = 0;

    offset = SetupRead32(setup->data + SETUP_OBJECT_POINTER);

    /* A NULL propDefs pointer is legal for a setup with no objects. */
    if (offset == 0)
    {
        return TRUE;
    }

    if (offset < SETUP_HEADER_SIZE || offset >= setup->size)
    {
        *reasonout = "the setup's object-list pointer is invalid.";
        return FALSE;
    }

    at = offset;

    /* Validate the complete variable-sized list before decoding props and
       characters into their separate host-native record arrays. */
    for (commandcount = 0; commandcount < SETUP_OBJECT_MAX; commandcount++)
    {
        unsigned char type;
        DWORD bytes;

        if (setup->size - at < 4)
        {
            *reasonout = "the setup's object list has no terminator.";
            return FALSE;
        }

        type = setup->data[at + 3];
        if (type == SETUP_PROP_END)
        {
            break;
        }

        bytes = SetupObjectWordCount(type) * 4;
        if (bytes < 4 || bytes > setup->size - at)
        {
            *reasonout = "the setup's object list is malformed.";
            return FALSE;
        }

        if (SetupTypeCreatesObject(type))
        {
            /* Every type accepted above starts with ObjectRecord. */
            if (bytes < 16)
            {
                *reasonout = "an object setup command is too small.";
                return FALSE;
            }
            objectcount++;
        }
        else if (type == PROPDEF_GUARD || SetupRead32(setup->data + at) == SETUP_DELETED_CHARACTER_HEADER)
        {
            charactercount++;
        }

        at += bytes;
    }

    if (commandcount == SETUP_OBJECT_MAX)
    {
        *reasonout = "the setup's object list is unreasonably long.";
        return FALSE;
    }

    if (objectcount == 0 && charactercount == 0)
    {
        return TRUE;
    }

    if (objectcount > 0)
    {
        setup->objects = (SetupObject *)malloc(objectcount * sizeof(*setup->objects));
    }
    if (charactercount > 0)
    {
        setup->characters = (SetupCharacter *)malloc(
            charactercount * sizeof(*setup->characters));
    }
    if ((objectcount > 0 && setup->objects == NULL)
        || (charactercount > 0 && setup->characters == NULL))
    {
        *reasonout = "out of memory decoding the setup's objects.";
        return FALSE;
    }

    at = offset;
    while (setup->data[at + 3] != SETUP_PROP_END)
    {
        const unsigned char *record = setup->data + at;
        unsigned char type = record[3];
        DWORD bytes = SetupObjectWordCount(type) * 4;

        if (SetupTypeCreatesObject(type))
        {
            SetupObject *object = &setup->objects[objectat++];

            object->type = type;
            object->extrascale = (unsigned short)SetupRead16(record);
            object->modelid = SetupRead16(record + 4);
            object->pad = SetupRead16(record + 6);
            object->flags = SetupRead32(record + 8);
            object->flags2 = SetupRead32(record + 12);
            object->sourceoffset = at;
            object->nonsolid = (record[2] & 0x20) != 0; /* PROPSTATE_NONSOLID */
            object->deleted =
                (object->flags2 & SETUP_OBJECT_DELETED_FLAGS2)
                    == SETUP_OBJECT_DELETED_FLAGS2;
        }
        else if (type == PROPDEF_GUARD || SetupRead32(record) == SETUP_DELETED_CHARACTER_HEADER)
        {
            SetupCharacter *character = &setup->characters[characterat++];

            character->chrnum = (unsigned short)SetupRead16(record + 4);
            character->pad = (unsigned short)SetupRead16(record + 6);
            character->bodyid = (unsigned short)SetupRead16(record + 8);
            character->ailistid = (unsigned short)SetupRead16(record + 10);
            character->flags = (unsigned short)SetupRead16(record + 20);
            character->headid = SetupRead16(record + 22);
            character->sourceoffset = at;
            character->deleted = type != PROPDEF_GUARD;
        }

        at += bytes;
    }

    setup->objectcount = objectat;
    setup->charactercount = characterat;
    return TRUE;
}

/* Returns FALSE for NaN and infinity as well as decoding big endian. */
static BOOL SetupReadFloat(const unsigned char *p, float *out)
{
    union { DWORD u; float f; } value;

    value.u = SetupRead32(p);
    if ((value.u & 0x7f800000u) == 0x7f800000u)
    {
        return FALSE;
    }

    *out = value.f;
    return TRUE;
}

/* Counts records using the same plink==NULL terminator as the game. */
static BOOL SetupCountPadList(const SetupFile *setup, DWORD offset,
                              DWORD recordsize, DWORD *countout)
{
    DWORD count;

    if (offset < SETUP_HEADER_SIZE || offset > setup->size)
    {
        return FALSE;
    }

    for (count = 0; count <= SETUP_PAD_MAX; count++)
    {
        DWORD record;

        if (count > (setup->size - offset) / recordsize)
        {
            return FALSE;
        }

        record = offset + count * recordsize;
        if (recordsize > setup->size - record)
        {
            return FALSE;
        }

        if (SetupRead32(setup->data + record + SETUP_PAD_LINK) == 0)
        {
            *countout = count;
            return TRUE;
        }
    }

    return FALSE;
}

/* Most references use 10000 + index for bound pads. Doors are the exception. */
static BOOL SetupPadMatches(const SetupPadRef *ref, DWORD value)
{
    return value == ref->index + (ref->bound ? 10000u : 0u);
}

static BOOL SetupPadExists(const SetupFile *setup, DWORD value)
{
    return value < 10000
        ? setup->pads && value < setup->padcount && !setup->pads[value].deleted
        : setup->boundpads && value - 10000 < setup->boundpadcount && !setup->boundpads[value - 10000].pad.deleted;
}

static BOOL SetupActionParameterIsCharacter(const ActionInstruction *ins, DWORD parameter)
{
    const ActionOpcode *op = &g_ActionOpcodes[ins->bytes[0]];
    /* Aim/facing operands can select a character or a pad. Character mode
     * takes priority when both bits are present. */
    return op->params[parameter].kind == ACTION_CHARACTER
        || (ins->bytes[0] >= 0x14 && ins->bytes[0] <= 0x17 && parameter == 1
            && (ActionReadValue(ins, 0) & 4));
}

BOOL SetupFileSetGlobalReferences(SetupFile *setup, const RomFile *rom, const char **why)
{
    ActionDocument actions = {0};
    SetupScriptReferences *refs;
    if (!setup || !rom) { *why = "No setup or project base ROM was supplied."; return FALSE; }
    if (!ActionDocumentLoadGlobals(&actions, rom, why))
    { ActionDocumentFree(&actions); return FALSE; }
    if (!actions.globalsloaded)
    {
        ActionDocumentFree(&actions);
        /* Older base ROMs remain editable, using conservative allocation.
         * Never retain a reference catalog from a different base ROM. */
        if (setup->globalrefs && !--setup->globalrefs->owners) { free(setup->globalrefs); }
        setup->globalrefs = NULL;
        *why = "";
        return TRUE;
    }
    refs = calloc(1, sizeof(*refs));
    if (!refs) { ActionDocumentFree(&actions); *why = "Out of memory indexing shared scripts."; return FALSE; }
    refs->owners = 1;
    for (DWORD b = 0; b < actions.count; b++) for (DWORD i = 0; i < actions.blocks[b].count; i++)
    {
        const ActionInstruction *ins = &actions.blocks[b].instructions[i];
        const ActionOpcode *op = &g_ActionOpcodes[ins->bytes[0]];
        for (DWORD p = 0; p < op->paramcount; p++)
        {
            DWORD v = ActionReadValue(ins, p);
            if (v < 65536)
            {
                if (ActionParameterIsPad(ins, p) && v != 9000) { refs->values[v] |= 1; }
                if (SetupActionParameterIsCharacter(ins, p)) { refs->values[v] |= 2; }
            }
        }
    }
    ActionDocumentFree(&actions);
    if (setup->globalrefs && !--setup->globalrefs->owners) { free(setup->globalrefs); }
    setup->globalrefs = refs;
    *why = "";
    return TRUE;
}

/* Ignore exactly one placement field, never the owner's other references
 * (for example a CCTV aim pad). This also covers native cameras and spawns. */
static BOOL SetupPadUnusedExcept(const SetupFile *setup, const SetupPadRef *ref,
                                 const RomFile *rom, DWORD ignoredfield, const char **why)
{
    DWORD at, i;
    ActionDocument actions = {0};
    BOOL unused = FALSE;

    *why = "This pad is used by a setup object.";
    for (i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *object = &setup->objects[i];
        if (object->deleted) { continue; }
        if (object->sourceoffset + 6 != ignoredfield && object->pad >= 0 && (object->type == PROPDEF_DOOR
            || !(object->flags & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR))))
        {
            if (object->type == PROPDEF_DOOR
                ? ref->bound && ref->index == (DWORD)object->pad
                : SetupPadMatches(ref, (DWORD)object->pad)) { return FALSE; }
        }
        if (object->type == PROPDEF_CCTV || object->type == PROPDEF_AUTOGUN)
        {
            if (object->sourceoffset > setup->size || setup->size - object->sourceoffset < 0x84)
            { goto malformed; }
            if (SetupPadMatches(ref, SetupRead32(setup->data + object->sourceoffset + 0x80)))
            { *why = "This pad is an aim target for a camera or drone gun."; return FALSE; }
        }
    }
    *why = "This pad is used by a character.";
    for (i = 0; !ref->bound && i < setup->charactercount; i++)
    {
        if (!setup->characters[i].deleted && setup->characters[i].sourceoffset + 6 != ignoredfield
            && setup->characters[i].pad == ref->index) { return FALSE; }
    }
    for (i = 0; i < setup->charactercount; i++)
    {
        const SetupCharacter *chr = &setup->characters[i];
        if (chr->deleted) { continue; }
        if (chr->sourceoffset > setup->size || setup->size - chr->sourceoffset < 28) { goto malformed; }
        if (SetupPadMatches(ref, (unsigned short)SetupRead16(setup->data + chr->sourceoffset + 12)))
        { *why = "This pad is a character's preset destination."; return FALSE; }
    }

    /* Every waypoint matters, even if it is not in a displayed patrol path:
     * guards also use this graph when finding routes during normal AI. */
    at = SetupRead32(setup->data);
    while (at)
    {
        if (at > setup->size - 16) { goto malformed; }
        if ((LONG)SetupRead32(setup->data + at) < 0) { break; }
        if (!ref->bound && SetupRead32(setup->data + at) == ref->index)
        { *why = "This pad is used by a navigation waypoint or patrol path."; return FALSE; }
        at += 16;
    }

    at = SetupRead32(setup->data + 8);
    while (at)
    {
        DWORD type, bytes;
        if (at > setup->size - 4) { goto malformed; }
        type = SetupRead32(setup->data + at);
        bytes = SetupIntroWordCount(type) * 4;
        if (!bytes || bytes > setup->size - at) { goto malformed; }
        if (type == 9) { break; }
        if ((type == 0 && at + 4 != ignoredfield && !ref->bound && SetupRead32(setup->data + at + 4) == ref->index)
            || (type == 3 && SetupPadMatches(ref, SetupRead32(setup->data + at + 28)))
            || (type == 6 && at + 24 != ignoredfield && SetupPadMatches(ref, SetupRead32(setup->data + at + 24))))
        { *why = "This pad is used by a spawn or intro camera."; return FALSE; }
        at += bytes;
    }

    at = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    while (at)
    {
        DWORD header, type, bytes, field = 0;
        if (at > setup->size - 4) { goto malformed; }
        header = SetupRead32(setup->data + at); type = header & 255;
        if (type > SETUP_PROP_END) { goto malformed; }
        if (type == SETUP_PROP_END) { break; }
        bytes = SetupObjectWordCount((unsigned char)type) * 4;
        if (bytes > setup->size - at) { goto malformed; }
        if (type == PROPDEF_OBJECTIVE_ENTER_ROOM) { field = 4; }
        if (type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM) { field = 8; }
        if (type == PROPDEF_CAMERAPOS && header != SETUP_DELETED_CHARACTER_HEADER) { field = 24; }
        if (field && at + field != ignoredfield && SetupPadMatches(ref, SetupRead32(setup->data + at + field)))
        { *why = "This pad is used by an objective or outro camera."; return FALSE; }
        at += bytes;
    }

    if (!ActionDocumentLoad(setup, &actions, why)) { return FALSE; }
    if (rom)
    {
        if (!ActionDocumentLoadGlobals(&actions, rom, why)) { goto done; }
    }
    else if (setup->globalrefs)
    {
        DWORD value = ref->index + (ref->bound ? 10000 : 0);
        if (value < 65536 && (setup->globalrefs->values[value] & 1))
        { *why = "This pad is referenced by a shared Action Block."; goto done; }
        actions.globalsloaded = TRUE;
    }
    if (!actions.globalsloaded)
    {
        *why = "Cannot verify pad usage: the project base ROM has no shared Action Block catalog. Rebase the project onto a current GUD ROM.";
        goto done;
    }
    for (i = 0; i < actions.count; i++)
    {
        const ActionBlock *block = &actions.blocks[i];
        DWORD row;
        for (row = 0; row < block->count; row++)
        {
            const ActionInstruction *ins = &block->instructions[row];
            const ActionOpcode *op = &g_ActionOpcodes[ins->bytes[0]];
            unsigned int p;
            for (p = 0; p < op->paramcount; p++)
            {
                DWORD value = ActionReadValue(ins, p);
                if (ActionParameterIsPad(ins, p) && value != 9000 && SetupPadMatches(ref, value))
                {
                    *why = block->global ? "This pad is referenced by a shared Action Block."
                                         : "This pad is referenced by a level Action Block.";
                    goto done;
                }
            }
        }
    }
    unused = TRUE;
done:
    ActionDocumentFree(&actions);
    return unused;
malformed:
    *why = "Cannot verify pad usage because a setup reference table is malformed.";
    return FALSE;
}

static BOOL SetupPadUnused(const SetupFile *setup, const SetupPadRef *ref,
                           const RomFile *rom, const char **why)
{
    return SetupPadUnusedExcept(setup, ref, rom, (DWORD)-1, why);
}

/* Recycle only explicit deletions and editor-owned placement pads. An
 * unreferenced authored standalone pad still belongs to the user. */
static DWORD SetupFindFreePad(const SetupFile *setup, BOOL bound, DWORD skip)
{
    DWORD count = bound ? setup->boundpadcount : setup->padcount;
    DWORD table = SetupRead32(setup->data + (bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    DWORD stride = bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    const char *why;
    if (!setup->globalrefs || table > setup->size || count > (setup->size - table) / stride)
    { return count + (skip == count); }
    for (DWORD i = 0; i < count; i++) if (i != skip)
    {
        DWORD marker = SetupRead32(setup->data + table + i * stride + 40);
        SetupPadRef ref = {i, bound};
        if ((marker == SETUP_DELETED_PAD_STAN || marker == SETUP_PRIVATE_PAD_STAN)
            && SetupPadUnused(setup, &ref, NULL, &why)) { return i; }
    }
    return count + (skip == count);
}

BOOL SetupFileDeletePad(SetupFile *setup, const SetupPadRef *ref,
                       const RomFile *rom, const char **reasonout)
{
    DWORD table, stride, count, record;
    SetupPad *pad;
    *reasonout = "Invalid pad selection.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || !ref) { return FALSE; }
    count = ref->bound ? setup->boundpadcount : setup->padcount;
    if (ref->index >= count || (ref->bound ? !setup->boundpads : !setup->pads)) { return FALSE; }
    table = SetupRead32(setup->data + (ref->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    stride = ref->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    if (!table || table > setup->size || count >= (setup->size - table) / stride) { return FALSE; }
    record = table + ref->index * stride;
    if (!SetupRead32(setup->data + record + SETUP_PAD_LINK)
        || SetupRead32(setup->data + table + count * stride + SETUP_PAD_LINK)) { return FALSE; }
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    if (pad->deleted) { *reasonout = "This pad has already been deleted."; return FALSE; }
    if (!SetupPadUnused(setup, ref, rom, reasonout)) { return FALSE; }
    SetupWrite32(setup->data + record + 40, SETUP_DELETED_PAD_STAN);
    pad->deleted = TRUE;
    setup->dirty = TRUE;
    *reasonout = "";
    return TRUE;
}

static BOOL SetupReadPad(const SetupFile *setup, const unsigned char *record,
                          SetupPad *pad)
{
    int axis;
    DWORD link = SetupRead32(record + SETUP_PAD_LINK);
    const unsigned char *end;

    pad->deleted = SetupRead32(record + 40) == SETUP_DELETED_PAD_STAN;
    if (link >= setup->size) { return FALSE; }
    end = (const unsigned char *)memchr(setup->data + link, 0, setup->size - link);
    if (end == NULL) { return FALSE; }
    ZeroMemory(pad->stanname, sizeof(pad->stanname));
    /* Valid stan names fit in nine bytes. Oversized/invalid names use the
       same nearest-walkable fallback as an empty plink string in the game. */
    if ((size_t)(end - (setup->data + link)) < sizeof(pad->stanname))
    {
        memcpy(pad->stanname, setup->data + link,
               (size_t)(end - (setup->data + link)));
    }

    for (axis = 0; axis < 3; axis++)
    {
        if (!SetupReadFloat(record + axis * 4, &pad->pos[axis])
            || !SetupReadFloat(record + 12 + axis * 4, &pad->up[axis])
            || !SetupReadFloat(record + 24 + axis * 4, &pad->look[axis]))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL SetupParsePads(SetupFile *setup, const char **reasonout)
{
    DWORD padoffset;
    DWORD boundoffset;
    DWORD i;

    if (setup->size < SETUP_HEADER_SIZE)
    {
        *reasonout = "the project setup is too small to have a header.";
        return FALSE;
    }

    padoffset = SetupRead32(setup->data + SETUP_PAD_POINTER);
    boundoffset = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER);

    if (!SetupCountPadList(setup, padoffset, SETUP_PAD_SIZE,
                           &setup->padcount))
    {
        *reasonout = "the setup's PadRecord list is malformed.";
        return FALSE;
    }

    if (!SetupCountPadList(setup, boundoffset, SETUP_BOUNDPAD_SIZE,
                           &setup->boundpadcount))
    {
        *reasonout = "the setup's BoundPadRecord list is malformed.";
        return FALSE;
    }

    if (setup->padcount > 0)
    {
        setup->pads = (SetupPad *)malloc(setup->padcount * sizeof(*setup->pads));
    }
    if (setup->boundpadcount > 0)
    {
        setup->boundpads = (SetupBoundPad *)malloc(
            setup->boundpadcount * sizeof(*setup->boundpads));
    }

    if ((setup->padcount > 0 && setup->pads == NULL)
        || (setup->boundpadcount > 0 && setup->boundpads == NULL))
    {
        *reasonout = "out of memory decoding the setup's pads.";
        return FALSE;
    }

    for (i = 0; i < setup->padcount; i++)
    {
        const unsigned char *record = setup->data + padoffset
                                    + i * SETUP_PAD_SIZE;

        if (!SetupReadPad(setup, record, &setup->pads[i]))
        {
            *reasonout = "a PadRecord contains an invalid coordinate or stan link.";
            return FALSE;
        }
    }

    for (i = 0; i < setup->boundpadcount; i++)
    {
        const unsigned char *record = setup->data + boundoffset
                                    + i * SETUP_BOUNDPAD_SIZE;
        SetupBoundPad *pad = &setup->boundpads[i];

        if (!SetupReadPad(setup, record, &pad->pad)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 0, &pad->xmin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 4, &pad->xmax)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 8, &pad->ymin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 12, &pad->ymax)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 16, &pad->zmin)
            || !SetupReadFloat(record + SETUP_BOUNDPAD_BBOX + 20, &pad->zmax))
        {
            *reasonout = "a BoundPadRecord contains an invalid coordinate, bound or stan link.";
            return FALSE;
        }
    }

    return TRUE;
}

/* A tombstone can be reused without renumbering any command. Protect all
 * native command-relative references, including tags used by AI/objectives.
 * Unrecognized command types disable recycling rather than guessing. */
static BOOL SetupCommandReferenced(const SetupFile *s, DWORD target)
{
    DWORD start = SetupRead32(s->data + SETUP_OBJECT_POINTER), at = start, wanted = 0;
    while (at < target)
    {
        if (at > s->size - 4) { return TRUE; }
        DWORD bytes = SetupObjectWordCount(s->data[at + 3]) * 4;
        if (bytes > s->size - at || s->data[at + 3] == SETUP_PROP_END) { return TRUE; }
        at += bytes; wanted++;
    }
    if (at != target) { return TRUE; }
    for (DWORD index = 0, at = start; at; index++)
    {
        DWORD type, bytes, fields = 0;
        if (at > s->size - 4) { return TRUE; }
        type = s->data[at + 3]; bytes = SetupObjectWordCount((unsigned char)type) * 4;
        if (bytes > s->size - at) { return TRUE; }
        if (type == SETUP_PROP_END) { return FALSE; }
        if (type > SETUP_PROP_END || type == 0 || type == 15 || type == 16 || type == 41) { return TRUE; }
        if (type == PROPDEF_LINK || type == PROPDEF_SWITCH || type == PROPDEF_LOCK_DOOR) { fields = 2; }
        if (type == PROPDEF_SAFE_ITEM) { fields = 3; }
        if (type == PROPDEF_RENAME) { fields = 1; }
        for (DWORD j = 0; j < fields; j++)
        { if ((long long)index + (LONG)SetupRead32(s->data + at + 4 + j * 4) == wanted) { return TRUE; } }
        if (type == PROPDEF_TAG && (long long)index + SetupRead16(s->data + at + 6) == wanted) { return TRUE; }
        if (type == PROPDEF_DOOR || type == PROPDEF_MONITOR)
        {
            LONG offset = (LONG)SetupRead32(s->data + at + (type == PROPDEF_DOOR ? 128 : 244));
            if (offset && (long long)index + offset == wanted) { return TRUE; }
        }
        if (SetupTypeCreatesObject((unsigned char)type)
            && (SetupRead32(s->data + at + 8) & PROPFLAG_INSIDEANOTHEROBJ)
            && (long long)index + SetupRead16(s->data + at + 6) == wanted) { return TRUE; }
        at += bytes;
    }
    return TRUE;
}

static BOOL SetupCharacterReferenced(const SetupFile *s, DWORD id)
{
    ActionDocument actions = {0};
    const char *why;
    BOOL used = TRUE;
    /* Special AI IDs and runtime-spawned/clone ranges are never recycled. */
    if (id >= 5000 || (id >= 248 && id <= 255) || !s->globalrefs
        || (s->globalrefs->values[id] & 2)
        || (s->globalrefs->values[id + 10000] & 2)) { return TRUE; }
    for (DWORD i = 0; i < s->objectcount; i++)
    {
        const SetupObject *obj = &s->objects[i];
        if (!obj->deleted && (obj->flags & PROPFLAG_ASSIGNEDTOCHR)
            && ((unsigned short)obj->pad == id || (unsigned short)obj->pad == id + 10000)) { return TRUE; }
    }
    for (DWORD at = SetupRead32(s->data + SETUP_OBJECT_POINTER); at;)
    {
        if (at > s->size - 4) { return TRUE; }
        DWORD type = s->data[at + 3], bytes = SetupObjectWordCount((unsigned char)type) * 4;
        if (bytes > s->size - at) { return TRUE; }
        if (type == SETUP_PROP_END) { break; }
        if (type == PROPDEF_GUARD_ATTRIBUTE || type == PROPDEF_GUARD)
        {
            DWORD value = type == PROPDEF_GUARD_ATTRIBUTE ? SetupRead32(s->data + at + 4)
                : (unsigned short)SetupRead16(s->data + at + 14);
            if (value == id || value == id + 10000) { return TRUE; }
        }
        at += bytes;
    }
    if (!ActionDocumentLoad(s, &actions, &why)) { return TRUE; }
    for (DWORD b = 0; b < actions.count; b++) for (DWORD i = 0; i < actions.blocks[b].count; i++)
    {
        const ActionInstruction *ins = &actions.blocks[b].instructions[i];
        const ActionOpcode *op = &g_ActionOpcodes[ins->bytes[0]];
        for (DWORD p = 0; p < op->paramcount; p++)
        {
            DWORD value = ActionReadValue(ins, p);
            if (SetupActionParameterIsCharacter(ins, p) && (value == id || value == id + 10000)) { goto done; }
        }
    }
    used = FALSE;
done:
    ActionDocumentFree(&actions); return used;
}

static DWORD SetupFindFreeCommand(const SetupFile *s, unsigned char type, DWORD *chrnum)
{
    if (type == PROPDEF_GUARD)
    {
        for (DWORD i = 0; i < s->charactercount; i++)
        {
            const SetupCharacter *chr = &s->characters[i];
            if (chr->deleted && !SetupCommandReferenced(s, chr->sourceoffset)
                && !SetupCharacterReferenced(s, chr->chrnum))
            { *chrnum = chr->chrnum; return chr->sourceoffset; }
        }
    }
    else for (DWORD i = 0; i < s->objectcount; i++)
    {
        const SetupObject *obj = &s->objects[i];
        if (obj->deleted && obj->type == type && !SetupCommandReferenced(s, obj->sourceoffset))
        { return obj->sourceoffset; }
    }
    return 0;
}

/* Copy the command stream before extending it: growing it in place could
 * overwrite another section. Commit discards the abandoned table copies. */
static BOOL SetupAddPlacement(SetupFile *setup, unsigned char type, int modelid, float levelscale,
                              const double position[3], const SetupBoundPad *bound,
                              const SetupPad *orientation,
                              DWORD *selectionout, const char **reasonout)
{
    SetupFile added = {0};
    DWORD oldcommands, commandend, commandsize, commandcount = 0;
    DWORD oldpads, newcommands, newrecord, newpads, newpad, chrnum = 0, i;
    BOOL door = type == PROPDEF_DOOR, character = type == PROPDEF_GUARD;
    BOOL aimed = type == PROPDEF_CCTV || type == PROPDEF_AUTOGUN;
    DWORD padheader = bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER;
    DWORD padsize = bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    DWORD padcount = setup ? (bound ? setup->boundpadcount : setup->padcount) : 0;
    DWORD recordsize = SetupObjectWordCount(type) * 4;
    DWORD padindex, aimindex, padtotal, reused;
    float authored[3], target[3];

    if (setup == NULL || setup->data == NULL || setup->size < SETUP_HEADER_SIZE ||
        setup->size > SETUP_FILE_MAX || position == NULL || selectionout == NULL || modelid < 0 ||
        modelid > 32767 || !isfinite(levelscale) || levelscale <= 0 || (aimed && !orientation) ||
        (setup->charactercount > 0 && setup->characters == NULL))
    {
        *reasonout = "The setup, model or level scale is invalid.";
        return FALSE;
    }
    reused = SetupFindFreeCommand(setup, type, &chrnum);
    padindex = SetupFindFreePad(setup, bound != NULL, (DWORD)-1);
    aimindex = aimed ? SetupFindFreePad(setup, FALSE, padindex) : padindex;
    padtotal = padcount;
    if (padindex >= padtotal) { padtotal = padindex + 1; }
    if (aimindex >= padtotal) { padtotal = aimindex + 1; }
    /* Ordinary props reserve pad numbers 10000 and above for bound pads.
       Characters can address the entire unsigned 16-bit normal-pad range. */
    /* Door indices omit +10000; other bound props must leave room for it
       within the signed 16-bit pad field. */
    if (padtotal > (door ? 32768u : bound ? 22768u : character ? SETUP_PAD_MAX : 10000u))
    {
        *reasonout = "There are no more pad indices available for this model.";
        return FALSE;
    }
    for (i = 0; i < 3; i++)
    {
        double value = position[i] * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000.0)
        {
            *reasonout = "The drop exceeds the setup coordinate range.";
            return FALSE;
        }
        authored[i] = (float)value;
        if (aimed)
        {
            /* Mounted CCTV faces along pad up. A drone's barrels point
             * along model -X, or minus the normal pad's side vector. */
            double direction = type == PROPDEF_CCTV ? orientation->up[i]
                : orientation->look[(i + 1) % 3] * orientation->up[(i + 2) % 3]
                  - orientation->look[(i + 2) % 3] * orientation->up[(i + 1) % 3];
            double aim = (position[i] + direction * 200.0) * levelscale;
            if (!isfinite(aim) || fabs(aim) > 100000000.0)
            { *reasonout = "The look-at pad exceeds the setup coordinate range."; return FALSE; }
            target[i] = (float)aim;
        }
    }
    if (aimed && target[0] == authored[0]
        && target[1] == authored[1] && target[2] == authored[2])
    { *reasonout = "The level scale cannot represent a separate look-at pad."; return FALSE; }
    if (character && !reused)
    {
        /* Append IDs after authored characters instead of filling holes that
           level scripts may intentionally reference. 248..255 are special AI
           IDs; 5000+ and 10000+ belong to spawned characters and clones. */
        for (i = 0; i < setup->charactercount; i++)
        {
            DWORD existing = setup->characters[i].chrnum;
            if (existing < 5000 && existing >= chrnum)
            {
                chrnum = existing + 1;
            }
        }
        if (chrnum >= 248 && chrnum <= 255)
        {
            chrnum = 256;
        }
        if (chrnum >= 5000)
        {
            *reasonout = "There are no more authored character IDs available.";
            return FALSE;
        }
    }

    oldcommands = SetupRead32(setup->data + SETUP_OBJECT_POINTER);
    commandend = oldcommands;
    if (oldcommands != 0)
    {
        if (oldcommands < SETUP_HEADER_SIZE || (oldcommands & 3))
        {
            goto malformed;
        }
        for (;;)
        {
            DWORD bytes;
            if (commandend > setup->size || setup->size - commandend < 4)
            {
                goto malformed;
            }
            if (setup->data[commandend + 3] == SETUP_PROP_END)
            {
                break;
            }
            if (++commandcount >= SETUP_OBJECT_MAX - (reused ? 0u : 1u))
            {
                *reasonout = "The setup command limit has been reached.";
                return FALSE;
            }
            bytes = SetupObjectWordCount(setup->data[commandend + 3]) * 4;
            if (bytes > setup->size - commandend)
            {
                goto malformed;
            }
            commandend += bytes;
        }
    }
    commandsize = commandend - oldcommands;
    oldpads = SetupRead32(setup->data + padheader);
    if (oldpads < SETUP_HEADER_SIZE || (oldpads & 3) || oldpads > setup->size ||
        padcount + 1 > (setup->size - oldpads) / padsize)
    {
        goto malformed;
    }

    newcommands = (setup->size + 3u) & ~3u;
    newrecord = newcommands + (reused ? reused - oldcommands : commandsize);
    newpads = newcommands + commandsize + (reused ? 0 : recordsize) + 4;
    newpad = newpads + padindex * padsize;
    added.size = newpads + (padtotal + 1) * padsize;
    if (added.size > SETUP_FILE_MAX)
    {
        *reasonout = "Adding this model would exceed the setup size limit.";
        return FALSE;
    }
    added.data = (unsigned char *)calloc(added.size, 1);
    if (added.data == NULL)
    {
        *reasonout = "Out of memory adding the setup model.";
        return FALSE;
    }
    memcpy(added.name, setup->name, sizeof(added.name));
    memcpy(added.data, setup->data, setup->size);
    memcpy(added.data + newcommands, setup->data + oldcommands, commandsize);
    memcpy(added.data + newpads, setup->data + oldpads, padcount * padsize);
    SetupWrite32(added.data + SETUP_OBJECT_POINTER, newcommands);
    SetupWrite32(added.data + padheader, newpads);
    SetupWrite32(added.data + newpads - 4, SETUP_PROP_END);
    memset(added.data + newrecord, 0, recordsize);
    memset(added.data + newpad, 0, padsize);
    SetupWrite32(added.data + newpad + 40, SETUP_PRIVATE_PAD_STAN);
    for (i = 0; i < 3; i++)
    {
        union
        {
            float f;
            DWORD u;
        } value;
        value.f = authored[i];
        SetupWrite32(added.data + newpad + i * 4, value.u);
    }
    SetupWrite32(added.data + newpad + 16, 0x3f800000u); /* up = +Y */
    SetupWrite32(added.data + newpad + 32, 0x3f800000u); /* look = +Z */
    /* Non-null pointer to an empty plink string; the game resolves the stan
       at the new position. The pad after all additions is the null terminator. */
    SetupWrite32(added.data + newpad + SETUP_PAD_LINK, newpads + padtotal * padsize + SETUP_PAD_LINK);

    if (orientation)
    {
        for (i = 0; i < 3; i++)
        {
            union { float f; DWORD u; } value;
            value.f = orientation->up[i]; SetupWrite32(added.data + newpad + 12 + i * 4, value.u);
            value.f = orientation->look[i]; SetupWrite32(added.data + newpad + 24 + i * 4, value.u);
        }
    }
    if (aimed)
    {
        DWORD aim = newpads + aimindex * padsize;
        memset(added.data + aim, 0, padsize);
        SetupWrite32(added.data + aim + 40, SETUP_PRIVATE_PAD_STAN);
        for (i = 0; i < 3; i++)
        {
            union { float f; DWORD u; } value;
            value.f = target[i]; SetupWrite32(added.data + aim + i * 4, value.u);
        }
        SetupWrite32(added.data + aim + 16, 0x3f800000u);
        SetupWrite32(added.data + aim + 32, 0x3f800000u);
        SetupWrite32(added.data + aim + SETUP_PAD_LINK, newpads + padtotal * padsize + SETUP_PAD_LINK);
    }

    if (bound)
    {
        const float bounds[6] = {bound->xmin, bound->xmax, bound->ymin,
            bound->ymax, bound->zmin, bound->zmax};
        for (i = 0; i < 3; i++)
        {
            union { float f; DWORD u; } value;
            value.f = bound->pad.up[i]; SetupWrite32(added.data + newpad + 12 + i * 4, value.u);
            value.f = bound->pad.look[i]; SetupWrite32(added.data + newpad + 24 + i * 4, value.u);
        }
        for (i = 0; i < 6; i++)
        {
            union { float f; DWORD u; } value;
            value.f = bounds[i]; SetupWrite32(added.data + newpad + SETUP_BOUNDPAD_BBOX + i * 4, value.u);
        }
    }
    if (door)
    {
        SetupWrite32(added.data + newrecord, (256u << 16) | PROPDEF_DOOR);
        SetupWrite32(added.data + newrecord + 4, ((DWORD)modelid << 16) | padindex);
        /* Register both adjacent rooms where possible, but don't close an
         * existing visibility portal merely because a new door is nearby. */
        SetupWrite32(added.data + newrecord + 8, PROPFLAG_FORCE_COLLISIONS | PROPFLAG_NO_PORTAL_CLOSE);
        SetupWrite32(added.data + newrecord + 0x74, 1000u << 16);
        /* Standalone, unlocked slider. All links, runtime pointers/caches and
         * exclusion flags start at zero in this complete 64-word DoorRecord. */
        SetupWrite32(added.data + newrecord + 0x84, 65536); /* 100% travel */
        SetupWrite32(added.data + newrecord + 0x88, 62259); /* 95% collision clearance */
        SetupWrite32(added.data + newrecord + 0x8c, 66); /* about 360%/s squared */
        SetupWrite32(added.data + newrecord + 0x90, 66);
        SetupWrite32(added.data + newrecord + 0x94, 1311); /* about 120%/s */
        SetupWrite32(added.data + newrecord + 0x98, DOORTYPE_SLIDING);
        SetupWrite32(added.data + newrecord + 0xa0, 5 * 60);
        SetupWrite32(added.data + newrecord + 0xa4, DOOR_OPEN_SOUND_METAL);
    }
    else if (character)
    {
        SetupWrite32(added.data + newrecord, type);
        SetupWrite32(added.data + newrecord + 4, (chrnum << 16) | padindex);
        /* GAILIST_DEAD_AI (1) yields forever. No weapons or patrol/mission
           behavior is implicitly assigned to a newly placed character. */
        SetupWrite32(added.data + newrecord + 8, ((DWORD)modelid << 16) | 1u);
        SetupWrite32(added.data + newrecord + 12, 0xffffffffu);          /* no presets */
        SetupWrite32(added.data + newrecord + 16, (1000u << 16) | 100u); /* hearing/vision */
        SetupWrite32(added.data + newrecord + 20, 0x0000ffffu);          /* random head */
    }
    else
    {
        /* Match stock armor and tank instance scales (unsigned 8.8). */
        DWORD extrascale = type == PROPDEF_ARMOUR ? 384u : type == PROPDEF_TANK ? 276u : 256u;
        SetupWrite32(added.data + newrecord, (extrascale << 16) | type);
        SetupWrite32(added.data + newrecord + 4, ((DWORD)modelid << 16) | (padindex + (bound ? 10000u : 0u)));
        if (type == PROPDEF_ARMOUR)
        {
            /* Armor is collectible by type; don't turn it into an obstacle
             * with FORCE_COLLISIONS or require the interaction button. */
            SetupWrite32(added.data + newrecord + 8, PROPFLAG_ALLOWFALL);
        }
        else if (type == PROPDEF_TANK)
        {
            /* Stock Runway tank: solid, invincible and floor grounded. */
            SetupWrite32(added.data + newrecord + 8,
                PROPFLAG_ALLOWFALL | PROPFLAG_FORCE_COLLISIONS | PROPFLAG_INVINCIBLE);
        }
        else if (type == PROPDEF_GLASS)
        {
            /* Match ordinary breakable free-standing panes in the game:
             * block movement, permit AI sight, and fit the flat window model
             * to the bound's width/height. No visibility portal is claimed.
             * GLASS uses a 32-word ObjectRecord in setup, not sizeof(GlassRecord). */
            SetupWrite32(added.data + newrecord + 8,
                PROPFLAG_FREE_STANDING_GLASS | PROPFLAG_FORCE_COLLISIONS | PROPFLAG_TRANSPARENT_TO_AI |
                PROPFLAG_ORTHOGONAL | PROPFLAG_ONSIDE | PROPFLAG_SCALE_TO_X_BOUNDS | PROPFLAG_SCALE_TO_Y_BOUNDS);
        }
        else if (type == PROPDEF_AUTOGUN)
        {
            /* Keep the base at the selected surface, including raised props,
             * without falling to the stan below or disabling the gun. */
            SetupWrite32(added.data + newrecord + 8, PROPFLAG_INAIR | PROPFLAG_ABSOLUTEPOSITION);
        }
        else if (orientation)
        {
            /* Match stock mounted cameras/alarms: anchor the model's Z-min
             * back face without grounding the prop or enabling falling. */
            SetupWrite32(added.data + newrecord + 8, PROPFLAG_ONSIDE);
        }
        else
        {
            SetupWrite32(added.data + newrecord + 8,
                         PROPFLAG_FORCE_COLLISIONS | PROPFLAG_ABSOLUTEPOSITION);
        }
        /* ObjectRecord.damage is authored as signed 16.16 durability and
           converted by domakedefaultobj. maxdamage starts at zero. */
        SetupWrite32(added.data + newrecord + 0x74, 1000u << 16);
        if (type == PROPDEF_ARMOUR)
        {
            /* Native 34-word BodyArmourRecord: setupLoadFiles converts the
             * signed 16.16 initialamount, then copies it to runtime amount.
             * 65536 gives full armor; writing float bits here is incorrect. */
            SetupWrite32(added.data + newrecord + 0x80, 65536);
        }
        else if (type == PROPDEF_TANK)
        {
            /* Native 56-word TankRecord. unkD8 is the tank's shell supply,
             * transferred to/from Bond on entry/exit; remaining fields are
             * collision caches, angles and other runtime state. */
            SetupWrite32(added.data + newrecord + 0xd8, 30);
        }
        else if (type == PROPDEF_CCTV)
        {
            /* Native 59-word CCTVRecord. setupCctv converts these signed
             * turn fractions; never write runtime floats/conversion state.
             * Sweep +/-45 degrees, about 30 degrees/sec, unlimited range. */
            SetupWrite32(added.data + newrecord + 0x80, aimindex);
            SetupWrite32(added.data + newrecord + 0xcc, 8192);
            SetupWrite32(added.data + newrecord + 0xd0, (DWORD)-8192);
            SetupWrite32(added.data + newrecord + 0xdc, 91);
        }
        else if (type == PROPDEF_AUTOGUN)
        {
            /* Native 54-word AutogunRecord. setupAutogun converts signed
             * 16.16 turns/metres; all runtime caches/pointers stay zero. */
            SetupWrite32(added.data + newrecord + 0x80, aimindex);
            SetupWrite32(added.data + newrecord + 0x88, 32768); /* +180 degrees */
            SetupWrite32(added.data + newrecord + 0x8c, (DWORD)-32768);
            SetupWrite32(added.data + newrecord + 0xa4, 0x111); /* Stock Control tracking speed. */
            SetupWrite32(added.data + newrecord + 0xa8, 20u << 16); /* 20 metres */
        }
    }
    if (!SetupParsePads(&added, reasonout) || !SetupParseObjects(&added, reasonout))
    {
        SetupFileFree(&added);
        return FALSE;
    }
    *selectionout = (DWORD)-1;
    for (i = 0; i < (character ? added.charactercount : added.objectcount); i++)
    {
        DWORD at = character ? added.characters[i].sourceoffset : added.objects[i].sourceoffset;
        if (at == newrecord) { *selectionout = i | (character ? SETUP_CHARACTER_SELECTION_BIT : 0); break; }
    }
    added.dirty = TRUE;
    added.actionmeta = setup->actionmeta; added.actionmetasize = setup->actionmetasize;
    added.globalrefs = setup->globalrefs; setup->globalrefs = NULL;
    setup->actionmeta = NULL;
    SetupFileFree(setup);
    *setup = added;
    return TRUE;

malformed:
    *reasonout = "The setup command or pad list is malformed.";
    return FALSE;
}

BOOL SetupFileAddModel(SetupFile *setup, BOOL character, int modelid, float levelscale,
                       const double position[3], DWORD *selectionout, const char **reasonout)
{
    return SetupAddPlacement(setup, character ? PROPDEF_GUARD : PROPDEF_PROP,
        modelid, levelscale, position, NULL, NULL, selectionout, reasonout);
}

BOOL SetupFileAddArmor(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], DWORD *selectionout,
                       const char **reasonout)
{
    return SetupAddPlacement(setup, PROPDEF_ARMOUR, modelid, levelscale,
        position, NULL, NULL, selectionout, reasonout);
}

BOOL SetupFileAddTank(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
                      DWORD *selectionout, const char **reasonout)
{
    SetupPad orientation = {0};
    double length;
    if (setup && strncmp(setup->name, "Ump_", 4) == 0)
    { *reasonout = "Tanks can only be placed in single-player levels."; return FALSE; }
    *reasonout = "The tank's facing direction is invalid.";
    if (!facing) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { if (!isfinite(facing[axis])) { return FALSE; } }
    length = hypot(facing[0], facing[2]);
    if (!isfinite(length)) { return FALSE; }
    orientation.up[1] = 1;
    orientation.look[0] = length > 1e-8 ? (float)(-facing[0] / length) : 0;
    orientation.look[2] = length > 1e-8 ? (float)(-facing[2] / length) : 1;
    return SetupAddPlacement(setup, PROPDEF_TANK, modelid, levelscale,
        position, NULL, &orientation, selectionout, reasonout);
}

BOOL SetupFileAddDoor(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
                      DWORD *selectionout, const char **reasonout)
{
    SetupBoundPad pad = {0};
    double length;
    *reasonout = "The door placement or level scale is invalid.";
    if (!facing || !isfinite(levelscale) || levelscale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { if (!isfinite(facing[axis])) { return FALSE; } }
    length = hypot(facing[0], facing[2]);
    if (!isfinite(length)) { return FALSE; }
    /* setupDoor maps model X/Y/Z to pad up/look/side. Put the base at the
     * drop's floor height, with the face towards the viewer. Looking straight
     * down uses a stable +X width axis. Dimensions are gameplay world units,
     * converted exactly once so Train and other scales get the same size. */
    pad.pad.up[0] = length > 1e-8 ? (float)(-facing[2] / length) : 1;
    pad.pad.up[2] = length > 1e-8 ? (float)(facing[0] / length) : 0;
    pad.pad.look[1] = 1;
    if (levelscale * 200.0 > 100000000.0 || levelscale * 6.0 < 0.000001)
    { *reasonout = "The level scale cannot represent the default door size."; return FALSE; }
    pad.xmin = -6 * levelscale; pad.xmax = 6 * levelscale; /* depth */
    pad.ymin = -50 * levelscale; pad.ymax = 50 * levelscale; /* width */
    pad.zmin = 0; pad.zmax = 200 * levelscale; /* height */
    return SetupAddPlacement(setup, PROPDEF_DOOR, modelid, levelscale, position, &pad, NULL, selectionout, reasonout);
}

BOOL SetupFileAddGlass(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], const double facing[3],
                       DWORD *selectionout, const char **reasonout)
{
    SetupBoundPad pad = {0};
    double length;
    *reasonout = "The glass placement or level scale is invalid.";
    if (!facing || !isfinite(levelscale) || levelscale <= 0) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { if (!isfinite(facing[axis])) { return FALSE; } }
    length = hypot(facing[0], facing[2]);
    if (!isfinite(length)) { return FALSE; }
    /* ONSIDE maps model X/Y/Z to -side/look/up. The stock pane lies in
     * model X/Y, so pad look is vertical and pad up faces the viewer.
     * Zero thickness matches PwindowZ's bounds, including after scaling. */
    pad.pad.up[0] = length > 1e-8 ? (float)(-facing[0] / length) : 0;
    pad.pad.up[2] = length > 1e-8 ? (float)(-facing[2] / length) : 1;
    pad.pad.look[1] = 1;
    if (levelscale * 200.0 > 100000000.0 || levelscale * 50.0 < 0.000001)
    { *reasonout = "The level scale cannot represent the default glass size."; return FALSE; }
    pad.xmin = -50 * levelscale; pad.xmax = 50 * levelscale;
    pad.zmin = 0; pad.zmax = 200 * levelscale;
    return SetupAddPlacement(setup, PROPDEF_GLASS, modelid, levelscale, position, &pad, NULL, selectionout, reasonout);
}

static BOOL SetupAddMountedObject(SetupFile *setup, unsigned char type, int modelid, float levelscale,
                                  const double position[3], const double facing[3],
                                  DWORD *selectionout, const char **reasonout)
{
    SetupPad mount = {0};
    double length;
    *reasonout = "The mounted object's facing direction is invalid.";
    if (!facing) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { if (!isfinite(facing[axis])) { return FALSE; } }
    length = hypot(facing[0], facing[2]);
    if (!isfinite(length)) { return FALSE; }
    /* ONSIDE maps model +Z to pad up and model +Y to pad look. Keep the
     * device upright even when looking down; its front faces the viewer. */
    mount.up[0] = length > 1e-8 ? (float)(-facing[0] / length) : 0;
    mount.up[2] = length > 1e-8 ? (float)(-facing[2] / length) : 1;
    mount.look[1] = 1;
    return SetupAddPlacement(setup, type, modelid, levelscale, position, NULL, &mount, selectionout, reasonout);
}

BOOL SetupFileAddCctv(SetupFile *setup, int modelid, float levelscale,
                      const double position[3], const double facing[3],
                      DWORD *selectionout, const char **reasonout)
{
    return SetupAddMountedObject(setup, PROPDEF_CCTV, modelid, levelscale, position, facing, selectionout, reasonout);
}

BOOL SetupFileAddAlarm(SetupFile *setup, int modelid, float levelscale,
                       const double position[3], const double facing[3],
                       DWORD *selectionout, const char **reasonout)
{
    return SetupAddMountedObject(setup, PROPDEF_ALARM, modelid, levelscale, position, facing, selectionout, reasonout);
}

BOOL SetupFileAddDroneGun(SetupFile *setup, int modelid, float levelscale,
                          const double position[3], const double facing[3],
                          DWORD *selectionout, const char **reasonout)
{
    SetupPad orientation = {0};
    double length;
    *reasonout = "The drone gun's facing direction is invalid.";
    if (!facing) { return FALSE; }
    for (int axis = 0; axis < 3; axis++) { if (!isfinite(facing[axis])) { return FALSE; } }
    length = hypot(facing[0], facing[2]);
    if (!isfinite(length)) { return FALSE; }
    /* Ground gun's barrel direction is model -X. Keep Y upright and rotate
     * that forward axis towards the viewer. Vertical views fall back to +Z. */
    orientation.up[1] = 1;
    orientation.look[0] = length > 1e-8 ? (float)(-facing[2] / length) : 1;
    orientation.look[2] = length > 1e-8 ? (float)(facing[0] / length) : 0;
    return SetupAddPlacement(setup, PROPDEF_AUTOGUN, modelid, levelscale, position,
        NULL, &orientation, selectionout, reasonout);
}

/* Copy the live intro list into appended storage. Keeping the old bytes and
 * pad tables preserves file-relative credits/AI links and shared pad indices.
 * Only the active header pointers change; no editor-only command is exported. */
static BOOL SetupEditSpawns(SetupFile *setup, const SetupMarkerRef *remove,
                           float levelscale, const double position[3],
                           SetupMarkerRef *out, const char **reasonout)
{
    SetupFile edited = {0};
    DWORD intro, at, end, command, count = 0, first = 0, firstcommand = 0;
    DWORD oldpads, newintro, write, written = 0, selected = 0, newpads, newpad;
    DWORD padindex = 0, padtotal;
    BOOL placing = position != NULL, multiplayer, found = FALSE, placed = FALSE;
    float native[3], look[3] = {0, 0, 1};
    *reasonout = "The setup's spawn records are invalid.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || setup->size > SETUP_FILE_MAX
        || (placing && (!out || !isfinite(levelscale) || levelscale <= 0))
        || (!placing && (!remove || remove->kind != SETUP_MARKER_SPAWN))) { return FALSE; }
    multiplayer = strncmp(setup->name, "Ump_", 4) == 0;
    intro = SetupRead32(setup->data + 8);
    at = intro;
    for (command = 0; intro && command < SETUP_OBJECT_MAX; command++)
    {
        DWORD type, bytes;
        if (at < SETUP_HEADER_SIZE || (at & 3) || at > setup->size || setup->size - at < 4) { return FALSE; }
        type = SetupRead32(setup->data + at);
        if (type == 9) { break; }
        bytes = SetupIntroWordCount(type) * 4;
        if (!bytes || bytes > setup->size - at) { return FALSE; }
        if (type == 0 && SetupRead32(setup->data + at + 8) == 0)
        {
            DWORD pad = SetupRead32(setup->data + at + 4);
            if (!setup->pads || pad >= setup->padcount) { return FALSE; }
            if (!count) { first = at; firstcommand = command; }
            count++;
            if (remove && remove->command == command) { found = TRUE; }
        }
        at += bytes;
    }
    if (command >= SETUP_OBJECT_MAX) { return FALSE; }
    end = at;
    if (!placing)
    {
        if (!found) { return FALSE; }
        if (count <= 1)
        { *reasonout = "The level must keep at least one spawn point."; return FALSE; }
    }
    else
    {
        /* g_Startpad in bondview.c has 16 entries, including all normal-play
         * starts loaded for a multiplayer setup. Never overflow that array. */
        if (multiplayer && count >= 16)
        { *reasonout = "The game supports at most 16 multiplayer spawn points."; return FALSE; }
        if (command >= SETUP_OBJECT_MAX - 1)
        { *reasonout = "The setup has no room for another spawn point."; return FALSE; }
        for (int axis = 0; axis < 3; axis++)
        {
            double value = position[axis] * levelscale;
            if (!isfinite(value) || fabs(value) > 100000000.0)
            { *reasonout = "The spawn position exceeds the setup coordinate range."; return FALSE; }
            native[axis] = (float)value;
        }
        if (!multiplayer && count)
        {
            const SetupPad *pad = &setup->pads[SetupRead32(setup->data + first + 4)];
            float length = hypotf(pad->look[0], pad->look[2]);
            if (!isfinite(length)) { return FALSE; }
            if (length > 0) { look[0] = pad->look[0] / length; look[2] = pad->look[2] / length; }
        }
    }
    oldpads = SetupRead32(setup->data + SETUP_PAD_POINTER);
    if (placing && (oldpads < SETUP_HEADER_SIZE || oldpads > setup->size
        || setup->padcount + 1 > (setup->size - oldpads) / SETUP_PAD_SIZE)) { return FALSE; }
    padtotal = setup->padcount;
    if (placing)
    {
        const char *unused;
        SetupPadRef ref = {count ? SetupRead32(setup->data + first + 4) : SETUP_PAD_INDEX_NONE, FALSE};
        padindex = !multiplayer && count && SetupPadUnusedExcept(setup, &ref, NULL, first + 4, &unused)
            ? ref.index : SetupFindFreePad(setup, FALSE, (DWORD)-1);
        if (padindex >= padtotal) { padtotal = padindex + 1; }
        if (padtotal > SETUP_PAD_MAX) { *reasonout = "The setup has no room for another spawn pad."; return FALSE; }
    }
    newintro = (setup->size + 3u) & ~3u;
    edited.size = newintro + (end - intro) + 4 + (placing ? 12 + (padtotal + 1) * SETUP_PAD_SIZE : 0);
    if (edited.size > SETUP_FILE_MAX)
    { *reasonout = "The spawn edit would exceed the setup size limit."; return FALSE; }
    edited.data = calloc(edited.size, 1);
    if (!edited.data) { *reasonout = "Out of memory editing spawn points."; return FALSE; }
    memcpy(edited.name, setup->name, sizeof(edited.name));
    memcpy(edited.data, setup->data, setup->size);
    write = newintro;
    for (at = intro, command = 0; at < end; command++)
    {
        DWORD type = SetupRead32(setup->data + at);
        DWORD bytes = SetupIntroWordCount(type) * 4;
        BOOL normal = type == 0 && SetupRead32(setup->data + at + 8) == 0;
        BOOL skip = normal && ((!placing && command == remove->command)
            || (placing && !multiplayer && command != firstcommand));
        if (!skip)
        {
            memcpy(edited.data + write, setup->data + at, bytes);
            if (normal && placing && !multiplayer)
            {
                SetupWrite32(edited.data + write + 4, padindex);
                selected = written; placed = TRUE;
            }
            if (type == 3 && placing && !multiplayer)
            {
                /* Authored room hints (notably Dam) refer to the old area.
                   -1 makes the game trace from the new player's collision
                   position, without moving pads shared with other records. */
                SetupWrite32(edited.data + write + 28, 0xffffffffu);
            }
            write += bytes; written++;
        }
        at += bytes;
    }
    if (placing && !placed)
    {
        SetupWrite32(edited.data + write, 0);
        SetupWrite32(edited.data + write + 4, padindex);
        selected = written;
        write += 12;
    }
    SetupWrite32(edited.data + write, 9);
    SetupWrite32(edited.data + 8, newintro);
    edited.size = write + 4;
    if (placing)
    {
        newpads = edited.size;
        newpad = newpads + padindex * SETUP_PAD_SIZE;
        memcpy(edited.data + newpads, setup->data + oldpads, setup->padcount * SETUP_PAD_SIZE);
        SetupWrite32(edited.data + SETUP_PAD_POINTER, newpads);
        memset(edited.data + newpad, 0, SETUP_PAD_SIZE);
        SetupWrite32(edited.data + newpad + 40, SETUP_PRIVATE_PAD_STAN);
        for (int axis = 0; axis < 3; axis++)
        {
            union { float f; DWORD u; } value;
            value.f = native[axis]; SetupWrite32(edited.data + newpad + axis * 4, value.u);
            value.f = look[axis]; SetupWrite32(edited.data + newpad + 24 + axis * 4, value.u);
        }
        SetupWrite32(edited.data + newpad + 16, 0x3f800000u); /* up = +Y */
        SetupWrite32(edited.data + newpad + SETUP_PAD_LINK, newpads + padtotal * SETUP_PAD_SIZE + SETUP_PAD_LINK);
        edited.size = newpads + (padtotal + 1) * SETUP_PAD_SIZE;
    }
    if (!SetupParsePads(&edited, reasonout) || !SetupParseObjects(&edited, reasonout))
    { SetupFileFree(&edited); return FALSE; }
    edited.dirty = TRUE;
    edited.actionmeta = setup->actionmeta; edited.actionmetasize = setup->actionmetasize;
    edited.globalrefs = setup->globalrefs; setup->globalrefs = NULL;
    setup->actionmeta = NULL;
    SetupFileFree(setup); *setup = edited;
    if (placing) { out->kind = SETUP_MARKER_SPAWN; out->command = selected; }
    *reasonout = "";
    return TRUE;
}

BOOL SetupFilePlaceSpawn(SetupFile *setup, float levelscale, const double position[3],
                        SetupMarkerRef *out, const char **reasonout)
{
    if (!position) { *reasonout = "No spawn position was supplied."; return FALSE; }
    return SetupEditSpawns(setup, NULL, levelscale, position, out, reasonout);
}

BOOL SetupFileDeleteSpawn(SetupFile *setup, const SetupMarkerRef *ref, const char **reasonout)
{
    return SetupEditSpawns(setup, ref, 1, NULL, NULL, reasonout);
}

void SetupPadGetBoxCorners(const SetupPad *pad,
                           float xmin, float xmax,
                           float ymin, float ymax,
                           float zmin, float zmax,
                           float worldscale,
                           float corners[8][3])
{
    float side[3];
    int corner;

    side[0] = pad->up[1] * pad->look[2] - pad->up[2] * pad->look[1];
    side[1] = pad->up[2] * pad->look[0] - pad->up[0] * pad->look[2];
    side[2] = pad->up[0] * pad->look[1] - pad->up[1] * pad->look[0];

    for (corner = 0; corner < 8; corner++)
    {
        float x = (corner & 1) ? xmax : xmin;
        float y = (corner & 2) ? ymax : ymin;
        float z = (corner & 4) ? zmax : zmin;
        int axis;

        for (axis = 0; axis < 3; axis++)
        {
            corners[corner][axis] = (pad->pos[axis]
                + side[axis] * x
                + pad->up[axis] * y
                + pad->look[axis] * z) * worldscale;
        }
    }
}

static BOOL SetupResourceNameIsValid(const char *name)
{
    size_t length = strlen(name);
    size_t i;

    if ((strncmp(name, "Usetup", 6) != 0
         && strncmp(name, "Ump_setup", 9) != 0)
        || length == 0 || name[length - 1] != 'Z')
    {
        return FALSE;
    }

    /* Project metadata is editable text. Restrict the filename to the
       resource-name alphabet so it can never escape the setup folder. */
    for (i = 0; i < length; i++)
    {
        char ch = name[i];

        if (!((ch >= 'A' && ch <= 'Z')
              || (ch >= 'a' && ch <= 'z')
              || (ch >= '0' && ch <= '9')
              || ch == '_'))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static BOOL SetupProjectPath(char *path, size_t pathsize,
                             const char *projectdir,
                             const char *setupname)
{
    int written;

    if (!SetupResourceNameIsValid(setupname))
    {
        return FALSE;
    }

    written = snprintf(path, pathsize, "%s\\setup\\%s.set",
                       projectdir, setupname);
    return written >= 0 && written < (int)pathsize;
}

DWORD SetupExtractAll(const RomFile *rom, const char *projectdir,
                      const char **reasonout)
{
    char dir[MAX_PATH];
    DWORD count = 0;
    DWORD index;
    int written;

    *reasonout = "";

    written = snprintf(dir, sizeof(dir), "%s\\setup", projectdir);
    if (written < 0 || written >= (int)sizeof(dir))
    {
        *reasonout = "the setup folder path is too long.";
        return 0;
    }

    if (!CreateDirectory(dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        *reasonout = "the setup folder could not be created.";
        return 0;
    }

    for (index = 0; ; index++)
    {
        char name[64];
        char path[MAX_PATH];
        DWORD offset;
        DWORD length;
        DWORD output;
        HANDLE file;
        BOOL ok;

        if (!RomGetFileByIndex(rom, index, name, sizeof(name), NULL, NULL))
        {
            break;
        }

        if (!SetupResourceNameIsValid(name))
        {
            continue;
        }

        if (!RomGetFileByIndex(rom, index, name, sizeof(name),
                               &offset, &length))
        {
            *reasonout = "a setup file-table entry is invalid.";
            return 0;
        }

        if (!SetupProjectPath(path, sizeof(path), projectdir, name))
        {
            *reasonout = "a setup output path is too long.";
            return 0;
        }

        file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            *reasonout = "a setup file could not be created.";
            return 0;
        }

        ok = WriteFile(file, rom->data + offset, length, &output, NULL)
          && output == length;
        CloseHandle(file);

        if (!ok)
        {
            *reasonout = "a setup file could not be fully written.";
            return 0;
        }

        count++;
    }

    if (count == 0)
    {
        *reasonout = "the ROM file table contains no setup files.";
    }

    return count;
}

BOOL SetupLoadProjectFile(const char *projectdir, const char *setupname,
                          SetupFile *out, const char **reasonout)
{
    char path[MAX_PATH];
    HANDLE file;
    DWORD got;

    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (!SetupProjectPath(path, sizeof(path), projectdir, setupname))
    {
        *reasonout = "the setup resource name or path is invalid.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        /* These multiplayer-only stages have placeholder solo names in
         * g_LevelInfoTable. Their actual resources are Ump_setup*. Keep the
         * resolved name in SetupFile so Save writes the correct ROM asset.
         * Never substitute a multiplayer setup for a missing solo mission
         * or for an existing but damaged setup. */
        static const char *const multiplayeronly[] = {
            "UsetuprefZ", "UsetupdishZ", "UsetupimpZ",
            "UsetupashZ", "UsetupameZ", "UsetupoatZ"
        };
        DWORD i;
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            for (i = 0; i < sizeof(multiplayeronly) / sizeof(multiplayeronly[0]); i++)
            {
                if (strcmp(setupname, multiplayeronly[i]) == 0)
                {
                    char name[64];
                    snprintf(name, sizeof(name), "Ump_%s", setupname + 1);
                    return SetupLoadProjectFile(projectdir, name, out, reasonout);
                }
            }
        }
        *reasonout = "the setup .set file is missing from this project.";
        return FALSE;
    }

    out->size = GetFileSize(file, NULL);
    if (out->size == INVALID_FILE_SIZE || out->size == 0
        || out->size > SETUP_FILE_MAX + SETUP_META_MAX + SETUP_META_FOOTER)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "the project setup file is empty, too large, or unreadable.";
        return FALSE;
    }

    out->data = (unsigned char *)malloc(out->size);
    if (out->data == NULL)
    {
        CloseHandle(file);
        ZeroMemory(out, sizeof(*out));
        *reasonout = "out of memory reading the project setup.";
        return FALSE;
    }

    if (!ReadFile(file, out->data, out->size, &got, NULL)
        || got != out->size)
    {
        CloseHandle(file);
        SetupFileFree(out);
        *reasonout = "the project setup file could not be fully read.";
        return FALSE;
    }

    CloseHandle(file);

    {
        DWORD native, meta;
        if (!SetupMetaSplit(out->data, out->size, &native, &meta) || native > SETUP_FILE_MAX)
        { SetupFileFree(out); *reasonout = "the setup metadata trailer is invalid."; return FALSE; }
        if (meta)
        {
            out->actionmeta = malloc(meta);
            if (!out->actionmeta)
            { SetupFileFree(out); *reasonout = "out of memory reading setup notes."; return FALSE; }
            memcpy(out->actionmeta, out->data + native, meta);
            out->actionmetasize = meta;
        }
        out->size = native;
    }

    if (!SetupFileCompact(out, reasonout)
        || !SetupParsePads(out, reasonout)
        || !SetupParseObjects(out, reasonout))
    {
        SetupFileFree(out);
        return FALSE;
    }

    strncpy(out->name, setupname, sizeof(out->name) - 1);
    return TRUE;
}


BOOL SetupSaveProjectFile(const char *projectdir, const SetupFile *setup,
                          const char **reasonout)
{
    char path[MAX_PATH], temporary[MAX_PATH];
    unsigned char footer[SETUP_META_FOOTER];
    unsigned char *packed;
    DWORD packedsize;
    HANDLE file;
    DWORD written;
    BOOL ok;
    *reasonout = "";
    if (!setup || !setup->data || !setup->size || setup->actionmetasize > SETUP_META_MAX
        || (setup->actionmetasize && !setup->actionmeta)
        || !SetupProjectPath(path, sizeof(path), projectdir, setup->name)
        || snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int)sizeof(temporary))
    { *reasonout = "there is no valid setup loaded to save."; return FALSE; }
    if (!SetupCompactNative(setup->data, setup->size, &packed, &packedsize, reasonout)) { return FALSE; }
    file = CreateFile(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    { free(packed); *reasonout = "the temporary setup file could not be opened for writing."; return FALSE; }
    ok = WriteFile(file, packed, packedsize, &written, NULL) && written == packedsize;
    free(packed);
    if (ok && setup->actionmetasize)
    {
        memcpy(footer, SETUP_META_MAGIC, 8);
        SetupMetaWrite32(footer + 8, packedsize);
        SetupMetaWrite32(footer + 12, setup->actionmetasize);
        ok = WriteFile(file, setup->actionmeta, setup->actionmetasize, &written, NULL)
          && written == setup->actionmetasize;
        if (ok) { ok = WriteFile(file, footer, sizeof(footer), &written, NULL) && written == sizeof(footer); }
    }
    if (!CloseHandle(file)) { ok = FALSE; }
    if (ok) { ok = MoveFileEx(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH); }
    if (!ok)
    { DeleteFile(temporary); *reasonout = "the setup could not be saved; the previous file was retained."; }
    return ok;
}

BOOL SetupFileClone(const SetupFile *source, SetupFile *out,
                    const char **reasonout)
{
    ZeroMemory(out, sizeof(*out));
    *reasonout = "";

    if (source == NULL)
    {
        *reasonout = "there is no setup to copy.";
        return FALSE;
    }
    if ((source->size > 0 && source->data == NULL)
        || (source->padcount > 0 && source->pads == NULL)
        || (source->boundpadcount > 0 && source->boundpads == NULL)
        || (source->objectcount > 0 && source->objects == NULL)
        || (source->charactercount > 0 && source->characters == NULL)
        || (source->actionmetasize > 0 && source->actionmeta == NULL))
    {
        *reasonout = "the setup document is incomplete.";
        return FALSE;
    }

    if (source->size > 0)
    {
        out->data = (unsigned char *)malloc(source->size);
    }
    if (source->actionmetasize)
    {
        out->actionmeta = malloc(source->actionmetasize);
        if (!out->actionmeta)
        { SetupFileFree(out); *reasonout = "out of memory copying setup notes."; return FALSE; }
        memcpy(out->actionmeta, source->actionmeta, source->actionmetasize);
        out->actionmetasize = source->actionmetasize;
    }
    if (source->padcount > 0)
    {
        out->pads = (SetupPad *)malloc(
            (size_t)source->padcount * sizeof(*out->pads));
    }
    if (source->boundpadcount > 0)
    {
        out->boundpads = (SetupBoundPad *)malloc(
            (size_t)source->boundpadcount * sizeof(*out->boundpads));
    }
    if (source->objectcount > 0)
    {
        out->objects = (SetupObject *)malloc(
            (size_t)source->objectcount * sizeof(*out->objects));
    }
    if (source->charactercount > 0)
    {
        out->characters = (SetupCharacter *)malloc(
            (size_t)source->charactercount * sizeof(*out->characters));
    }

    if ((source->size > 0 && out->data == NULL)
        || (source->padcount > 0 && out->pads == NULL)
        || (source->boundpadcount > 0 && out->boundpads == NULL)
        || (source->objectcount > 0 && out->objects == NULL)
        || (source->charactercount > 0 && out->characters == NULL))
    {
        SetupFileFree(out);
        *reasonout = "out of memory copying the setup document.";
        return FALSE;
    }

    if (source->size > 0)
    {
        memcpy(out->data, source->data, source->size);
    }
    if (source->padcount > 0)
    {
        memcpy(out->pads, source->pads,
               (size_t)source->padcount * sizeof(*out->pads));
    }
    if (source->boundpadcount > 0)
    {
        memcpy(out->boundpads, source->boundpads,
               (size_t)source->boundpadcount * sizeof(*out->boundpads));
    }
    if (source->objectcount > 0)
    {
        memcpy(out->objects, source->objects,
               (size_t)source->objectcount * sizeof(*out->objects));
    }
    if (source->charactercount > 0)
    {
        memcpy(out->characters, source->characters,
               (size_t)source->charactercount * sizeof(*out->characters));
    }

    out->size = source->size;
    out->padcount = source->padcount;
    out->boundpadcount = source->boundpadcount;
    out->objectcount = source->objectcount;
    out->charactercount = source->charactercount;
    out->dirty = source->dirty;
    out->globalrefs = source->globalrefs;
    if (out->globalrefs) { out->globalrefs->owners++; }
    lstrcpyn(out->name, source->name, sizeof(out->name));
    return TRUE;
}


BOOL SetupFileSetObjectFlag(SetupFile *setup, DWORD objectindex,
                            unsigned int bank, DWORD mask, BOOL enabled,
                            BOOL *changedout, const char **reasonout)
{
    SetupObject *object;
    unsigned char *record;
    DWORD value, previous;
    *changedout = FALSE;
    *reasonout = "the selected setup object is invalid.";
    if (!setup || !setup->data || !setup->objects || objectindex >= setup->objectcount
        || (objectindex & SETUP_CHARACTER_SELECTION_BIT)) { return FALSE; }
    if (bank > 1 || mask == 0 || (mask & (mask - 1)) != 0)
    { *reasonout = "select one PROPFLAG or PROPFLAG2 bit."; return FALSE; }
    object = &setup->objects[objectindex];
    if (object->sourceoffset < SETUP_HEADER_SIZE || object->sourceoffset > setup->size
        || setup->size - object->sourceoffset < 16)
    { *reasonout = "the selected setup object's source record is invalid."; return FALSE; }
    record = setup->data + object->sourceoffset;
    if (record[3] != object->type || !SetupTypeCreatesObject(record[3])
        || SetupObjectWordCount(record[3]) * 4 > setup->size - object->sourceoffset
        || SetupRead32(record + 8) != object->flags || SetupRead32(record + 12) != object->flags2)
    { *reasonout = "the selected setup object's source record is inconsistent."; return FALSE; }
    previous = bank ? object->flags2 : object->flags;
    value = enabled ? previous | mask : previous & ~mask;
    *reasonout = "";
    if (value == previous) { return TRUE; }
    SetupWrite32(record + 8 + bank * 4, value);
    if (bank) { object->flags2 = value; }
    else { object->flags = value; }
    /* Keep live state identical to reloading this setup. The editor also uses
       the five mode-exclusion bits together for Delete Object tombstones. */
    object->deleted = (object->flags2 & SETUP_OBJECT_DELETED_FLAGS2) == SETUP_OBJECT_DELETED_FLAGS2;
    setup->dirty = TRUE;
    *changedout = TRUE;
    return TRUE;
}

BOOL SetupFileDeleteObject(SetupFile *setup, DWORD objectindex,
                           const char **reasonout)
{
    SetupObject *object;
    DWORD flags2;

    *reasonout = "";
    if (setup == NULL || setup->data == NULL
        || objectindex >= setup->objectcount)
    {
        *reasonout = "the selected setup object is invalid.";
        return FALSE;
    }

    object = &setup->objects[objectindex];
    if (object->deleted)
    {
        *reasonout = "the selected setup object is already deleted.";
        return FALSE;
    }
    if (object->sourceoffset > setup->size
        || setup->size - object->sourceoffset < 16)
    {
        *reasonout = "the selected setup object's source record is invalid.";
        return FALSE;
    }

    flags2 = object->flags2 | SETUP_OBJECT_DELETED_FLAGS2;
    SetupWrite32(setup->data + object->sourceoffset + 12, flags2);
    object->flags2 = flags2;
    object->deleted = TRUE;
    setup->dirty = TRUE;
    return TRUE;
}

BOOL SetupFileDeleteCharacter(SetupFile *setup, DWORD characterindex,
                              const char **reasonout)
{
    SetupCharacter *character;
    *reasonout = "The selected setup character is invalid.";
    if (setup == NULL || setup->data == NULL || setup->characters == NULL
        || characterindex >= setup->charactercount) { return FALSE; }
    character = &setup->characters[characterindex];
    if (character->deleted)
    { *reasonout = "The selected setup character is already deleted."; return FALSE; }
    if (character->sourceoffset > setup->size || setup->size - character->sourceoffset < 28
        || setup->data[character->sourceoffset + 3] != PROPDEF_GUARD)
    { *reasonout = "The selected setup character's source record is invalid."; return FALSE; }
    SetupWrite32(setup->data + character->sourceoffset, SETUP_DELETED_CHARACTER_HEADER);
    character->deleted = TRUE;
    setup->dirty = TRUE;
    *reasonout = "";
    return TRUE;
}

BOOL SetupFileTranslatePad(SetupFile *setup, const SetupPadRef *ref,
                            float levelscale, const double offset[3],
                            BOOL *changedout, const char **reasonout)
{
    DWORD table, stride, count, record, terminator;
    SetupPad *pad;
    float position[3];
    int axis;

    *changedout = FALSE;
    *reasonout = "The selected pad is invalid.";
    if (setup == NULL || ref == NULL || offset == NULL || setup->data == NULL
        || setup->size < SETUP_HEADER_SIZE || !isfinite(levelscale) || levelscale <= 0) { return FALSE; }
    count = ref->bound ? setup->boundpadcount : setup->padcount;
    stride = ref->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    table = SetupRead32(setup->data + (ref->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    if (ref->index >= count || count > SETUP_PAD_MAX || table < SETUP_HEADER_SIZE
        || table > setup->size || count + 1 > (setup->size - table) / stride
        || (ref->bound ? setup->boundpads == NULL : setup->pads == NULL)) { return FALSE; }
    record = table + ref->index * stride;
    terminator = table + count * stride;
    if (SetupRead32(setup->data + terminator + SETUP_PAD_LINK) != 0) { return FALSE; }
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    for (axis = 0; axis < 3; axis++)
    {
        double value = pad->pos[axis] + offset[axis] * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000.0)
        {
            *reasonout = "The move exceeds the setup coordinate range.";
            return FALSE;
        }
        position[axis] = (float)value;
    }
    for (axis = 0; axis < 3; axis++)
    {
        if (position[axis] != pad->pos[axis]) { *changedout = TRUE; }
    }
    if (*changedout)
    {
        for (axis = 0; axis < 3; axis++)
        {
            union { float f; DWORD u; } value;
            value.f = position[axis];
            pad->pos[axis] = value.f;
            SetupWrite32(setup->data + record + axis * 4, value.u);
        }
        /* Point to an existing null byte without editing a stan-name string
         * that other pads may share. A non-null plink keeps this record alive. */
        SetupWrite32(setup->data + record + SETUP_PAD_LINK, terminator + SETUP_PAD_LINK);
        pad->stanname[0] = '\0';
        setup->dirty = TRUE;
    }
    *reasonout = "";
    return TRUE;
}

BOOL SetupFileSetPadStanName(SetupFile *setup, const SetupPadRef *ref,
    const char *name, const char **reasonout)
{
    DWORD count, stride, table, record, start, size;
    size_t length;
    SetupPad *pad;
    unsigned char *data;
    *reasonout = "The placement pad or Stan tile name is invalid.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE
        || setup->size > SETUP_FILE_MAX || !ref || !name)
    { return FALSE; }
    for (length = 0; length < sizeof(pad->stanname) && name[length]; length++) {}
    if (!length || length >= sizeof(pad->stanname)) { return FALSE; }
    count = ref->bound ? setup->boundpadcount : setup->padcount;
    stride = ref->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    table = SetupRead32(setup->data + (ref->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    if (ref->index >= count || count > SETUP_PAD_MAX || table < SETUP_HEADER_SIZE
        || table > setup->size || count + 1 > (setup->size - table) / stride
        || (ref->bound ? !setup->boundpads : !setup->pads)) { return FALSE; }
    record = table + ref->index * stride;
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    if (pad->deleted || !SetupRead32(setup->data + record + SETUP_PAD_LINK)) { return FALSE; }
    if (!strcmp(pad->stanname, name)) { *reasonout = ""; return TRUE; }
    start = (setup->size + 3u) & ~3u;
    size = (start + (DWORD)length + 1u + 3u) & ~3u;
    if (size > SETUP_FILE_MAX)
    { *reasonout = "The setup has no room for the placement's Stan tile name."; return FALSE; }
    data = calloc(size, 1);
    if (!data) { *reasonout = "Out of memory retaining the placement's Stan tile."; return FALSE; }
    memcpy(data, setup->data, setup->size);
    memcpy(data + start, name, length + 1);
    SetupWrite32(data + record + SETUP_PAD_LINK, start);
    /* Names may be shared by other pads: never overwrite the old string.
     * Transaction compaction discards obsolete names before retaining history. */
    memcpy(pad->stanname, name, length + 1);
    free(setup->data); setup->data = data; setup->size = size;
    setup->dirty = TRUE; *reasonout = ""; return TRUE;
}

/* Reuse a model's pad if no other consumer refers to it; otherwise detach
 * its placement first. Physical table order is unrelated to pad ownership. */
BOOL SetupFileTranslateModel(SetupFile *setup, DWORD selection,
                              float levelscale, const double offset[3],
                              const char **reasonout)
{
    SetupObject *object = NULL;
    SetupCharacter *character = NULL;
    SetupPad *pad;
    DWORD header, stride, count, table, index, record, end;
    DWORD owner = selection & ~SETUP_CHARACTER_SELECTION_BIT;
    DWORD sourceoffset, maxindex;
    BOOL bound, door, reuse;
    float position[3];
    int axis;
    *reasonout = "The selected model has no editable placement pad.";
    if (setup == NULL || setup->data == NULL || setup->size < SETUP_HEADER_SIZE
        || offset == NULL || !isfinite(levelscale) || levelscale <= 0) { return FALSE; }
    if (selection & SETUP_CHARACTER_SELECTION_BIT)
    {
        if (setup->characters == NULL || owner >= setup->charactercount) { return FALSE; }
        character = &setup->characters[owner];
        if (character->deleted || character->sourceoffset > setup->size - 28
            || setup->data[character->sourceoffset + 3] != PROPDEF_GUARD) { return FALSE; }
        sourceoffset = character->sourceoffset;
        door = bound = FALSE;
        index = character->pad;
        maxindex = 65535; /* GuardRecord.PadID is an unsigned ordinary-pad index. */
    }
    else
    {
        if (setup->objects == NULL || owner >= setup->objectcount) { return FALSE; }
        object = &setup->objects[owner];
        if (object->deleted || object->pad < 0 || object->sourceoffset > setup->size - 16) { return FALSE; }
        sourceoffset = object->sourceoffset;
        door = object->type == PROPDEF_DOOR;
        bound = door || object->pad >= 10000;
        index = (DWORD)object->pad - (bound && !door ? 10000 : 0);
        maxindex = bound ? (door ? 32767 : 22767) : 9999;
    }
    count = bound ? setup->boundpadcount : setup->padcount;
    header = bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER;
    stride = bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    table = SetupRead32(setup->data + header);
    if (index >= count || count > SETUP_PAD_MAX || table > setup->size
        || (bound ? setup->boundpads == NULL : setup->pads == NULL)
        || (count + 1) > (setup->size - table) / stride) { return FALSE; }
    record = table + index * stride;
    end = table + count * stride;
    pad = bound ? &setup->boundpads[index].pad : &setup->pads[index];
    for (axis = 0; axis < 3; axis++)
    {
        double value = pad->pos[axis] + offset[axis] * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000.0)
        {
            *reasonout = "The move exceeds the setup coordinate range.";
            return FALSE;
        }
        position[axis] = (float)value;
    }
    /* Ownership follows references, not the pad's physical byte position. */
    {
        SetupPadRef ref = {index, bound};
        reuse = SetupPadUnusedExcept(setup, &ref, NULL, sourceoffset + 6, reasonout);
    }
    if (!reuse)
    {
        DWORD newindex = SetupFindFreePad(setup, bound, index);
        DWORD newcount = newindex < count ? count : newindex + 1;
        DWORD newtable = (setup->size + 3) & ~3u;
        DWORD newsize = newtable + (newcount + 1) * stride;
        unsigned char *data;
        void *pads;
        DWORD encoded = newindex + (bound && !door ? 10000 : 0);
        if (newindex > maxindex || newsize > SETUP_FILE_MAX)
        {
            *reasonout = "The setup has no room for another placement pad.";
            return FALSE;
        }
        data = calloc(newsize, 1);
        pads = malloc((size_t)newcount * (bound ? sizeof(SetupBoundPad) : sizeof(SetupPad)));
        if (data == NULL || pads == NULL)
        {
            free(data); free(pads); *reasonout = "Out of memory copying the model's pad."; return FALSE;
        }
        memcpy(data, setup->data, setup->size);
        memcpy(data + newtable, setup->data + table, (size_t)count * stride);
        memcpy(data + newtable + newindex * stride, setup->data + record, stride);
        if (bound)
        {
            SetupBoundPad *list = pads;
            memcpy(list, setup->boundpads, (size_t)count * sizeof(*list));
            list[newindex] = setup->boundpads[index];
            free(setup->boundpads); setup->boundpads = list; setup->boundpadcount = newcount;
        }
        else
        {
            SetupPad *list = pads;
            memcpy(list, setup->pads, (size_t)count * sizeof(*list));
            list[newindex] = setup->pads[index];
            free(setup->pads); setup->pads = list; setup->padcount = newcount;
        }
        free(setup->data); setup->data = data; setup->size = newsize;
        SetupWrite32(data + header, newtable);
        if (character != NULL) { character->pad = (unsigned short)encoded; }
        else { object->pad = (short)encoded; }
        data[sourceoffset + 6] = (unsigned char)(encoded >> 8);
        data[sourceoffset + 7] = (unsigned char)encoded;
        record = newtable + newindex * stride;
        end = newtable + newcount * stride;
        SetupWrite32(data + record + SETUP_PAD_LINK, end + SETUP_PAD_LINK);
        SetupWrite32(data + record + 40, SETUP_PRIVATE_PAD_STAN);
        index = newindex;
    }
    pad = bound ? &setup->boundpads[index].pad : &setup->pads[index];
    SetupWrite32(setup->data + record + SETUP_PAD_LINK, end + SETUP_PAD_LINK);
    pad->deleted = FALSE;
    pad->stanname[0] = '\0';
    for (axis = 0; axis < 3; axis++)
    {
        union { float f; DWORD u; } value;
        value.f = position[axis]; pad->pos[axis] = value.f;
        SetupWrite32(setup->data + record + axis * 4, value.u);
    }
    /* Explicit world-space placement must survive the game's grounding
       step. Sideways/upside-down props already use their authored height. */
    if (object != NULL && !door)
    {
        object->flags |= PROPFLAG_ABSOLUTEPOSITION;

        if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_UPSIDEDOWN))) 
        { 
            object->flags |= PROPFLAG_INAIR;
        }

        SetupWrite32(setup->data + object->sourceoffset + 8, object->flags);
    }

    setup->dirty = TRUE;
    *reasonout = "";
    return TRUE;
}

void SetupFileFree(SetupFile *setup)
{
    if (setup->globalrefs && !--setup->globalrefs->owners) { free(setup->globalrefs); }
    free(setup->actionmeta);
    free(setup->characters);
    free(setup->objects);
    free(setup->boundpads);
    free(setup->pads);
    free(setup->data);
    ZeroMemory(setup, sizeof(*setup));
}

BOOL SetupFileGetModelPad(const SetupFile *setup, DWORD selection, SetupPadRef *ref)
{
    DWORD index = selection & ~SETUP_CHARACTER_SELECTION_BIT;
    if (!setup || !ref)
    {
        return FALSE;
    }
    if (selection & SETUP_CHARACTER_SELECTION_BIT)
    {
        if (index >= setup->charactercount || setup->characters[index].deleted)
        {
            return FALSE;
        }
        ref->bound = FALSE;
        ref->index = setup->characters[index].pad;
    }
    else
    {
        const SetupObject *object;
        if (index >= setup->objectcount)
        {
            return FALSE;
        }
        object = &setup->objects[index];
        if (object->deleted || object->pad < 0)
        {
            return FALSE;
        }
        ref->bound = object->type == PROPDEF_DOOR || object->pad >= 10000;
        ref->index = object->pad - (ref->bound && object->type != PROPDEF_DOOR ? 10000 : 0);
    }
    return ref->index < (ref->bound ? setup->boundpadcount : setup->padcount);
}
BOOL SetupFilePadRotation(const SetupFile *setup, const SetupPadRef *ref, Rotation *out)
{
    const SetupPad *pad;
    double up[3], look[3];
    int axis;
    if (!setup || !ref || ref->index >= (ref->bound ? setup->boundpadcount : setup->padcount))
    {
        return FALSE;
    }
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    for (axis = 0; axis < 3; axis++)
    {
        up[axis] = pad->up[axis];
        look[axis] = pad->look[axis];
    }
    return RotationBasis(out, up, look);
}
BOOL SetupFileRotatePad(SetupFile *setup, const SetupPadRef *ref, const Rotation *rotation,
                        BOOL *changed, const char **reasonout)
{
    SetupPad *pad;
    DWORD table, stride, record;
    double up[3], look[3];
    int axis;
    *changed = FALSE;
    *reasonout = "Invalid pad rotation.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || !ref ||
        !RotationValid(rotation) ||
        ref->index >= (ref->bound ? setup->boundpadcount : setup->padcount))
    {
        return FALSE;
    }
    stride = ref->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    table = SetupRead32(setup->data + (ref->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    if (table < SETUP_HEADER_SIZE || table > setup->size ||
        ref->index >= (setup->size - table) / stride)
    {
        return FALSE;
    }
    record = table + ref->index * stride;
    pad = ref->bound ? &setup->boundpads[ref->index].pad : &setup->pads[ref->index];
    for (axis = 0; axis < 3; axis++)
    {
        up[axis] = pad->up[axis];
        look[axis] = pad->look[axis];
    }
    RotationVector(rotation, up, up);
    RotationVector(rotation, look, look);
    for (axis = 0; axis < 3; axis++)
    {
        if (!isfinite((float)up[axis]) || !isfinite((float)look[axis]))
        {
            return FALSE;
        }
    }
    for (axis = 0; axis < 3; axis++)
    {
        union
        {
            float f;
            DWORD u;
        } value;
        value.f = SetupPadDirectionValue((float)up[axis]);
        *changed |= value.f != pad->up[axis];
        pad->up[axis] = value.f;
        SetupWrite32(setup->data + record + 12 + axis * 4, value.u);
        value.f = SetupPadDirectionValue((float)look[axis]);
        *changed |= value.f != pad->look[axis];
        pad->look[axis] = value.f;
        SetupWrite32(setup->data + record + 24 + axis * 4, value.u);
    }
    setup->dirty |= *changed;
    *reasonout = "";
    return TRUE;
}

static BOOL SetupMarkerRecord(const SetupFile *setup, const SetupMarkerRef *ref, DWORD *offset)
{
    DWORD at, command, type, bytes;
    BOOL outro = ref && ref->kind == SETUP_MARKER_OUTRO;
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || !ref
        || ref->kind >= SETUP_MARKER_KIND_COUNT || ref->command >= SETUP_OBJECT_MAX) { return FALSE; }
    at = SetupRead32(setup->data + (outro ? 12 : 8));
    if (at < SETUP_HEADER_SIZE || (at & 3)) { return FALSE; }
    for (command = 0; command <= ref->command; command++)
    {
        if (at > setup->size || setup->size - at < 4) { return FALSE; }
        type = outro ? setup->data[at + 3] : SetupRead32(setup->data + at);
        if (type == (outro ? SETUP_PROP_END : 9)) { return FALSE; }
        bytes = (outro ? SetupObjectWordCount((unsigned char)type) : SetupIntroWordCount(type)) * 4;
        if (!bytes || bytes > setup->size - at) { return FALSE; }
        if (command == ref->command)
        {
            DWORD expected = ref->kind == SETUP_MARKER_SPAWN ? 0 : ref->kind == SETUP_MARKER_SWIRL ? 3
                : outro ? PROPDEF_CAMERAPOS : 6;
            if (type != expected || (outro && SetupRead32(setup->data + at) == SETUP_DELETED_CHARACTER_HEADER)
                || (type == 3 && (SetupRead32(setup->data + at + 4) & 1))
                || (type == 0 && SetupRead32(setup->data + at + 8) != 0)) { return FALSE; }
            *offset = at;
            return TRUE;
        }
        at += bytes;
    }
    return FALSE;
}

static BOOL SetupFixedValue(double value, double scale, DWORD *encoded)
{
    double rounded = round(value * scale);
    if (!isfinite(rounded) || rounded < -2147483648.0 || rounded > 2147483647.0) { return FALSE; }
    *encoded = (DWORD)(LONG)rounded;
    return TRUE;
}

static void SetupSwirlWorldPoint(const unsigned char *record, const SetupMarker *spawn, double world[3])
{
    double local[3];
    int axis;
    for (axis = 0; axis < 3; axis++) { local[axis] = (LONG)SetupRead32(record + 8 + axis * 4) / 65536.0; }
    world[0] = local[0]; world[1] = local[1] + 175; world[2] = local[2];
    if (SetupRead32(record + 4) & 2)
    {
        world[0] = local[2] * spawn->look[0] + local[0] * spawn->look[2];
        world[2] = local[2] * spawn->look[2] - local[0] * spawn->look[0];
    }
    for (axis = 0; axis < 3; axis++) { world[axis] += spawn->position[axis]; }
}

/* A moved camera needs a pad at its new location for the game's room/stan
 * lookup. Detach shared pads and recycle unused private placement slots. */
static BOOL SetupMoveCameraPad(SetupFile *setup, DWORD command, float levelscale,
                              const DWORD coordinates[3], const char **why)
{
    DWORD table = SetupRead32(setup->data + SETUP_PAD_POINTER), count = setup->padcount;
    DWORD old = SetupRead32(setup->data + command + 24), end, record, i;
    float pos[3];
    BOOL reuse;
    if (table < SETUP_HEADER_SIZE || table > setup->size || count > SETUP_PAD_MAX
        || count + 1 > (setup->size - table) / SETUP_PAD_SIZE || (count && !setup->pads)) { return FALSE; }
    for (i = 0; i < 3; i++)
    {
        double value = ((LONG)coordinates[i] / 100.0) * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000.0) { return FALSE; }
        pos[i] = (float)value;
    }
    end = table + count * SETUP_PAD_SIZE;
    record = old < count ? table + old * SETUP_PAD_SIZE : 0;
    {
        SetupPadRef ref = {old, FALSE};
        reuse = old < count && SetupPadUnusedExcept(setup, &ref, NULL, command + 24, why);
    }
    if (!reuse)
    {
        DWORD newindex = SetupFindFreePad(setup, FALSE, old);
        DWORD newcount = newindex < count ? count : newindex + 1;
        DWORD newtable = (setup->size + 3u) & ~3u, newsize = newtable + (newcount + 1) * SETUP_PAD_SIZE;
        unsigned char *data;
        SetupPad *pads;
        if (newcount > SETUP_PAD_MAX || newsize > SETUP_FILE_MAX) { *why = "The setup has no room for another camera pad."; return FALSE; }
        data = calloc(newsize, 1); pads = calloc(newcount, sizeof(*pads));
        if (!data || !pads) { free(data); free(pads); *why = "Out of memory moving the camera's room pad."; return FALSE; }
        memcpy(data, setup->data, setup->size);
        memcpy(data + newtable, setup->data + table, count * SETUP_PAD_SIZE);
        if (count) { memcpy(pads, setup->pads, count * sizeof(*pads)); }
        free(setup->data); free(setup->pads);
        setup->data = data; setup->size = newsize; setup->pads = pads; setup->padcount = newcount;
        SetupWrite32(data + SETUP_PAD_POINTER, newtable);
        old = newindex; record = newtable + newindex * SETUP_PAD_SIZE;
        end = newtable + newcount * SETUP_PAD_SIZE;
        memset(data + record, 0, SETUP_PAD_SIZE);
        memset(&pads[newindex], 0, sizeof(*pads));
        SetupWrite32(data + record + 40, SETUP_PRIVATE_PAD_STAN);
        SetupWrite32(data + command + 24, old);
    }
    setup->pads[old].up[1] = 1; setup->pads[old].look[2] = 1; setup->pads[old].stanname[0] = 0;
    SetupWrite32(setup->data + record + 16, 0x3f800000u);
    SetupWrite32(setup->data + record + 32, 0x3f800000u);
    SetupWrite32(setup->data + record + SETUP_PAD_LINK, end + SETUP_PAD_LINK);
    for (i = 0; i < 3; i++)
    {
        union { float f; DWORD u; } value;
        value.f = pos[i]; setup->pads[old].pos[i] = value.f;
        SetupWrite32(setup->data + record + i * 4, value.u);
    }
    return TRUE;
}

typedef struct SetupCameraList {
    DWORD start, end, commands, count, first, firstcommand;
} SetupCameraList;

static BOOL SetupScanCameras(const SetupFile *setup, BOOL outro, SetupCameraList *list)
{
    DWORD at;
    ZeroMemory(list, sizeof(*list));
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || setup->size > SETUP_FILE_MAX) { return FALSE; }
    list->start = at = SetupRead32(setup->data + (outro ? 12 : 8));
    if (!at) { return TRUE; }
    if (at < SETUP_HEADER_SIZE || (at & 3)) { return FALSE; }
    for (; list->commands < SETUP_OBJECT_MAX; list->commands++)
    {
        DWORD type, bytes;
        if (at > setup->size || setup->size - at < 4) { return FALSE; }
        type = outro ? setup->data[at + 3] : SetupRead32(setup->data + at);
        if (type == (outro ? SETUP_PROP_END : 9)) { list->end = at; return TRUE; }
        bytes = (outro ? SetupObjectWordCount((unsigned char)type) : SetupIntroWordCount(type)) * 4;
        if (!bytes || bytes > setup->size - at) { return FALSE; }
        if (type == (outro ? PROPDEF_CAMERAPOS : 6)
            && (!outro || SetupRead32(setup->data + at) != SETUP_DELETED_CHARACTER_HEADER))
        {
            if (!list->count) { list->first = at; list->firstcommand = list->commands; }
            list->count++;
        }
        at += bytes;
    }
    return FALSE;
}

/* All matching copies are native CameraPos commands, still addressed by
 * their original script tags. No editor-only record format is needed. */
static void SetupSyncOutroCameras(SetupFile *setup, const SetupCameraList *list,
                                  DWORD source, const unsigned char *previous)
{
    DWORD at;
    for (at = list->start; at < list->end; at += SetupObjectWordCount(setup->data[at + 3]) * 4)
    {
        if (setup->data[at + 3] == PROPDEF_CAMERAPOS && SetupRead32(setup->data + at) != SETUP_DELETED_CHARACTER_HEADER
            && at != source && (!previous || !memcmp(setup->data + at + 4, previous, 24)))
        { memcpy(setup->data + at + 4, setup->data + source + 4, 24); }
    }
}

BOOL SetupFilePlaceCamera(SetupFile *setup, SetupMarkerKind kind, float levelscale,
                         const double position[3], const double look[3],
                         SetupMarkerRef *out, const char **reasonout)
{
    SetupFile copy = {0}; SetupCameraList list;
    DWORD record, command, coordinates[3], angles[2], captions[2];
    BOOL outro = kind == SETUP_MARKER_OUTRO;
    double length;
    *reasonout = "The camera placement or setup is invalid.";
    if ((kind != SETUP_MARKER_INTRO && !outro) || !out || !position || !look
        || !isfinite(levelscale) || levelscale <= 0 || !SetupScanCameras(setup, outro, &list)) { return FALSE; }
    if (strncmp(setup->name, "Ump_", 4) == 0)
    { *reasonout = "Intro and outro cameras can only be placed in single-player levels."; return FALSE; }
    length = hypot(hypot(look[0], look[1]), look[2]);
    if (!isfinite(length) || length < 1e-8) { return FALSE; }
    for (int axis = 0; axis < 3; axis++)
    { if (!SetupFixedValue(position[axis], 100, &coordinates[axis])) { return FALSE; } }
    if (!SetupFixedValue(atan2(look[0], -look[2]), 65536, &angles[0])
        || !SetupFixedValue(atan2(look[1], hypot(look[0], look[2])), 65536, &angles[1])) { return FALSE; }
    /* New intro candidates inherit the level's title text. If no camera
     * exists, use the blank TITLE_STR_227 in permanently loaded LTITLE (39).
     * Language ID 0 would dereference an unloaded bank in langGet. */
    captions[0] = (!outro && list.count) ? SetupRead32(setup->data + list.first + 28) : 0;
    captions[1] = (!outro && list.count) ? SetupRead32(setup->data + list.first + 32) : 0;
    if (!captions[0]) { captions[0] = (39u << 10) | 227u; }
    if (!SetupFileClone(setup, &copy, reasonout)) { return FALSE; }
    if (outro && list.count)
    { record = list.first; command = list.firstcommand; }
    else
    {
        DWORD start = (copy.size + 3u) & ~3u, bytes = list.end - list.start;
        DWORD size = start + bytes + (outro ? 28 : 40) + 4;
        unsigned char *data;
        if (size > SETUP_FILE_MAX || list.commands >= SETUP_OBJECT_MAX - 1)
        { *reasonout = "The setup has no room for another camera."; goto fail; }
        data = realloc(copy.data, size);
        if (!data) { *reasonout = "Out of memory placing the camera."; goto fail; }
        copy.data = data;
        memset(data + copy.size, 0, size - copy.size);
        memcpy(data + start, data + list.start, bytes);
        record = start + bytes; command = list.commands;
        SetupWrite32(data + (outro ? 12 : 8), start);
        SetupWrite32(data + record, outro ? PROPDEF_CAMERAPOS : 6);
        SetupWrite32(data + size - 4, outro ? SETUP_PROP_END : 9);
        copy.size = size;
        if (!outro)
        {
            SetupWrite32(data + record + 28, captions[0]);
            SetupWrite32(data + record + 32, captions[1]);
            /* The game builds prev links and counts every type-6 record. */
            SetupWrite32(data + record + 36, 0);
        }
    }
    SetupWrite32(copy.data + record + 24, 0xffffffffu); /* new private room pad */
    *reasonout = "The setup's room pad table or camera position is invalid.";
    if (!SetupMoveCameraPad(&copy, record, levelscale, coordinates, reasonout)) { goto fail; }
    for (int axis = 0; axis < 3; axis++) { SetupWrite32(copy.data + record + 4 + axis * 4, coordinates[axis]); }
    for (int axis = 0; axis < 2; axis++) { SetupWrite32(copy.data + record + 16 + axis * 4, angles[axis]); }
    if (outro)
    {
        if (!SetupScanCameras(&copy, TRUE, &list)) { goto fail; }
        SetupSyncOutroCameras(&copy, &list, record, NULL);
        /* Appending the prop list moves source offsets, but keeps indices. */
        free(copy.objects); copy.objects = NULL; copy.objectcount = 0;
        free(copy.characters); copy.characters = NULL; copy.charactercount = 0;
        if (!SetupParseObjects(&copy, reasonout)) { goto fail; }
    }
    copy.dirty = TRUE;
    SetupFileFree(setup); *setup = copy;
    out->kind = kind; out->command = command;
    *reasonout = ""; return TRUE;
fail:
    SetupFileFree(&copy);
    return FALSE;
}

BOOL SetupFileDeleteIntroCamera(SetupFile *setup, const SetupMarkerRef *ref, const char **reasonout)
{
    SetupFile copy = {0}; SetupCameraList list;
    DWORD record, start, bytes, size;
    unsigned char *data;
    *reasonout = "The selected intro camera is invalid.";
    if (!ref || ref->kind != SETUP_MARKER_INTRO || !SetupScanCameras(setup, FALSE, &list)
        || !SetupMarkerRecord(setup, ref, &record)) { return FALSE; }
    if (strncmp(setup->name, "Ump_", 4) == 0)
    { *reasonout = "Intro cameras are only editable in single-player levels."; return FALSE; }
    if (list.count <= 1)
    { *reasonout = "The level must keep at least one intro camera."; return FALSE; }
    if (!SetupFileClone(setup, &copy, reasonout)) { return FALSE; }
    start = (copy.size + 3u) & ~3u; bytes = list.end - list.start - 40; size = start + bytes + 4;
    if (size > SETUP_FILE_MAX) { *reasonout = "The setup size limit has been reached."; goto fail; }
    data = realloc(copy.data, size);
    if (!data) { *reasonout = "Out of memory deleting the intro camera."; goto fail; }
    copy.data = data;
    memset(data + copy.size, 0, start - copy.size);
    memcpy(data + start, data + list.start, record - list.start);
    memcpy(data + start + record - list.start, data + record + 40, list.end - record - 40);
    SetupWrite32(data + start + bytes, 9); SetupWrite32(data + 8, start);
    copy.size = size; copy.dirty = TRUE;
    SetupFileFree(setup); *setup = copy;
    *reasonout = ""; return TRUE;
fail:
    SetupFileFree(&copy); return FALSE;
}

BOOL SetupFileTransformMarker(SetupFile *setup, const SetupMarkerRef *ref,
                             const SetupMarker *spawn, float levelscale,
                             const double offset[3], const Rotation *rotation,
                             BOOL *changed, const char **reasonout)
{
    DWORD record, values[3];
    SetupCameraList outrolist;
    unsigned char previous[24];
    int axis;
    *changed = FALSE;
    *reasonout = "Invalid setup marker transform or coordinate range.";
    if (!SetupMarkerRecord(setup, ref, &record) || !isfinite(levelscale) || levelscale <= 0
        || (!!offset == !!rotation) || (rotation && !RotationValid(rotation))) { return FALSE; }
    if (ref->kind == SETUP_MARKER_OUTRO)
    {
        if (!SetupScanCameras(setup, TRUE, &outrolist)) { return FALSE; }
        memcpy(previous, setup->data + record + 4, sizeof(previous));
    }
    if (offset) { for (axis = 0; axis < 3; axis++) { if (!isfinite(offset[axis])) { return FALSE; } } }
    if (offset && offset[0] == 0 && offset[1] == 0 && offset[2] == 0) { *reasonout = ""; return TRUE; }
    if (ref->kind == SETUP_MARKER_SPAWN)
    {
        SetupPadRef pad = {SetupRead32(setup->data + record + 4), FALSE};
        /* The game uses only the horizontal facing of a spawn pad. */
        if (rotation && (fabs(rotation->m[1][1] - 1) > 1e-6)) { return FALSE; }
        return offset ? SetupFileTranslatePad(setup, &pad, levelscale, offset, changed, reasonout)
            : SetupFileRotatePad(setup, &pad, rotation, changed, reasonout);
    }
    if (ref->kind == SETUP_MARKER_SWIRL)
    {
        SetupSwirlPath path = {0};
        DWORD index, first, last, i, *encoded = NULL;
        if (!spawn || spawn->kind != SETUP_MARKER_SPAWN
            || !SetupFileBuildSwirlPath(setup, spawn, &path, reasonout)) { return FALSE; }
        for (index = 0; index < path.pointcount && path.points[index].command != ref->command; index++) {}
        if (index == path.pointcount) { SetupSwirlPathFree(&path); return FALSE; }
        first = last = index;
        if (rotation)
        {
            /* Rotate neighbouring controls around this point. Walk past
             * duplicated endpoint controls so their shared tangent can turn. */
            while (first > 0)
            {
                first--;
                if (memcmp(path.points[first].position, path.points[index].position, sizeof(path.points[first].position))) { break; }
            }
            while (last + 1 < path.pointcount)
            {
                last++;
                if (memcmp(path.points[last].position, path.points[index].position, sizeof(path.points[last].position))) { break; }
            }
        }
        encoded = malloc((size_t)(last - first + 1) * 3 * sizeof(*encoded));
        if (!encoded) { SetupSwirlPathFree(&path); *reasonout = "Out of memory transforming swirl controls."; return FALSE; }
        for (i = first; i <= last; i++)
        {
            double world[3], pivot[3], local[3];
            DWORD at = record - index * 32 + i * 32;
            SetupSwirlWorldPoint(setup->data + at, spawn, world);
            SetupSwirlWorldPoint(setup->data + record, spawn, pivot);
            for (axis = 0; axis < 3; axis++)
            {
                if (offset) { world[axis] += offset[axis]; }
            }
            if (rotation) { RotationPoint(rotation, pivot, world, world); }
            for (axis = 0; axis < 3; axis++) { local[axis] = world[axis] - spawn->position[axis]; }
            local[1] -= 175;
            if (SetupRead32(setup->data + at + 4) & 2)
            {
                double x = local[0], z = local[2];
                double norm = (double)spawn->look[0] * spawn->look[0] + (double)spawn->look[2] * spawn->look[2];
                local[0] = (x * spawn->look[2] - z * spawn->look[0]) / norm;
                local[2] = (x * spawn->look[0] + z * spawn->look[2]) / norm;
            }
            for (axis = 0; axis < 3; axis++)
            {
                if (!SetupFixedValue(local[axis], 65536, &encoded[(i - first) * 3 + axis]))
                { free(encoded); SetupSwirlPathFree(&path); *reasonout = "The swirl move exceeds signed 16.16 coordinates."; return FALSE; }
            }
        }
        for (i = first; i <= last; i++)
        {
            DWORD at = record - index * 32 + i * 32;
            for (axis = 0; axis < 3; axis++)
            {
                DWORD value = encoded[(i - first) * 3 + axis];
                *changed |= value != SetupRead32(setup->data + at + 8 + axis * 4);
                SetupWrite32(setup->data + at + 8 + axis * 4, value);
            }
        }
        free(encoded); SetupSwirlPathFree(&path);
    }
    else if (offset)
    {
        for (axis = 0; axis < 3; axis++)
        {
            if (!SetupFixedValue((LONG)SetupRead32(setup->data + record + 4 + axis * 4) / 100.0 + offset[axis], 100, &values[axis])) { return FALSE; }
            *changed |= values[axis] != SetupRead32(setup->data + record + 4 + axis * 4);
        }
        if (*changed)
        {
            if (!SetupMoveCameraPad(setup, record, levelscale, values, reasonout)) { *changed = FALSE; return FALSE; }
            for (axis = 0; axis < 3; axis++) { SetupWrite32(setup->data + record + 4 + axis * 4, values[axis]); }
        }
    }
    else
    {
        SetupMarker marker;
        double look[3];
        SetupCameraMarker(setup->data + record, ref->kind, &marker);
        for (axis = 0; axis < 3; axis++) { look[axis] = marker.look[axis]; }
        RotationVector(rotation, look, look);
        if (!SetupFixedValue(atan2(look[0], -look[2]), 65536, &values[0])
            || !SetupFixedValue(atan2(look[1], hypot(look[0], look[2])), 65536, &values[1])) { return FALSE; }
        for (axis = 0; axis < 2; axis++)
        {
            *changed |= values[axis] != SetupRead32(setup->data + record + 16 + axis * 4);
            SetupWrite32(setup->data + record + 16 + axis * 4, values[axis]);
        }
    }
    if (*changed && ref->kind == SETUP_MARKER_OUTRO)
    { SetupSyncOutroCameras(setup, &outrolist, record, previous); }
    setup->dirty |= *changed;
    *reasonout = "";
    return TRUE;
}

/* Keep ordinary-pad indices alive: AI, paths and guards can reference them.
   A promoted pad is appended to the bound table; prop references can then
   point at its encoded bound index without renumbering any setup commands. */
static BOOL SetupAppendBoundPad(SetupFile *setup, const SetupPadRef *source, SetupPadRef *out,
                                const char **reasonout)
{
    DWORD count = setup->boundpadcount;
    DWORD index = SetupFindFreePad(setup, TRUE, source->bound ? source->index : (DWORD)-1);
    DWORD newcount = index < count ? count : index + 1;
    DWORD oldtable = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER);
    DWORD table = (setup->size + 3) & ~3u;
    DWORD size = table + (newcount + 1) * SETUP_BOUNDPAD_SIZE;
    DWORD sourcetable =
        SetupRead32(setup->data + (source->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    DWORD stride = source->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    unsigned char *data;
    SetupBoundPad *pads;
    if (index > 22767 || size > SETUP_FILE_MAX || oldtable > setup->size ||
        count > (setup->size - oldtable) / SETUP_BOUNDPAD_SIZE ||
        source->index >= (source->bound ? count : setup->padcount) || sourcetable > setup->size ||
        source->index + 1 > (setup->size - sourcetable) / stride)
    {
        *reasonout = "The setup has no room for another bound pad.";
        return FALSE;
    }
    data = calloc(size, 1);
    pads = calloc(newcount, sizeof(*pads));
    if (!data || !pads)
    {
        free(data);
        free(pads);
        *reasonout = "Out of memory creating a bound pad.";
        return FALSE;
    }
    memcpy(data, setup->data, setup->size);
    if (count)
    {
        memcpy(data + table, setup->data + oldtable, count * SETUP_BOUNDPAD_SIZE);
        memcpy(pads, setup->boundpads, count * sizeof(*pads));
    }
    memset(data + table + index * SETUP_BOUNDPAD_SIZE, 0, SETUP_BOUNDPAD_SIZE);
    memcpy(data + table + index * SETUP_BOUNDPAD_SIZE,
           setup->data + sourcetable + source->index * stride, stride);
    if (source->bound)
    {
        pads[index] = setup->boundpads[source->index];
    }
    else
    {
        pads[index].pad = setup->pads[source->index];
    }
    /* Empty stan link, with a non-null pointer so this remains a live record. */
    SetupWrite32(data + table + index * SETUP_BOUNDPAD_SIZE + SETUP_PAD_LINK,
                 table + newcount * SETUP_BOUNDPAD_SIZE + SETUP_PAD_LINK);
    SetupWrite32(data + table + index * SETUP_BOUNDPAD_SIZE + 40, SETUP_PRIVATE_PAD_STAN);
    pads[index].pad.deleted = FALSE;
    pads[index].pad.stanname[0] = '\0';
    SetupWrite32(data + SETUP_BOUNDPAD_POINTER, table);
    free(setup->data);
    free(setup->boundpads);
    setup->data = data;
    setup->size = size;
    setup->boundpads = pads;
    setup->boundpadcount = newcount;
    out->index = index;
    out->bound = TRUE;
    setup->dirty = TRUE;
    return TRUE;
}

static BOOL SetupWriteBounds(SetupFile *setup, const SetupPadRef *ref, const double bounds[6],
                             const char **reasonout)
{
    DWORD table = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER), record;
    SetupBoundPad *pad;
    float values[6];
    int i;
    for (i = 0; i < 6; i++)
    {
        if (!isfinite(bounds[i]) || fabs(bounds[i]) > 100000000)
        {
            *reasonout = "Scaling exceeds the setup coordinate range.";
            return FALSE;
        }
        values[i] = (float)bounds[i];
    }
    if (!ref->bound || ref->index >= setup->boundpadcount || table > setup->size ||
        ref->index + 1 > (setup->size - table) / SETUP_BOUNDPAD_SIZE)
    {
        *reasonout = "Invalid bound pad.";
        return FALSE;
    }
    record = table + ref->index * SETUP_BOUNDPAD_SIZE;
    pad = &setup->boundpads[ref->index];
    pad->xmin = values[0];
    pad->xmax = values[1];
    pad->ymin = values[2];
    pad->ymax = values[3];
    pad->zmin = values[4];
    pad->zmax = values[5];
    for (i = 0; i < 6; i++)
    {
        union
        {
            float f;
            DWORD u;
        } value;
        value.f = values[i];
        SetupWrite32(setup->data + record + SETUP_BOUNDPAD_BBOX + i * 4, value.u);
    }
    setup->dirty = TRUE;
    return TRUE;
}

BOOL SetupFileTranslateModelReferenced(SetupFile *setup, DWORD selection,
    float levelscale, const double offset[3], const float reference[3],
    const char *stanname, const char **reasonout)
{
    SetupPadRef ref;
    SetupPad previous, *pad;
    SetupObject *object;
    double bounds[6] = {0}, axes[3][3], delta[3], determinant = 0;
    const double zero[3] = {0};
    float position[3];
    DWORD record;
    BOOL wasbound;
    int axis, component;
    *reasonout = "Invalid prop placement reference.";
    if (!setup || !setup->data || !offset || !reference || !stanname || !stanname[0]
        || !isfinite(levelscale) || levelscale <= 0
        || (selection & SETUP_CHARACTER_SELECTION_BIT)
        || !SetupFileGetModelPad(setup, selection, &ref)
        || setup->objects[selection].type == PROPDEF_DOOR) { return FALSE; }
    wasbound = ref.bound;
    previous = ref.bound ? setup->boundpads[ref.index].pad : setup->pads[ref.index];
    if (wasbound)
    {
        const SetupBoundPad *bound = &setup->boundpads[ref.index];
        bounds[0] = bound->xmin; bounds[1] = bound->xmax;
        bounds[2] = bound->ymin; bounds[3] = bound->ymax;
        bounds[4] = bound->zmin; bounds[5] = bound->zmax;
    }
    for (axis = 0; axis < 3; axis++)
    {
        double value = (double)reference[axis] * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000 || !isfinite(offset[axis]))
        { *reasonout = "The move exceeds the setup coordinate range."; return FALSE; }
        /* A retained reference must not drift through repeated world/native
         * conversions, especially when its pad is on a tile boundary. */
        position[axis] = reference[axis] == previous.pos[axis] / levelscale
            ? previous.pos[axis] : (float)value;
        delta[axis] = previous.pos[axis] + offset[axis] * levelscale - position[axis];
        axes[1][axis] = previous.up[axis];
        axes[2][axis] = previous.look[axis];
        axes[0][axis] = (double)previous.up[(axis+1)%3] * previous.look[(axis+2)%3]
                     - (double)previous.up[(axis+2)%3] * previous.look[(axis+1)%3];
        determinant += axes[0][axis] * axes[0][axis];
    }
    if (!isfinite(determinant) || determinant < 1e-12) { return FALSE; }
    determinant = sqrt(determinant);
    for (axis = 0; axis < 3; axis++) { axes[0][axis] /= determinant; }
    /* Invert padGetCenter's basis: normalized cross, authored up/look.
     * Imported pads need not have perfectly normalized orientation vectors. */
    for (axis = 0; axis < 3; axis++)
    {
        double shift = 0;
        for (component = 0; component < 3; component++)
        {
            int a = (axis+1)%3, b = (axis+2)%3;
            int c = (component+1)%3, d = (component+2)%3;
            shift += delta[component] * (axes[a][c]*axes[b][d] - axes[a][d]*axes[b][c]);
        }
        shift /= determinant;
        bounds[axis*2] += shift; bounds[axis*2+1] += shift;
        if (!isfinite(bounds[axis*2]) || !isfinite(bounds[axis*2+1])
            || fabs(bounds[axis*2]) > 100000000 || fabs(bounds[axis*2+1]) > 100000000)
        { *reasonout = "The move exceeds the setup coordinate range."; return FALSE; }
    }
    /* Detach shared pads before changing their bounds or reference position. */
    if (wasbound)
    {
        if (!SetupFileTranslateModel(setup, selection, levelscale, zero, reasonout)
            || !SetupFileGetModelPad(setup, selection, &ref)) { return FALSE; }
    }
    else if (!SetupAppendBoundPad(setup, &ref, &ref, reasonout)) { return FALSE; }
    object = &setup->objects[selection];
    object->pad = (short)(ref.index + 10000);
    setup->data[object->sourceoffset+6] = (unsigned char)(object->pad >> 8);
    setup->data[object->sourceoffset+7] = (unsigned char)object->pad;
    /* Fitting flags on an ordinary pad were inert. Do not let promotion turn
     * its zero-size placement bounds into a new scale for the model. */
    if (!wasbound)
    { object->flags &= ~(PROPFLAG_SCALE_TO_PAD_BOUNDS | PROPFLAG_SCALE_TO_X_BOUNDS
                      | PROPFLAG_SCALE_TO_Y_BOUNDS | PROPFLAG_SCALE_TO_Z_BOUNDS); }
    object->flags |= PROPFLAG_ABSOLUTEPOSITION;
    if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_UPSIDEDOWN))) { object->flags |= PROPFLAG_INAIR; }
    object->flags2 |= PROPFLAG2_USE_PAD_REFERENCE;
    SetupWrite32(setup->data + object->sourceoffset + 8, object->flags);
    SetupWrite32(setup->data + object->sourceoffset + 12, object->flags2);
    record = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER) + ref.index * SETUP_BOUNDPAD_SIZE;
    pad = &setup->boundpads[ref.index].pad;
    for (axis = 0; axis < 3; axis++)
    {
        union { float f; DWORD u; } value;
        value.f = position[axis]; pad->pos[axis] = value.f;
        SetupWrite32(setup->data + record + axis*4, value.u);
    }
    return SetupWriteBounds(setup, &ref, bounds, reasonout)
        && SetupFileSetPadStanName(setup, &ref, stanname, reasonout);
}

BOOL SetupFileScalePad(SetupFile *setup, SetupPadRef *ref, float levelscale, const Scaling *scale,
                       const char **reasonout)
{
    double bounds[6] = {-SETUP_PAD_HALF_SIZE, SETUP_PAD_HALF_SIZE,  -SETUP_PAD_HALF_SIZE,
                        SETUP_PAD_HALF_SIZE,  -SETUP_PAD_HALF_SIZE, SETUP_PAD_HALF_SIZE};
    DWORD i, oldindex;
    int axis;
    *reasonout = "Invalid pad scale.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || !ref || !ScalingValid(scale) ||
        !isfinite(levelscale) || levelscale <= 0 ||
        ref->index >= (ref->bound ? setup->boundpadcount : setup->padcount))
    {
        return FALSE;
    }
    if (ref->bound)
    {
        const SetupBoundPad *pad = &setup->boundpads[ref->index];
        bounds[0] = pad->xmin;
        bounds[1] = pad->xmax;
        bounds[2] = pad->ymin;
        bounds[3] = pad->ymax;
        bounds[4] = pad->zmin;
        bounds[5] = pad->zmax;
    }
    else
    {
        /* Match the ordinary pad's world-sized preview when promoting it
           to a bound pad, whose extents are stored in native level units. */
        for (i = 0; i < 6; i++) { bounds[i] *= levelscale; }
    }
    for (axis = 0; axis < 3; axis++)
    {
        double center = (bounds[axis * 2] + bounds[axis * 2 + 1]) * .5;
        double half = (bounds[axis * 2 + 1] - bounds[axis * 2]) * .5 * scale->factor[axis];
        bounds[axis * 2] = center - half;
        bounds[axis * 2 + 1] = center + half;
    }
    if (!ref->bound)
    {
        oldindex = ref->index;
        if (!SetupAppendBoundPad(setup, ref, ref, reasonout))
        {
            return FALSE;
        }
        /* Guards and AI retain the ordinary pad. Existing props use the new
           volume with their authored placement/sizing flags. */
        for (i = 0; i < setup->objectcount; i++)
        {
            SetupObject *object = &setup->objects[i];
            if (object->type != PROPDEF_DOOR && object->pad >= 0 && object->pad < 10000 &&
                (DWORD)object->pad == oldindex)
            {
                object->pad = (short)(ref->index + 10000);
                setup->data[object->sourceoffset + 6] = (unsigned char)(object->pad >> 8);
                setup->data[object->sourceoffset + 7] = (unsigned char)object->pad;
            }
        }
    }
    return SetupWriteBounds(setup, ref, bounds, reasonout);
}

/* Called inside the frame's setup transaction. Each scaled prop owns its
   bound pad so another prop sharing the old pad is never resized with it. */
BOOL SetupFileSetModelBounds(SetupFile *setup, DWORD selection, float levelscale,
                             const double bounds[6], const char **reasonout)
{
    SetupPadRef ref;
    SetupObject *object;
    char stanname[16];
    double zero[3] = {0};
    if ((selection & SETUP_CHARACTER_SELECTION_BIT) ||
        !SetupFileGetModelPad(setup, selection, &ref))
    {
        *reasonout = "Only props can be scaled.";
        return FALSE;
    }
    lstrcpyn(stanname, ref.bound ? setup->boundpads[ref.index].pad.stanname
                                : setup->pads[ref.index].stanname, sizeof(stanname));
    if (ref.bound)
    {
        if (!SetupFileTranslateModel(setup, selection, levelscale, zero, reasonout) ||
            !SetupFileGetModelPad(setup, selection, &ref))
        {
            return FALSE;
        }
    }
    else if (!SetupAppendBoundPad(setup, &ref, &ref, reasonout))
    {
        return FALSE;
    }
    /* Detaching/promoting a pad changes ownership and bounds, not its
     * authored position. Keep the floor needed to rebuild elevated props. */
    if (stanname[0] && !SetupFileSetPadStanName(setup, &ref, stanname, reasonout)) { return FALSE; }
    object = &setup->objects[selection];
    object->pad = (short)(ref.index + (object->type == PROPDEF_DOOR ? 0 : 10000));
    setup->data[object->sourceoffset + 6] = (unsigned char)(object->pad >> 8);
    setup->data[object->sourceoffset + 7] = (unsigned char)object->pad;
    if (object->type != PROPDEF_DOOR)
    {
        object->flags &= ~PROPFLAG_SCALE_TO_PAD_BOUNDS;
        object->flags |= PROPFLAG_SCALE_TO_X_BOUNDS | PROPFLAG_SCALE_TO_Y_BOUNDS |
                         PROPFLAG_SCALE_TO_Z_BOUNDS | PROPFLAG_ABSOLUTEPOSITION;
        if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_UPSIDEDOWN)))
        {
            object->flags |= PROPFLAG_INAIR;
        }
        object->extrascale = 256;
        setup->data[object->sourceoffset] = 1;
        setup->data[object->sourceoffset + 1] = 0;
        SetupWrite32(setup->data + object->sourceoffset + 8, object->flags);
    }
    return SetupWriteBounds(setup, &ref, bounds, reasonout);
}

/* Shared authored fields. Offsets are the N64 setup ABI, never host sizeof. */
static const unsigned char *SetupObjectPropertyRecord(const SetupFile *setup, DWORD index,
                                                     const char **reasonout)
{
    const SetupObject *object;
    const unsigned char *record;
    *reasonout = "The selected setup object is no longer available.";
    if (!setup || !setup->data || !setup->objects || index >= setup->objectcount) { return NULL; }
    object = &setup->objects[index];
    if (object->deleted || object->sourceoffset < SETUP_HEADER_SIZE
        || object->sourceoffset > setup->size || setup->size - object->sourceoffset < 0x80) { return NULL; }
    record = setup->data + object->sourceoffset;
    if (!SetupTypeCreatesObject(record[3]) || record[3] != object->type
        || SetupObjectWordCount(record[3]) * 4 > setup->size - object->sourceoffset
        || SetupRead16(record + 4) != object->modelid || SetupRead16(record + 6) != object->pad)
    { *reasonout = "The setup object's source record is inconsistent."; return NULL; }
    *reasonout = "";
    return record;
}

BOOL SetupFileCanDuplicateObject(const SetupFile *setup, DWORD index)
{
    const char *why;
    SetupPadRef ref;
    return SetupObjectPropertyRecord(setup, index, &why)
        && !(setup->objects[index].flags & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR))
        && SetupFileGetModelPad(setup, index, &ref);
}

static BOOL SetupDuplicatePad(SetupFile *dest, const SetupFile *source,
    const SetupPadRef *ref, DWORD limit, DWORD excluded, SetupPadRef *out, const char **reasonout)
{
    DWORD header = ref->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER;
    DWORD stride = ref->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    DWORD count = ref->bound ? dest->boundpadcount : dest->padcount;
    DWORD index = SetupFindFreePad(dest, ref->bound, excluded);
    DWORD total = index < count ? count : index + 1;
    DWORD old = SetupRead32(dest->data + header), start = (dest->size + 3u) & ~3u;
    DWORD size = start + (total + 1) * stride;
    DWORD from = SetupRead32(source->data + header) + ref->index * stride;
    unsigned char *data;
    char stanname[16];
    if (total > limit || size > SETUP_FILE_MAX || old > dest->size
        || count > (dest->size - old) / stride || from > source->size
        || stride > source->size - from)
    { *reasonout = "There is no room for the copied placement pad."; return FALSE; }
    lstrcpyn(stanname, ref->bound ? source->boundpads[ref->index].pad.stanname
                                 : source->pads[ref->index].stanname, sizeof(stanname));
    data = calloc(size, 1);
    if (!data) { *reasonout = "Out of memory copying the placement pad."; return FALSE; }
    memcpy(data, dest->data, dest->size);
    memcpy(data + start, dest->data + old, count * stride);
    memcpy(data + start + index * stride, source->data + from, stride);
    /* A clipboard snapshot has its own native offsets. Use a valid empty
     * link while rebuilding this table, then copy the name into dest below. */
    SetupWrite32(data + start + index * stride + SETUP_PAD_LINK,
                 start + total * stride + SETUP_PAD_LINK);
    SetupWrite32(data + start + index * stride + 40, SETUP_PRIVATE_PAD_STAN);
    SetupWrite32(data + header, start);
    free(dest->data); dest->data = data; dest->size = size;
    free(dest->pads); dest->pads = NULL; dest->padcount = 0;
    free(dest->boundpads); dest->boundpads = NULL; dest->boundpadcount = 0;
    if (!SetupParsePads(dest, reasonout)) { return FALSE; }
    out->bound = ref->bound; out->index = index;
    /* The duplicate starts at the source position. Its transform will
     * resolve the destination from this floor instead of ceiling height. */
    return !stanname[0] || SetupFileSetPadStanName(dest, out, stanname, reasonout);
}

BOOL SetupFileDuplicateObject(SetupFile *setup, const SetupFile *source,
    DWORD index, DWORD *selectionout, const char **reasonout)
{
    SetupFile copy = {0};
    SetupPadRef ref, placed, aimref = {0}, aimcopy;
    DWORD start, end, commands = 0, bytes, reused, newstart, record, size, unused;
    const SetupObject *object;
    unsigned char *data;
    LONG aim = -1;
    *reasonout = "Select a placed object to duplicate.";
    if (!setup || !setup->data || setup->size < SETUP_HEADER_SIZE || setup->size > SETUP_FILE_MAX
        || !selectionout || !SetupFileCanDuplicateObject(source, index)
        || !SetupFileClone(setup, &copy, reasonout)) { return FALSE; }
    object = &source->objects[index];
    SetupFileGetModelPad(source, index, &ref);
    if (!SetupDuplicatePad(&copy, source, &ref,
        ref.bound ? (object->type == PROPDEF_DOOR ? 32768u : 22768u) : 10000u,
        (DWORD)-1, &placed, reasonout)) { goto fail; }
    if (object->type == PROPDEF_CCTV || object->type == PROPDEF_AUTOGUN)
    {
        aim = (LONG)SetupRead32(source->data + object->sourceoffset + 0x80);
        if (aim >= 0)
        {
            if ((DWORD)aim >= source->padcount || source->pads[aim].deleted)
            { *reasonout = "The object's look-at pad is unavailable."; goto fail; }
            aimref.index = (DWORD)aim;
            /* The new placement is not yet referenced by a command. */
            if (!SetupDuplicatePad(&copy, source, &aimref, 10000u,
                placed.bound ? (DWORD)-1 : placed.index, &aimcopy, reasonout)) { goto fail; }
            aim = (LONG)aimcopy.index;
        }
    }
    bytes = SetupObjectWordCount(object->type) * 4;
    start = SetupRead32(copy.data + SETUP_OBJECT_POINTER); end = start;
    reused = SetupFindFreeCommand(&copy, object->type, &unused);
    if (start && (start < SETUP_HEADER_SIZE || (start & 3) || start > copy.size - 4)) { goto malformed; }
    while (start && copy.data[end + 3] != SETUP_PROP_END)
    {
        DWORD length = SetupObjectWordCount(copy.data[end + 3]) * 4;
        if (++commands >= SETUP_OBJECT_MAX - (reused ? 0u : 1u)
            || length > copy.size - end || copy.size - end - length < 4) { goto malformed; }
        end += length;
    }
    newstart = (copy.size + 3u) & ~3u;
    record = newstart + (reused ? reused - start : end - start);
    size = newstart + end - start + (reused ? 0 : bytes) + 4;
    if (size > SETUP_FILE_MAX) { *reasonout = "The setup size limit has been reached."; goto fail; }
    data = calloc(size, 1);
    if (!data) { *reasonout = "Out of memory duplicating the object."; goto fail; }
    memcpy(data, copy.data, copy.size);
    memcpy(data + newstart, copy.data + start, end - start);
    memcpy(data + record, source->data + object->sourceoffset, bytes);
    SetupWrite32(data + SETUP_OBJECT_POINTER, newstart);
    SetupWrite32(data + size - 4, SETUP_PROP_END);
    SetupWrite32(data + record + 4, ((DWORD)(unsigned short)object->modelid << 16)
        | (placed.index + (placed.bound && object->type != PROPDEF_DOOR ? 10000u : 0u)));
    /* Native relative links would point at unrelated commands from this new
     * index. Copies are standalone, with no duplicate tags or mission links. */
    if (object->type == PROPDEF_DOOR) { SetupWrite32(data + record + 0x80, 0); }
    if (object->type == PROPDEF_MONITOR) { SetupWrite32(data + record + 244, 0); }
    if (object->type == PROPDEF_CCTV || object->type == PROPDEF_AUTOGUN)
    { SetupWrite32(data + record + 0x80, (DWORD)aim); }
    free(copy.data); copy.data = data; copy.size = size;
    free(copy.objects); copy.objects = NULL; copy.objectcount = 0;
    free(copy.characters); copy.characters = NULL; copy.charactercount = 0;
    if (!SetupParseObjects(&copy, reasonout)) { goto fail; }
    for (DWORD i = 0; i < copy.objectcount; i++)
    {
        if (copy.objects[i].sourceoffset != record) { continue; }
        copy.dirty = TRUE;
        SetupFileFree(setup); *setup = copy; *selectionout = i;
        return TRUE;
    }
malformed:
    *reasonout = "The setup command list is malformed or full.";
fail:
    SetupFileFree(&copy);
    return FALSE;
}

BOOL SetupFileGetObjectProperties(const SetupFile *setup, DWORD index,
                                  SetupObjectProperties *out, const char **reasonout)
{
    const unsigned char *record = SetupObjectPropertyRecord(setup, index, reasonout);
    if (!record || !out) { return FALSE; }
    memset(out, 0, sizeof(*out));
    out->object = setup->objects[index];
    out->health = (LONG)SetupRead32(record + 0x74) / 65536.0;
    if (out->object.type == PROPDEF_ARMOUR)
    { out->armorstrength = (LONG)SetupRead32(record + 0x80) * (100.0 / 65536.0); }
    if (out->object.type == PROPDEF_CCTV)
    {
        /* setupCctv converts these signed integer words to floats in-game.
         * CC is the upper/initial yaw, D0 the lower yaw (objTickCctv). */
        out->cctv.lookpad = (LONG)SetupRead32(record + 0x80);
        out->cctv.sweepmax = (LONG)SetupRead32(record + 0xcc) * (360.0 / 65536.0);
        out->cctv.sweepmin = (LONG)SetupRead32(record + 0xd0) * (360.0 / 65536.0);
        out->cctv.speed = (LONG)SetupRead32(record + 0xdc) * (21600.0 / 65536.0);
        LONG range = (LONG)SetupRead32(record + 0xe8);
        out->cctv.range = range > 0 ? (DWORD)range : 0;
    }
    if (out->object.type == PROPDEF_AUTOGUN)
    {
        /* setupAutogun: signed 16.16 turns, turns/tick, and metres.
         * objTickAutogun compares yaw offsets only within [-180, 180). */
        out->drone.aimpad = (LONG)SetupRead32(record + 0x80);
        out->drone.yawmax = fmax(-180, fmin(180, (LONG)SetupRead32(record + 0x88) * (360.0 / 65536.0)));
        out->drone.yawmin = fmax(-180, fmin(180, (LONG)SetupRead32(record + 0x8c) * (360.0 / 65536.0)));
        out->drone.speed = (LONG)SetupRead32(record + 0xa4) * (21600.0 / 65536.0);
        out->drone.range = (LONG)SetupRead32(record + 0xa8) * (100.0 / 65536.0);
    }
    if (out->object.type == PROPDEF_KEY) { out->keyflags = SetupRead32(record + 0x80); }
    if (out->object.type == PROPDEF_DOOR)
    {
        out->door.travel = (LONG)SetupRead32(record + 0x84) / 65536.0;
        out->door.clearance = (LONG)SetupRead32(record + 0x88) / 65536.0;
        out->door.accel = (LONG)SetupRead32(record + 0x8c) / 65536.0;
        out->door.decel = (LONG)SetupRead32(record + 0x90) / 65536.0;
        out->door.speed = (LONG)SetupRead32(record + 0x94) / 65536.0;
        out->door.flags = (unsigned short)(SetupRead32(record + 0x98) >> 16);
        out->door.type = (unsigned short)SetupRead32(record + 0x98);
        out->keyflags = SetupRead32(record + 0x9c);
        out->door.closeframes = SetupRead32(record + 0xa0);
        out->door.sound = SetupRead32(record + 0xa4);
    }
    if (out->object.type == PROPDEF_MAGAZINE) { out->ammotype = SetupRead32(record + 0x80); }
    if (out->object.type == PROPDEF_AMMO)
    {
        for (DWORD slot = 0; slot < AMMOTYPE_GLOBAL_MAX; slot++)
        {
            DWORD word = SetupRead32(record + 0x80 + slot * 4);
            out->ammo[slot].model = (unsigned short)(word >> 16);
            out->ammo[slot].quantity = (unsigned short)word;
        }
    }
    return TRUE;
}

const char *SetupObjectTypeName(unsigned char type)
{
    switch (type)
    {
    case PROPDEF_DOOR: return "Door";
    case PROPDEF_PROP: return "Prop";
    case PROPDEF_KEY: return "Key";
    case PROPDEF_ALARM: return "Alarm";
    case PROPDEF_CCTV: return "CCTV camera";
    case PROPDEF_MAGAZINE: return "Single-ammo pickup";
    case PROPDEF_COLLECTABLE: return "Weapon / item";
    case PROPDEF_MONITOR: return "Monitor";
    case PROPDEF_MULTI_MONITOR: return "Multi-monitor";
    case PROPDEF_RACK: return "Monitor rack";
    case PROPDEF_AUTOGUN: return "Drone gun";
    case PROPDEF_HAT: return "Hat";
    case PROPDEF_AMMO: return "Ammo crate";
    case PROPDEF_ARMOUR: return "Body armour";
    case PROPDEF_GAS_RELEASING: return "Gas-releasing prop";
    case PROPDEF_VEHICLE: return "Vehicle";
    case PROPDEF_AIRCRAFT: return "Aircraft";
    case PROPDEF_GLASS: return "Glass";
    case PROPDEF_SAFE: return "Safe";
    case PROPDEF_TANK: return "Tank";
    case PROPDEF_TINTED_GLASS: return "Tinted glass";
    default: return "Unknown object";
    }
}

BOOL SetupFileSetObjectProperty(SetupFile *setup, const SetupObjectPropertyEdit *edit,
                                BOOL *changedout, const char **reasonout)
{
    const unsigned char *record;
    DWORD encoded, offset, previous;
    const char *modelname;
    *changedout = FALSE;
    *reasonout = "The object property request is invalid.";
    if (!edit) { return FALSE; }
    record = SetupObjectPropertyRecord(setup, edit->objectindex, reasonout);
    if (!record) { return FALSE; }
    if (setup->objects[edit->objectindex].sourceoffset != edit->sourceoffset || record[3] != edit->type)
    { *reasonout = "The selected object changed; select it again before editing."; return FALSE; }
    if (!isfinite(edit->value))
    { *reasonout = "Enter a finite number."; return FALSE; }
    if (edit->property >= SETUP_OBJECT_DOOR_TRAVEL && edit->property <= SETUP_OBJECT_DOOR_KEY_FLAGS
        && record[3] != PROPDEF_DOOR)
    { *reasonout = "Door settings can only be edited on a door."; return FALSE; }
    if (edit->property >= SETUP_OBJECT_CCTV_LOOK_PAD && edit->property <= SETUP_OBJECT_CCTV_RANGE
        && record[3] != PROPDEF_CCTV)
    { *reasonout = "CCTV settings can only be edited on a CCTV camera."; return FALSE; }
    if (edit->property >= SETUP_OBJECT_DRONE_AIM_PAD && edit->property <= SETUP_OBJECT_DRONE_RANGE
        && record[3] != PROPDEF_AUTOGUN)
    { *reasonout = "Drone gun settings can only be edited on a drone gun."; return FALSE; }
    switch (edit->property)
    {
    case SETUP_OBJECT_ARMOR_STRENGTH:
        if (record[3] != PROPDEF_ARMOUR)
        { *reasonout = "Armor strength can only be edited on an armor pickup."; return FALSE; }
        if (edit->value < 0 || edit->value > 100)
        { *reasonout = "Armor strength must be between 0 and 100 percent."; return FALSE; }
        encoded = (DWORD)floor(edit->value * (65536.0 / 100.0) + 0.5);
        if (encoded == SetupRead32(record + 0x80)) { return TRUE; }
        /* Only author initialamount; amount at 0x84 belongs to the runtime. */
        SetupWrite32(setup->data + edit->sourceoffset + 0x80, encoded);
        break;
    case SETUP_OBJECT_DRONE_AIM_PAD:
        if (edit->value < -1 || edit->value > 2147483647.0 || floor(edit->value) != edit->value)
        { *reasonout = "Choose an existing aim pad or the default +Z direction."; return FALSE; }
        encoded = (DWORD)(LONG)edit->value;
        if (edit->value >= 0 && !SetupPadExists(setup, encoded))
        { *reasonout = "The aim pad does not exist in this setup."; return FALSE; }
        previous = SetupRead32(record + 0x80);
        if (encoded == previous || (edit->value == -1 && (LONG)previous < 0)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0x80, encoded);
        break;
    case SETUP_OBJECT_DRONE_YAW_MIN:
    case SETUP_OBJECT_DRONE_YAW_MAX:
    {
        if (edit->value < -180 || edit->value > 180)
        { *reasonout = "Enter a horizontal aim limit from -180 to 180 degrees."; return FALSE; }
        encoded = (DWORD)(LONG)round(edit->value * (65536.0 / 360.0));
        offset = edit->property == SETUP_OBJECT_DRONE_YAW_MIN ? 0x8c : 0x88;
        LONG other = (LONG)SetupRead32(record + (offset == 0x8c ? 0x88 : 0x8c));
        other = (LONG)fmax(-32768, fmin(32768, other));
        if (offset == 0x8c ? (LONG)encoded > other : (LONG)encoded < other)
        { *reasonout = "Horizontal aim minimum must not exceed maximum."; return FALSE; }
        previous = SetupRead32(record + offset);
        /* Do not rewrite a stock full-circle sentinel just by accepting its
         * displayed effective limit. Other fields always keep it verbatim. */
        if ((LONG)encoded == (LONG)fmax(-32768, fmin(32768, (LONG)previous))) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    }
    case SETUP_OBJECT_DRONE_SPEED:
    case SETUP_OBJECT_DRONE_RANGE:
    {
        double factor = edit->property == SETUP_OBJECT_DRONE_SPEED ? 21600.0 : 100.0;
        if (edit->value < 0 || edit->value > 2147483647.0 * (factor / 65536.0))
        { *reasonout = "Drone gun speed or range is outside the supported range."; return FALSE; }
        encoded = (DWORD)floor(edit->value * (65536.0 / factor) + 0.5);
        offset = edit->property == SETUP_OBJECT_DRONE_SPEED ? 0xa4 : 0xa8;
        if (encoded == SetupRead32(record + offset)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    }
    case SETUP_OBJECT_CCTV_LOOK_PAD:
        if (edit->value < 0 || edit->value > 2147483647.0 || floor(edit->value) != edit->value)
        { *reasonout = "Choose an existing look-at pad."; return FALSE; }
        encoded = (DWORD)edit->value;
        if (!SetupPadExists(setup, encoded))
        { *reasonout = "The look-at pad does not exist in this setup."; return FALSE; }
        if (encoded == SetupRead32(record + 0x80)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0x80, encoded);
        break;
    case SETUP_OBJECT_CCTV_SWEEP_MIN:
    case SETUP_OBJECT_CCTV_SWEEP_MAX:
        /* The game's yaw normalization adds/subtracts one full turn. Keep
         * authored endpoints within that range, in the order its sweep needs. */
        if (edit->value < -360 || edit->value > 360)
        { *reasonout = "Enter a sweep angle from -360 to 360 degrees."; return FALSE; }
        encoded = (DWORD)(LONG)round(edit->value * (65536.0 / 360.0));
        offset = edit->property == SETUP_OBJECT_CCTV_SWEEP_MIN ? 0xd0 : 0xcc;
        if (edit->property == SETUP_OBJECT_CCTV_SWEEP_MIN
            ? (LONG)encoded > (LONG)SetupRead32(record + 0xcc)
            : (LONG)encoded < (LONG)SetupRead32(record + 0xd0))
        { *reasonout = "Sweep minimum must be no greater than sweep maximum. Equal angles hold the camera still."; return FALSE; }
        if (encoded == SetupRead32(record + offset)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    case SETUP_OBJECT_CCTV_SPEED:
        if (edit->value < 0 || edit->value > 2147483647.0 * (21600.0 / 65536.0))
        { *reasonout = "CCTV turn speed is outside the supported range."; return FALSE; }
        encoded = (DWORD)floor(edit->value * (65536.0 / 21600.0) + 0.5);
        if (encoded == SetupRead32(record + 0xdc)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0xdc, encoded);
        break;
    case SETUP_OBJECT_CCTV_RANGE:
        if (edit->value < 0 || edit->value > 2147483647.0 || floor(edit->value) != edit->value)
        { *reasonout = "Enter a whole detection range from 0 to 2147483647 world units. 0 means unlimited."; return FALSE; }
        encoded = (DWORD)edit->value;
        previous = SetupRead32(record + 0xe8);
        /* Stock cameras use -100 for unlimited. Preserve that encoding when
         * accepting the displayed 0 without a semantic change. */
        if (encoded == previous || (!encoded && (LONG)previous <= 0)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0xe8, encoded);
        break;
    case SETUP_OBJECT_DOOR_TRAVEL:
    case SETUP_OBJECT_DOOR_CLEARANCE:
    case SETUP_OBJECT_DOOR_ACCEL:
    case SETUP_OBJECT_DOOR_DECEL:
    case SETUP_OBJECT_DOOR_SPEED:
        if (edit->value < 0 || edit->value > 2147483647.0 / 65536.0)
        { *reasonout = "Door movement is outside the supported range."; return FALSE; }
        encoded = (DWORD)floor(edit->value * 65536.0 + 0.5);
        /* chrobjApplySpeed divides by deceleration; zero acceleration cannot
         * start a stationary door. Require at least one native fixed-point step. */
        if (!encoded && (edit->property == SETUP_OBJECT_DOOR_ACCEL || edit->property == SETUP_OBJECT_DOOR_DECEL))
        { *reasonout = "Door acceleration and deceleration must be positive."; return FALSE; }
        offset = 0x84 + 4 * (edit->property - SETUP_OBJECT_DOOR_TRAVEL);
        if (encoded == SetupRead32(record + offset)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    case SETUP_OBJECT_DOOR_CLOSE_DELAY:
        /* Authored seconds, stored in 60 Hz ticks. The game compares this
         * delay using signed timer arithmetic. Zero means no waiting. */
        if (edit->value < 0 || edit->value > 2147483647.0 / 60.0)
        { *reasonout = "Enter a door close delay from 0 to 35791394.11666667 seconds."; return FALSE; }
        encoded = (DWORD)floor(edit->value * 60.0 + 0.5);
        if (encoded == SetupRead32(record + 0xa0)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0xa0, encoded);
        break;
    case SETUP_OBJECT_DOOR_TYPE:
    case SETUP_OBJECT_DOOR_SOUND:
    case SETUP_OBJECT_DOOR_FLAGS:
    case SETUP_OBJECT_DOOR_KEY_FLAGS:
        if (edit->value < 0 || floor(edit->value) != edit->value
            || edit->value > (edit->property == SETUP_OBJECT_DOOR_TYPE ? DOORTYPE_AZTECCHAIR
                : edit->property == SETUP_OBJECT_DOOR_SOUND ? DOOR_OPEN_SOUND_METAL_4
                : edit->property == SETUP_OBJECT_DOOR_FLAGS ? 65535.0 : 4294967295.0))
        { *reasonout = "Choose a supported door type, sound or flag mask."; return FALSE; }
        encoded = (DWORD)edit->value;
        offset = edit->property == SETUP_OBJECT_DOOR_SOUND ? 0xa4
            : edit->property == SETUP_OBJECT_DOOR_KEY_FLAGS ? 0x9c : 0x98;
        previous = SetupRead32(record + offset);
        if (edit->property == SETUP_OBJECT_DOOR_TYPE) { encoded |= previous & 0xffff0000u; }
        if (edit->property == SETUP_OBJECT_DOOR_FLAGS)
        { encoded = (encoded << 16) | (previous & 0xffffu); }
        if (encoded == previous) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    case SETUP_OBJECT_KEY_FLAGS:
    case SETUP_OBJECT_AMMO_TYPE:
        if (record[3] != (edit->property == SETUP_OBJECT_KEY_FLAGS ? PROPDEF_KEY : PROPDEF_MAGAZINE))
        { *reasonout = "This property does not belong to the selected object type."; return FALSE; }
        if (edit->value < 0 || edit->value > (edit->property == SETUP_OBJECT_KEY_FLAGS ? 4294967295.0 : AMMOTYPE_MAX - 1)
            || floor(edit->value) != edit->value)
        { *reasonout = "Choose a valid ammo type or a 32-bit key mask."; return FALSE; }
        encoded = (DWORD)edit->value;
        if (encoded == SetupRead32(record + 0x80)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0x80, encoded);
        break;
    case SETUP_OBJECT_AMMO_QUANTITY:
        if (record[3] != PROPDEF_AMMO || edit->slot >= AMMOTYPE_GLOBAL_MAX)
        { *reasonout = "Choose a valid ammo crate slot."; return FALSE; }
        if (edit->value < 0 || edit->value > 65535 || floor(edit->value) != edit->value)
        { *reasonout = "Ammo quantities must be whole numbers from 0 to 65535."; return FALSE; }
        encoded = (DWORD)edit->value;
        offset = 0x80 + edit->slot * 4;
        previous = SetupRead32(record + offset);
        /* The upper half contains the original released-model ID. */
        encoded = (previous & 0xffff0000u) | encoded;
        if (encoded == previous) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + offset, encoded);
        break;
    case SETUP_OBJECT_HEALTH:
        if (edit->value < 0 || edit->value > 2147483647.0 / 65536.0)
        { *reasonout = "Health must be between 0 and 32767.99998474121."; return FALSE; }
        encoded = (DWORD)floor(edit->value * 65536.0 + 0.5);
        if (encoded == SetupRead32(record + 0x74)) { return TRUE; }
        SetupWrite32(setup->data + edit->sourceoffset + 0x74, encoded);
        break;
    case SETUP_OBJECT_MODEL:
        if (edit->value < 0 || edit->value > 32767 || floor(edit->value) != edit->value
            || !ModelGetPropDefinition((int)edit->value, &modelname, NULL) || !modelname || !*modelname)
        { *reasonout = "Choose a valid prop model."; return FALSE; }
        if ((short)edit->value == setup->objects[edit->objectindex].modelid) { return TRUE; }
        /* Model and pad share a word; preserve the pad's exact signed bits. */
        encoded = ((DWORD)edit->value << 16) | (SetupRead32(record + 4) & 0xffffu);
        SetupWrite32(setup->data + edit->sourceoffset + 4, encoded);
        setup->objects[edit->objectindex].modelid = (short)edit->value;
        break;
    default:
        *reasonout = "This object property is not editable.";
        return FALSE;
    }
    setup->dirty = TRUE;
    *changedout = TRUE;
    return TRUE;
}
