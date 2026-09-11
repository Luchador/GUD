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

static BOOL SetupReadPad(const SetupFile *setup, const unsigned char *record,
                          SetupPad *pad)
{
    int axis;
    DWORD link = SetupRead32(record + SETUP_PAD_LINK);
    const unsigned char *end;

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

/* Copy the command stream before appending: growing it in place would overwrite
   another setup section. Internal links are file-relative offsets or command
   indices, so retaining the old data and command order preserves both. */
static BOOL SetupAddPlacement(SetupFile *setup, unsigned char type, int modelid, float levelscale,
                              const double position[3], const SetupBoundPad *bound,
                              DWORD *selectionout, const char **reasonout)
{
    SetupFile added = {0};
    DWORD oldcommands, commandend, commandsize, commandcount = 0;
    DWORD oldpads, newcommands, newrecord, newpads, newpad, chrnum = 0, i;
    BOOL door = type == PROPDEF_DOOR, character = type == PROPDEF_GUARD;
    DWORD padheader = bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER;
    DWORD padsize = bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    DWORD padcount = setup ? (bound ? setup->boundpadcount : setup->padcount) : 0;
    DWORD recordsize = SetupObjectWordCount(type) * 4;
    float authored[3];

    if (setup == NULL || setup->data == NULL || setup->size < SETUP_HEADER_SIZE ||
        setup->size > SETUP_FILE_MAX || position == NULL || selectionout == NULL || modelid < 0 ||
        modelid > 32767 || !isfinite(levelscale) || levelscale <= 0 ||
        (setup->charactercount > 0 && setup->characters == NULL))
    {
        *reasonout = "The setup, model or level scale is invalid.";
        return FALSE;
    }
    /* Ordinary props reserve pad numbers 10000 and above for bound pads.
       Characters can address the entire unsigned 16-bit normal-pad range. */
    /* Door indices omit +10000; other bound props must leave room for it
       within the signed 16-bit pad field. */
    if (padcount >= (door ? 32768u : bound ? 22768u : character ? SETUP_PAD_MAX : 10000u))
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
    }
    if (character)
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
            if (++commandcount >= SETUP_OBJECT_MAX - 1)
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
    newrecord = newcommands + commandsize;
    newpads = newrecord + recordsize + 4;
    newpad = newpads + padcount * padsize;
    added.size = newpad + 2 * padsize;
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
    SetupWrite32(added.data + newrecord + recordsize, SETUP_PROP_END);
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
       at the new position. The following pad remains the null terminator. */
    SetupWrite32(added.data + newpad + SETUP_PAD_LINK, newpad + padsize + SETUP_PAD_LINK);

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
        SetupWrite32(added.data + newrecord + 4, ((DWORD)modelid << 16) | padcount);
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
        SetupWrite32(added.data + newrecord + 4, (chrnum << 16) | setup->padcount);
        /* GAILIST_DEAD_AI (1) yields forever. No weapons or patrol/mission
           behavior is implicitly assigned to a newly placed character. */
        SetupWrite32(added.data + newrecord + 8, ((DWORD)modelid << 16) | 1u);
        SetupWrite32(added.data + newrecord + 12, 0xffffffffu);          /* no presets */
        SetupWrite32(added.data + newrecord + 16, (1000u << 16) | 100u); /* hearing/vision */
        SetupWrite32(added.data + newrecord + 20, 0x0000ffffu);          /* random head */
    }
    else
    {
        SetupWrite32(added.data + newrecord, (256u << 16) | type); /* scale 1 */
        SetupWrite32(added.data + newrecord + 4, ((DWORD)modelid << 16) | (padcount + (bound ? 10000u : 0u)));
        if (type == PROPDEF_GLASS)
        {
            /* Match ordinary breakable free-standing panes in the game:
             * block movement, permit AI sight, and fit the flat window model
             * to the bound's width/height. No visibility portal is claimed.
             * GLASS uses a 32-word ObjectRecord in setup, not sizeof(GlassRecord). */
            SetupWrite32(added.data + newrecord + 8,
                PROPFLAG_FREE_STANDING_GLASS | PROPFLAG_FORCE_COLLISIONS | PROPFLAG_TRANSPARENT_TO_AI |
                PROPFLAG_ORTHOGONAL | PROPFLAG_ONSIDE | PROPFLAG_SCALE_TO_X_BOUNDS | PROPFLAG_SCALE_TO_Y_BOUNDS);
        }
        else
        {
            SetupWrite32(added.data + newrecord + 8,
                         PROPFLAG_FORCE_COLLISIONS | PROPFLAG_ABSOLUTEPOSITION);
        }
        /* ObjectRecord.damage is authored as signed 16.16 durability and
           converted by domakedefaultobj. maxdamage starts at zero. */
        SetupWrite32(added.data + newrecord + 0x74, 1000u << 16);
    }
    if (!SetupParsePads(&added, reasonout) || !SetupParseObjects(&added, reasonout))
    {
        SetupFileFree(&added);
        return FALSE;
    }
    *selectionout =
        character ? (SETUP_CHARACTER_SELECTION_BIT | setup->charactercount) : setup->objectcount;
    added.dirty = TRUE;
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
        modelid, levelscale, position, NULL, selectionout, reasonout);
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
    return SetupAddPlacement(setup, PROPDEF_DOOR, modelid, levelscale, position, &pad, selectionout, reasonout);
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
    return SetupAddPlacement(setup, PROPDEF_GLASS, modelid, levelscale, position, &pad, selectionout, reasonout);
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
        if (command >= SETUP_OBJECT_MAX - 1 || setup->padcount >= SETUP_PAD_MAX)
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
    newintro = (setup->size + 3u) & ~3u;
    edited.size = newintro + (end - intro) + 4 + (placing ? 12 + (setup->padcount + 2) * SETUP_PAD_SIZE : 0);
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
                SetupWrite32(edited.data + write + 4, setup->padcount);
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
        SetupWrite32(edited.data + write + 4, setup->padcount);
        selected = written;
        write += 12;
    }
    SetupWrite32(edited.data + write, 9);
    SetupWrite32(edited.data + 8, newintro);
    edited.size = write + 4;
    if (placing)
    {
        newpads = edited.size;
        newpad = newpads + setup->padcount * SETUP_PAD_SIZE;
        memcpy(edited.data + newpads, setup->data + oldpads, setup->padcount * SETUP_PAD_SIZE);
        SetupWrite32(edited.data + SETUP_PAD_POINTER, newpads);
        for (int axis = 0; axis < 3; axis++)
        {
            union { float f; DWORD u; } value;
            value.f = native[axis]; SetupWrite32(edited.data + newpad + axis * 4, value.u);
            value.f = look[axis]; SetupWrite32(edited.data + newpad + 24 + axis * 4, value.u);
        }
        SetupWrite32(edited.data + newpad + 16, 0x3f800000u); /* up = +Y */
        SetupWrite32(edited.data + newpad + SETUP_PAD_LINK, newpad + SETUP_PAD_SIZE + SETUP_PAD_LINK);
        edited.size = newpad + 2 * SETUP_PAD_SIZE;
    }
    if (!SetupParsePads(&edited, reasonout) || !SetupParseObjects(&edited, reasonout))
    { SetupFileFree(&edited); return FALSE; }
    edited.dirty = TRUE;
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
        || out->size > SETUP_FILE_MAX)
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

    if (!SetupParsePads(out, reasonout)
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
    char path[MAX_PATH];
    HANDLE file;
    DWORD written;
    BOOL ok;

    *reasonout = "";

    if (setup == NULL || setup->data == NULL || setup->size == 0
        || !SetupProjectPath(path, sizeof(path), projectdir, setup->name))
    {
        *reasonout = "there is no valid setup loaded to save.";
        return FALSE;
    }

    file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        *reasonout = "the project setup could not be opened for writing.";
        return FALSE;
    }

    ok = WriteFile(file, setup->data, setup->size, &written, NULL)
      && written == setup->size;
    if (!CloseHandle(file))
    {
        ok = FALSE;
    }

    if (!ok)
    {
        *reasonout = "the project setup could not be fully written.";
    }

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
        || (source->charactercount > 0 && source->characters == NULL))
    {
        *reasonout = "the setup document is incomplete.";
        return FALSE;
    }

    if (source->size > 0)
    {
        out->data = (unsigned char *)malloc(source->size);
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

/* Give an explicitly moved model its own pad. Appending a replacement pad
 * table keeps every existing setup command index and embedded pointer valid.
 * An editor-created final pad/table can be reused on subsequent drags. */
BOOL SetupFileTranslateModel(SetupFile *setup, DWORD selection,
                              float levelscale, const double offset[3],
                              const char **reasonout)
{
    SetupObject *object = NULL;
    SetupCharacter *character = NULL;
    SetupPad *pad;
    DWORD header, stride, count, table, index, record, end, i;
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
    /* The private pad's empty plink points at its own terminator's null
       link. This both identifies our tail table and requests the game's
       nearest-stan lookup at the new location, rather than a stale link. */
    reuse = index == count - 1 && end + stride == setup->size
         && SetupRead32(setup->data + record + SETUP_PAD_LINK) == end + SETUP_PAD_LINK;
    for (i = 0; reuse && i < setup->objectcount; i++)
    {
        const SetupObject *other = &setup->objects[i];
        BOOL otherbound = other->type == PROPDEF_DOOR || other->pad >= 10000;
        int otherindex = other->pad - (otherbound && other->type != PROPDEF_DOOR ? 10000 : 0);
        if ((character != NULL || i != owner)
            && otherbound == bound && otherindex == (int)index) { reuse = FALSE; }
    }
    for (i = 0; reuse && !bound && i < setup->charactercount; i++)
    {
        if ((character == NULL || i != owner)
            && setup->characters[i].pad == index) { reuse = FALSE; }
    }
    if (!reuse)
    {
        DWORD newtable = (setup->size + 3) & ~3u;
        DWORD newsize = newtable + (count + 2) * stride;
        unsigned char *data;
        void *pads;
        DWORD encoded = count + (bound && !door ? 10000 : 0);
        if (count > maxindex || newsize > SETUP_FILE_MAX)
        {
            *reasonout = "The setup has no room for another placement pad.";
            return FALSE;
        }
        data = calloc(newsize, 1);
        pads = malloc((size_t)(count + 1) * (bound ? sizeof(SetupBoundPad) : sizeof(SetupPad)));
        if (data == NULL || pads == NULL)
        {
            free(data); free(pads); *reasonout = "Out of memory copying the model's pad."; return FALSE;
        }
        memcpy(data, setup->data, setup->size);
        memcpy(data + newtable, setup->data + table, (size_t)count * stride);
        memcpy(data + newtable + count * stride, setup->data + record, stride);
        if (bound)
        {
            SetupBoundPad *list = pads;
            memcpy(list, setup->boundpads, (size_t)count * sizeof(*list));
            list[count] = setup->boundpads[index];
            free(setup->boundpads); setup->boundpads = list; setup->boundpadcount++;
        }
        else
        {
            SetupPad *list = pads;
            memcpy(list, setup->pads, (size_t)count * sizeof(*list));
            list[count] = setup->pads[index];
            free(setup->pads); setup->pads = list; setup->padcount++;
        }
        free(setup->data); setup->data = data; setup->size = newsize;
        SetupWrite32(data + header, newtable);
        if (character != NULL) { character->pad = (unsigned short)encoded; }
        else { object->pad = (short)encoded; }
        data[sourceoffset + 6] = (unsigned char)(encoded >> 8);
        data[sourceoffset + 7] = (unsigned char)encoded;
        record = newtable + count * stride;
        end = record + stride;
        SetupWrite32(data + record + SETUP_PAD_LINK, end + SETUP_PAD_LINK);
        index = count;
    }
    pad = bound ? &setup->boundpads[index].pad : &setup->pads[index];
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
        value.f = (float)up[axis];
        *changed |= value.f != pad->up[axis];
        pad->up[axis] = value.f;
        SetupWrite32(setup->data + record + 12 + axis * 4, value.u);
        value.f = (float)look[axis];
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
 * lookup. Never move a shared authored room pad. Reuse our private tail pad
 * on subsequent moves; a moved table preserves all other pad indices. */
static BOOL SetupMoveCameraPad(SetupFile *setup, DWORD command, float levelscale,
                              const DWORD coordinates[3], const char **why)
{
    DWORD table = SetupRead32(setup->data + SETUP_PAD_POINTER), count = setup->padcount;
    DWORD old = SetupRead32(setup->data + command + 24), end, record, i;
    float pos[3];
    BOOL reuse;
    if (table < SETUP_HEADER_SIZE || table > setup->size || count >= SETUP_PAD_MAX
        || count + 1 > (setup->size - table) / SETUP_PAD_SIZE || (count && !setup->pads)) { return FALSE; }
    for (i = 0; i < 3; i++)
    {
        double value = ((LONG)coordinates[i] / 100.0) * levelscale;
        if (!isfinite(value) || fabs(value) > 100000000.0) { return FALSE; }
        pos[i] = (float)value;
    }
    end = table + count * SETUP_PAD_SIZE;
    record = old < count ? table + old * SETUP_PAD_SIZE : 0;
    reuse = count && old == count - 1 && end + SETUP_PAD_SIZE == setup->size
        && SetupRead32(setup->data + record + SETUP_PAD_LINK) == end + SETUP_PAD_LINK;
    /* The marker owns this pad only if no other intro/prop command uses it. */
    for (i = 0; reuse && i < 2; i++)
    {
        DWORD at = SetupRead32(setup->data + 8 + i * 4), n;
        if (!at) { continue; }
        for (n = 0; n < SETUP_OBJECT_MAX; n++)
        {
            DWORD type, bytes, pad = (DWORD)-1;
            if (at > setup->size || setup->size - at < 4) { reuse = FALSE; break; }
            type = i ? setup->data[at + 3] : SetupRead32(setup->data + at);
            if (type == (i ? SETUP_PROP_END : 9)) { break; }
            bytes = (i ? SetupObjectWordCount((unsigned char)type) : SetupIntroWordCount(type)) * 4;
            if (!bytes || bytes > setup->size - at) { reuse = FALSE; break; }
            if ((!i && type == 6) || (i && type == PROPDEF_CAMERAPOS)) { pad = SetupRead32(setup->data + at + 24); }
            if (!i && type == 0) { pad = SetupRead32(setup->data + at + 4); }
            if (at != command && pad == old) { reuse = FALSE; break; }
            at += bytes;
        }
    }
    for (i = 0; reuse && i < setup->charactercount; i++) { if (setup->characters[i].pad == old) { reuse = FALSE; } }
    for (i = 0; reuse && i < setup->objectcount; i++) { if (setup->objects[i].pad == (int)old) { reuse = FALSE; } }
    if (!reuse)
    {
        DWORD newtable = (setup->size + 3u) & ~3u, newsize = newtable + (count + 2) * SETUP_PAD_SIZE;
        unsigned char *data;
        SetupPad *pads;
        if (newsize > SETUP_FILE_MAX) { *why = "The setup has no room for another camera pad."; return FALSE; }
        data = calloc(newsize, 1); pads = calloc(count + 1, sizeof(*pads));
        if (!data || !pads) { free(data); free(pads); *why = "Out of memory moving the camera's room pad."; return FALSE; }
        memcpy(data, setup->data, setup->size);
        memcpy(data + newtable, setup->data + table, count * SETUP_PAD_SIZE);
        if (count) { memcpy(pads, setup->pads, count * sizeof(*pads)); }
        free(setup->data); free(setup->pads);
        setup->data = data; setup->size = newsize; setup->pads = pads; setup->padcount = count + 1;
        SetupWrite32(data + SETUP_PAD_POINTER, newtable);
        old = count; record = newtable + count * SETUP_PAD_SIZE; end = record + SETUP_PAD_SIZE;
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
    DWORD oldtable = SetupRead32(setup->data + SETUP_BOUNDPAD_POINTER);
    DWORD table = (setup->size + 3) & ~3u;
    DWORD size = table + (count + 2) * SETUP_BOUNDPAD_SIZE;
    DWORD sourcetable =
        SetupRead32(setup->data + (source->bound ? SETUP_BOUNDPAD_POINTER : SETUP_PAD_POINTER));
    DWORD stride = source->bound ? SETUP_BOUNDPAD_SIZE : SETUP_PAD_SIZE;
    unsigned char *data;
    SetupBoundPad *pads;
    if (count > 22767 || size > SETUP_FILE_MAX || oldtable > setup->size ||
        count > (setup->size - oldtable) / SETUP_BOUNDPAD_SIZE ||
        source->index >= (source->bound ? count : setup->padcount) || sourcetable > setup->size ||
        source->index + 1 > (setup->size - sourcetable) / stride)
    {
        *reasonout = "The setup has no room for another bound pad.";
        return FALSE;
    }
    data = calloc(size, 1);
    pads = calloc(count + 1, sizeof(*pads));
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
    memcpy(data + table + count * SETUP_BOUNDPAD_SIZE,
           setup->data + sourcetable + source->index * stride, stride);
    if (source->bound)
    {
        pads[count] = setup->boundpads[source->index];
    }
    else
    {
        pads[count].pad = setup->pads[source->index];
    }
    /* Empty stan link, with a non-null pointer so this remains a live record. */
    SetupWrite32(data + table + count * SETUP_BOUNDPAD_SIZE + SETUP_PAD_LINK,
                 table + (count + 1) * SETUP_BOUNDPAD_SIZE + SETUP_PAD_LINK);
    pads[count].pad.stanname[0] = '\0';
    SetupWrite32(data + SETUP_BOUNDPAD_POINTER, table);
    free(setup->data);
    free(setup->boundpads);
    setup->data = data;
    setup->size = size;
    setup->boundpads = pads;
    setup->boundpadcount = count + 1;
    out->index = count;
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
    double zero[3] = {0};
    if ((selection & SETUP_CHARACTER_SELECTION_BIT) ||
        !SetupFileGetModelPad(setup, selection, &ref))
    {
        *reasonout = "Only props can be scaled.";
        return FALSE;
    }
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

BOOL SetupFileGetObjectProperties(const SetupFile *setup, DWORD index,
                                  SetupObjectProperties *out, const char **reasonout)
{
    const unsigned char *record = SetupObjectPropertyRecord(setup, index, reasonout);
    if (!record || !out) { return FALSE; }
    memset(out, 0, sizeof(*out));
    out->object = setup->objects[index];
    out->health = (LONG)SetupRead32(record + 0x74) / 65536.0;
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
    case SETUP_OBJECT_DRONE_AIM_PAD:
        if (edit->value < -1 || edit->value > 2147483647.0 || floor(edit->value) != edit->value)
        { *reasonout = "Choose an existing aim pad or the default +Z direction."; return FALSE; }
        encoded = (DWORD)(LONG)edit->value;
        if (edit->value >= 0 && (encoded < 10000 ? encoded >= setup->padcount : encoded - 10000 >= setup->boundpadcount))
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
        if (encoded < 10000 ? encoded >= setup->padcount : encoded - 10000 >= setup->boundpadcount)
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
