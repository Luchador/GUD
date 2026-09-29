#ifndef GEDITOR_ACTIONTEXT_H
#define GEDITOR_ACTIONTEXT_H
#include "actionblocks.h"
#include "textbank.h"

typedef struct ActionTextBank {
    TextBank text;
    char name[64], error[160];
} ActionTextBank;
typedef struct ActionText {
    ActionTextBank banks[TEXT_BANK_MAX_FILES];
    char error[160];
} ActionText;

/* Read-only snapshot for one modal editor session. Missing/bad text does not
 * prevent editing scripts. Load into a zeroed or previously freed snapshot. */
void ActionTextLoad(ActionText *text, const char *dir, const RomFile *rom);
void ActionTextFree(ActionText *text);
const char *ActionTextResolve(const ActionText *text, DWORD id,
    const char **bankname, const char **why);
void ActionTextSummary(const ActionText *text, DWORD id, char *out, size_t size);
void ActionTextInstructionFormat(const ActionText *text, const ActionBlock *block,
    DWORD instruction, char *out, size_t size);
#endif
