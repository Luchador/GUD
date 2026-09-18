#ifndef GEDITOR_CHARACTERPROPERTIES_H
#define GEDITOR_CHARACTERPROPERTIES_H
#include <windows.h>
#include "setupload.h"
/* Synchronous stack-owned SetupCharacterWeaponEdit; frame owns history. */
#define CHARACTERPROPERTIES_WM_WEAPON_CHANGED (WM_APP + 77)
BOOL CharacterPropertiesRegisterClass(HINSTANCE instance);
HWND CharacterPropertiesCreate(HWND parent, HINSTANCE instance);
BOOL CharacterPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index);
BOOL CharacterPropertiesHandleMessage(HWND panel, MSG *message);
#endif
