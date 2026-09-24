#ifndef GEDITOR_TEXTEDITOR_H
#define GEDITOR_TEXTEDITOR_H
#include "project.h"
/* Modal, one file draft at a time. Save File writes directly to the project. */
BOOL TextEditorShow(HWND owner, const GEditorProject *project, DWORD level, const char **why);
#endif
