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
    case 20: return 0x2d;
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
        else if (type == PROPDEF_GUARD)
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
        else if (type == PROPDEF_GUARD)
        {
            SetupCharacter *character = &setup->characters[characterat++];

            character->chrnum = (unsigned short)SetupRead16(record + 4);
            character->pad = (unsigned short)SetupRead16(record + 6);
            character->bodyid = (unsigned short)SetupRead16(record + 8);
            character->ailistid = (unsigned short)SetupRead16(record + 10);
            character->flags = (unsigned short)SetupRead16(record + 20);
            character->headid = SetupRead16(record + 22);
            character->sourceoffset = at;
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
BOOL SetupFileAddModel(SetupFile *setup, BOOL character, int modelid, float levelscale,
                       const double position[3], DWORD *selectionout, const char **reasonout)
{
    SetupFile added = {0};
    DWORD oldcommands, commandend, commandsize, commandcount = 0;
    DWORD oldpads, newcommands, newrecord, newpads, newpad, chrnum = 0, i;
    unsigned char type = character ? PROPDEF_GUARD : PROPDEF_PROP;
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
    if (setup->padcount >= (character ? SETUP_PAD_MAX : 10000u))
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
        if (oldcommands < SETUP_HEADER_SIZE)
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
    oldpads = SetupRead32(setup->data + SETUP_PAD_POINTER);
    if (oldpads < SETUP_HEADER_SIZE || oldpads > setup->size ||
        setup->padcount > (setup->size - oldpads) / SETUP_PAD_SIZE)
    {
        goto malformed;
    }

    newcommands = (setup->size + 3u) & ~3u;
    newrecord = newcommands + commandsize;
    newpads = newrecord + recordsize + 4;
    newpad = newpads + setup->padcount * SETUP_PAD_SIZE;
    added.size = newpad + 2 * SETUP_PAD_SIZE;
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
    memcpy(added.data + newpads, setup->data + oldpads, setup->padcount * SETUP_PAD_SIZE);
    SetupWrite32(added.data + SETUP_OBJECT_POINTER, newcommands);
    SetupWrite32(added.data + SETUP_PAD_POINTER, newpads);
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
    SetupWrite32(added.data + newpad + SETUP_PAD_LINK, newpad + SETUP_PAD_SIZE + SETUP_PAD_LINK);

    if (character)
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
        SetupWrite32(added.data + newrecord + 4, ((DWORD)modelid << 16) | setup->padcount);
        SetupWrite32(added.data + newrecord + 8,
                     PROPFLAG_FORCE_COLLISIONS | PROPFLAG_ABSOLUTEPOSITION);
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
        if (character->sourceoffset > setup->size - 28
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
        if (!(object->flags & (PROPFLAG_ONSIDE | PROPFLAG_UPSIDEDOWN))) { object->flags |= PROPFLAG_INAIR; }
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
        if (index >= setup->charactercount)
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
