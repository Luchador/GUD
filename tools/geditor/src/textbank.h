#ifndef GEDITOR_TEXTBANK_H
#define GEDITOR_TEXTBANK_H
#include "rom.h"
#include <wchar.h>

/* The briefing screen supplies 512 * 879 bytes to langLoadToBank. */
#define TEXT_BANK_MAX_SIZE (512u * 879u)
#define TEXT_BANK_MAX_SLOTS 1024u
#define TEXT_BANK_MAX_FILES 64u
#define TEXT_BANK_UNKNOWN_ID ((DWORD)-1)

typedef struct TextBank { unsigned char *data; DWORD size, count; } TextBank;
typedef struct TextBankFile { char name[64]; DWORD id; } TextBankFile;

BOOL TextBankIsResource(const char *name);
BOOL TextBankProjectPath(const char *dir, const char *name, char *path, size_t size);
BOOL TextBankCatalog(const RomFile *rom, TextBankFile *files, DWORD *count, const char **why);
BOOL TextBankValidate(const unsigned char *data, DWORD size, DWORD *count, const char **why);
BOOL TextBankLoad(const unsigned char *data, DWORD size, TextBank *out, const char **why);
void TextBankFree(TextBank *bank);
const char *TextBankString(const TextBank *bank, DWORD slot); /* NULL is an unused slot. */
BOOL TextBankEqual(const unsigned char *a, DWORD asize, const unsigned char *b, DWORD bsize);
BOOL TextBankSet(TextBank *bank, DWORD slot, const char *text, const char **why);
/* Lossless ASCII editing: real newlines, \\, \n, \r, \t and \xHH escapes. */
wchar_t *TextBankFormat(const char *text);
BOOL TextBankParse(const wchar_t *text, char **out, const char **why);
BOOL TextBankLoadProject(const char *dir, const RomFile *rom, const char *name,
    TextBank *out, const char **why);
BOOL TextBankSaveProject(const char *dir, const char *name, const TextBank *bank, const char **why);
#endif
