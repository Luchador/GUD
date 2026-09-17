#ifndef GEDITOR_NEWPROPS_H
#define GEDITOR_NEWPROPS_H
#include "rom.h"
#include <src/custompropformat.h>

void NewPropsReset(void);
BOOL NewPropsOpen(const char *project,const char **why);
BOOL NewPropsHasUnsaved(void);
int NewPropsCount(void);
BOOL NewPropsDefinition(int id,const char **name,float *scale);
const unsigned char *NewPropsData(const char *project,const char *name,DWORD *size);
BOOL NewPropsImport(const char *project,const char *name,const char *path,BOOL replace,
    DWORD *triangles,const char **why);
/* Takes ownership only on success; properties retain bounds and placement scale. */
BOOL NewPropsReplace(const char *name,unsigned char *data,DWORD size,const char **why);
BOOL NewPropsSave(const char *project,const char **why);
BOOL NewPropsExportToRom(const char *project,RomFile *rom,const char **why);
BOOL NewPropsCheckRebase(const char *project,const RomFile *rom,const char **why);
#endif
