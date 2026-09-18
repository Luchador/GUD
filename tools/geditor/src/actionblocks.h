#ifndef GEDITOR_ACTIONBLOCKS_H
#define GEDITOR_ACTIONBLOCKS_H
#include "setupload.h"

#define ACTION_OPCODE_COUNT 253
#define ACTION_NAME_SIZE 64
#define ACTION_NOTE_SIZE 256
#define ACTION_MISSING_TARGET ((DWORD)-1)
typedef enum ActionParamKind {
    ACTION_NUMBER, ACTION_LABEL, ACTION_CHARACTER, ACTION_BLOCK, ACTION_PAD,
    ACTION_METERS, ACTION_SECONDS60, ACTION_SECONDS30, ACTION_MASK
} ActionParamKind;
typedef struct ActionParam {
    const char *name;
    unsigned char offset, width, kind;
} ActionParam;
typedef struct ActionOpcode {
    const char *symbol, *title;
    unsigned char size, paramcount;
    ActionParam params[8];
} ActionOpcode;
extern const ActionOpcode g_ActionOpcodes[ACTION_OPCODE_COUNT];
typedef struct ActionInstruction {
    unsigned char *bytes;
    DWORD size, uid, target;
    char name[ACTION_NAME_SIZE], note[ACTION_NOTE_SIZE];
} ActionInstruction;
typedef struct ActionBlock {
    DWORD id, sourceoffset, sourcesize;
    BOOL global, changed, disabled;
    char name[ACTION_NAME_SIZE];
    ActionInstruction *instructions;
    DWORD count;
} ActionBlock;
typedef struct ActionDocument {
    ActionBlock *blocks;
    DWORD count, nextuid, charactercount;
    unsigned short *assignments;
    BOOL changed, globalsloaded;
} ActionDocument;
typedef struct ActionIssue {
    DWORD block, instruction;
    BOOL error;
    char text[192];
} ActionIssue;

BOOL ActionDocumentLoad(const SetupFile *setup, ActionDocument *out, const char **why);
BOOL ActionDocumentLoadGlobals(ActionDocument *doc, const RomFile *rom, const char **why);
BOOL ActionDocumentClone(const ActionDocument *source, ActionDocument *out, const char **why);
void ActionDocumentFree(ActionDocument *doc);
BOOL ActionDocumentAddBlock(ActionDocument *doc, DWORD source, BOOL background, DWORD *out, const char **why);
BOOL ActionDocumentDeleteBlock(ActionDocument *doc, const SetupFile *setup, DWORD block, const char **why);
BOOL ActionDocumentSetEnabled(ActionDocument *doc, DWORD block, BOOL enabled, const char **why);
BOOL ActionBlockInsert(ActionDocument *doc, DWORD block, DWORD before, unsigned int opcode, const char **why);
BOOL ActionBlockDelete(ActionDocument *doc, DWORD block, DWORD instruction, const char **why);
BOOL ActionBlockMove(ActionDocument *doc, DWORD block, DWORD instruction, int direction, const char **why);
BOOL ActionInstructionSet(ActionDocument *doc, DWORD block, DWORD instruction,
    const DWORD values[8], DWORD target, const char *name, const char *note,
    const char *text, const char **why);
BOOL ActionDocumentAssign(ActionDocument *doc, DWORD character, DWORD block, const char **why);
/* Compile into an independent setup clone. No-op edits retain every native byte. */
BOOL ActionDocumentCompile(const ActionDocument *doc, const SetupFile *source,
    SetupFile *out, const char **why);
/* Export-only copy: disabled entries point at End of block. Project bytecode
 * and metadata are never changed. A NULL result means no replacement needed. */
BOOL ActionSetupBuildRuntime(const SetupFile *source, unsigned char **out, DWORD *size, const char **why);
BOOL ActionDocumentValidate(const ActionDocument *doc, const SetupFile *setup,
    ActionIssue **issues, DWORD *count, const char **why);
DWORD ActionReadValue(const ActionInstruction *ins, unsigned int parameter);
BOOL ActionParameterIsPad(const ActionInstruction *ins, unsigned int parameter);
double ActionDisplayValue(const ActionParam *param, DWORD value);
BOOL ActionParseValue(const ActionParam *param, const char *text, DWORD *value);
int ActionTargetParameter(unsigned int opcode);
DWORD ActionFindTarget(const ActionBlock *block, DWORD instruction);
DWORD ActionFindInstruction(const ActionBlock *block, DWORD uid);
void ActionInstructionFormat(const ActionBlock *block, DWORD instruction, char *text, size_t size);
void ActionBlockTitle(const ActionBlock *block, char *text, size_t size);
#endif
