#ifndef GEDITOR_OCCLUDERS_H
#define GEDITOR_OCCLUDERS_H
#include "rom.h"
/* Called after native setup compaction validates its tables and pointers. */
BOOL OccludersValidateNative(const unsigned char *data, DWORD size, const RomFile *rom, const char **why);
#endif
