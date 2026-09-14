#ifndef GEDITOR_PORTALOPTIONS_H
#define GEDITOR_PORTALOPTIONS_H

#include "bgdocument.h"

/* A drop supplies the initial center and first room. Cancel leaves out alone. */
BOOL PortalOptionsPrompt(HWND parent, DWORD roomcount, BgPortalPlacement *out);

#endif
