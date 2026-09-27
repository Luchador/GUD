#ifndef GEDITOR_RENDERSTUDIO_H
#define GEDITOR_RENDERSTUDIO_H
#include "project.h"

BOOL RenderStudioShow(HWND owner, HINSTANCE instance, const GEditorProject *project, const char **why);
void RenderStudioSetProject(const GEditorProject *project);
void RenderStudioClose(void);
BOOL RenderStudioHandleMessage(MSG *message);

#endif
