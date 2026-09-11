#ifndef GEDITOR_OBJECTFLAGCATALOG_H
#define GEDITOR_OBJECTFLAGCATALOG_H

#include <src/propconstants.h>

typedef struct ObjectFlagDefinition {
    unsigned int bank, mask;
    const char *name, *label, *description;
    /* PROPDEF_NOTHING: shared option; -1: game-managed state, never editable. */
    int type;
} ObjectFlagDefinition;

static const ObjectFlagDefinition g_ObjectFlags[] = {
#define OBJECT_FLAG(bank, symbol, label, description) \
    {bank, (unsigned int)symbol, #symbol, label, description, PROPDEF_NOTHING},
#define OBJECT_FLAG_FOR_TYPE(bank, symbol, label, description, type) \
    {bank, (unsigned int)symbol, #symbol, label, description, type},
#define OBJECT_FLAG_GAME_STATE(bank, symbol, label, description) \
    {bank, (unsigned int)symbol, #symbol, label, description, -1},
#include "objectflagdefs.h"
#undef OBJECT_FLAG
#undef OBJECT_FLAG_FOR_TYPE
#undef OBJECT_FLAG_GAME_STATE
};
#define OBJECTFLAGS_COUNT ((int)(sizeof(g_ObjectFlags) / sizeof(*g_ObjectFlags)))

static int ObjectFlagAppliesToType(const ObjectFlagDefinition *flag, int type)
{
    return flag->type == PROPDEF_NOTHING || (flag->type >= 0 && flag->type == type);
}

#endif
