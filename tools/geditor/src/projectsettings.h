#ifndef GEDITOR_PROJECTSETTINGS_H
#define GEDITOR_PROJECTSETTINGS_H

#include <windows.h>

BOOL ProjectSettingsShow(HWND owner, HINSTANCE instance, const char *projectdir);
void ProjectSettingsRefresh(const char *projectdir);
BOOL ProjectSettingsHandleMessage(MSG *message);

#endif
