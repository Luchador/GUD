#ifndef GUD_CUSTOMPROPS_H
#define GUD_CUSTOMPROPS_H
#include <bondtypes.h>
#include <custompropformat.h>
#include "ob.h"

typedef struct CustomPropRomConfig {
    u32 version;
    u32 romStart;
    u32 romSize;
    u32 reserved;
} CustomPropRomConfig;

extern CustomPropRomConfig g_CustomPropRomConfig;
void customPropsReset(void);
void customPropsInit(void);
ItemModelFileRecord *propModelGet(s32 modelid);
s32 customPropFileIndex(const char *name);
fileentry *customPropFile(s32 index);
resource_lookup_data_entry *customPropFileInfo(s32 index);
#endif
