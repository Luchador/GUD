#include <stdlib.h>
#include <string.h>
#include <src/propconstants.h>
#include "setupselection.h"

BOOL SetupSelectionValid(const SetupFile *s, DWORD id)
{
    DWORD i = id & ~SETUP_CHARACTER_SELECTION_BIT;
    if (!s) { return FALSE; }
    if (id & SETUP_CHARACTER_SELECTION_BIT)
        return s->characters && i < s->charactercount && !s->characters[i].deleted;
    return s->objects && i < s->objectcount && !s->objects[i].deleted;
}

static BOOL Model(const SetupFile *s, DWORD id, unsigned int *model)
{
    if (!SetupSelectionValid(s, id)) { return FALSE; }
    if (id & SETUP_CHARACTER_SELECTION_BIT)
    { *model = s->characters[id & ~SETUP_CHARACTER_SELECTION_BIT].bodyid; return TRUE; }
    const SetupObject *o = &s->objects[id];
    /* Carried/contained records are parts of another viewport model. */
    if (o->modelid < 0 || (o->type != PROPDEF_DOOR
        && (o->flags & (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_INSIDEANOTHEROBJ)))) { return FALSE; }
    *model = o->modelid; return TRUE;
}

BOOL SetupSelectionSimilar(const SetupFile *s, const DWORD *selected, DWORD count,
    DWORD **out, DWORD *outcount, const char **why)
{
    *out = NULL; *outcount = 0; *why = "Select an object or character first.";
    if (!s || !selected || !count) { return FALSE; }
    /* IDs occupy separate namespaces: a character body number must never
     * match an object model with the same numeric value. */
    unsigned char models[2][65536] = {{0}};
    BOOL found = FALSE;
    for (DWORD i = 0; i < count; i++)
    {
        unsigned int model;
        if (Model(s, selected[i], &model))
        { models[(selected[i] & SETUP_CHARACTER_SELECTION_BIT) != 0][model] = 1; found = TRUE; }
    }
    if (!found) { return FALSE; }
    DWORD capacity = s->objectcount + s->charactercount;
    DWORD *ids = capacity ? malloc((size_t)capacity * sizeof(*ids)) : NULL;
    if (capacity && !ids) { *why = "Out of memory selecting similar models."; return FALSE; }
    for (DWORD family = 0; family < 2; family++)
        for (DWORD i = 0; i < (family ? s->charactercount : s->objectcount); i++)
        {
            unsigned int model; DWORD id = i | (family ? SETUP_CHARACTER_SELECTION_BIT : 0);
            if (Model(s, id, &model) && models[family][model]) { ids[(*outcount)++] = id; }
        }
    *out = ids; *why = ""; return TRUE;
}

BOOL SetupSelectionFlags(const SetupFile *s, const DWORD *ids, DWORD count, SetupFlagSummary *out)
{
    memset(out, 0, sizeof(*out)); out->objecttype = -1;
    out->all[0] = out->all[1] = out->all[2] = 0xffffffffu;
    if (!s || (count && !ids)) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        /* The last mode-exclusion bit hides an object just like Delete.
         * Retain its flags so the user can re-enable a mode without losing the group. */
        if (ids[i] & SETUP_CHARACTER_SELECTION_BIT)
        { if (!SetupSelectionValid(s, ids[i])) { return FALSE; } }
        else if (!s->objects || ids[i] >= s->objectcount) { return FALSE; }
        if (ids[i] & SETUP_CHARACTER_SELECTION_BIT)
        {
            DWORD value = s->characters[ids[i] & ~SETUP_CHARACTER_SELECTION_BIT].flags;
            out->characters++; out->all[2] &= value; out->any[2] |= value;
        }
        else
        {
            const SetupObject *o = &s->objects[ids[i]];
            if (!out->objects) { out->objecttype = o->type; }
            else if (out->objecttype != o->type) { out->objecttype = -1; }
            out->objects++;
            out->all[0] &= o->flags; out->any[0] |= o->flags;
            out->all[1] &= o->flags2; out->any[1] |= o->flags2;
        }
    }
    if (!out->objects) { out->all[0] = out->all[1] = 0; }
    if (!out->characters) { out->all[2] = 0; }
    return TRUE;
}

static BOOL CharacterFlag(SetupFile *s, DWORD index, DWORD mask, BOOL enabled, BOOL *changed, const char **why)
{
    SetupCharacter *c = &s->characters[index]; DWORD at = c->sourceoffset;
    if (at < 40 || at > s->size || s->size - at < 28 || s->data[at+3] != PROPDEF_GUARD
        || ((unsigned int)s->data[at+4] << 8 | s->data[at+5]) != c->chrnum
        || ((unsigned int)s->data[at+20] << 8 | s->data[at+21]) != c->flags)
    { *why = "The selected character's source record is inconsistent."; return FALSE; }
    unsigned short value = enabled ? c->flags | mask : c->flags & ~mask;
    *changed = value != c->flags;
    if (*changed)
    { c->flags = value; s->data[at+20] = value >> 8; s->data[at+21] = value; s->dirty = TRUE; }
    return TRUE;
}

BOOL SetupSelectionSetFlag(SetupFile *s, const DWORD *ids, DWORD count,
    unsigned int bank, DWORD mask, BOOL enabled, BOOL *changed, const char **why)
{
    SetupFlagSummary summary; SetupFile copy = {0}; BOOL any = FALSE;
    *changed = FALSE; *why = "The selected flag or model selection is invalid.";
    if (!s || !s->data || !count || bank > 2 || !mask || (mask & (mask-1))
        || (bank == 2 && mask > 0x8000) || !SetupSelectionFlags(s, ids, count, &summary)
        || !(bank == 2 ? summary.characters : summary.objects)) { return FALSE; }
    if (!SetupFileClone(s, &copy, why)) { return FALSE; }
    for (DWORD i = 0; i < count; i++)
    {
        BOOL character = (ids[i] & SETUP_CHARACTER_SELECTION_BIT) != 0, edited = FALSE;
        if (character != (bank == 2)) { continue; }
        BOOL ok = character ? CharacterFlag(&copy, ids[i] & ~SETUP_CHARACTER_SELECTION_BIT, mask, enabled, &edited, why)
            : SetupFileSetObjectFlag(&copy, ids[i], bank, mask, enabled, &edited, why);
        if (!ok) { SetupFileFree(&copy); return FALSE; }
        any |= edited;
    }
    if (any) { SetupFileFree(s); *s = copy; }
    else { SetupFileFree(&copy); }
    *changed = any; *why = ""; return TRUE;
}
