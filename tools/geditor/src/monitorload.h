#ifndef GEDITOR_MONITORLOAD_H
#define GEDITOR_MONITORLOAD_H

#include "rom.h"
#include "monitoranim.h"

typedef struct MonitorSurface {
    DWORD firsttriangle; /* Two consecutive triangles, ordered 012, 023. */
    int animation;
    DWORD seed;
} MonitorSurface;

typedef struct MonitorGeometry {
    MonitorBank bank;
    MonitorSurface *surfaces;
    DWORD count;
} MonitorGeometry;

BOOL MonitorBankLoadRom(MonitorBank *out, const RomFile *rom, const char **reason);
void MonitorGeometryFree(MonitorGeometry *geometry);

#endif
