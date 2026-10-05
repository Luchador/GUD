#ifndef GEDITOR_NEWPROPS_H
#define GEDITOR_NEWPROPS_H
#include "rom.h"
#include <src/custompropformat.h>

enum { NEW_MODEL_CHARACTERS, NEW_MODEL_ITEMS, NEW_MODEL_PROPS };
int NewPropsCategory(const char *name);
const char *NewPropsFolder(const char *name);
BOOL NewPropsMakeName(int category,const char *stem,char name[64]);

void NewPropsReset(void);
BOOL NewPropsOpen(const char *project,const char **why);
BOOL NewPropsHasUnsaved(void);
int NewPropsCount(void);
BOOL NewPropsDefinition(int id,const char **name,float *scale);
BOOL NewPropsCharacter(int id,const char **name,int *templateid);
/* -1 for static/stock models. */
int NewPropsCharacterId(const char *name);
int NewPropsCharacterKind(const char *name);
BOOL NewPropsImportCharacter(const char *project,const char *name,const char *path,
    int templateid,BOOL fithead,DWORD *triangles,const char **why);
const unsigned char *NewPropsData(const char *project,const char *name,DWORD *size);
BOOL NewPropsImport(const char *project,const char *name,const char *path,BOOL replace,
    DWORD *triangles,const char **why);
/* Takes ownership only on success; properties retain bounds and placement scale. */
BOOL NewPropsReplace(const char *name,unsigned char *data,DWORD size,const char **why);
BOOL NewPropsSave(const char *project,const char **why);
BOOL NewPropsExportToRom(const char *project,RomFile *rom,const char **why);
BOOL NewPropsCheckRebase(const char *project,const RomFile *rom,const char **why);
#endif
