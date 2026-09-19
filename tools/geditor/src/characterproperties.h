#ifndef GEDITOR_CHARACTERPROPERTIES_H
#define GEDITOR_CHARACTERPROPERTIES_H
#include <windows.h>
#include "setupload.h"
/* Synchronous stack-owned SetupCharacterWeaponEdit; frame owns history. */
#define CHARACTERPROPERTIES_WM_WEAPON_CHANGED (WM_APP + 77)
/* Synchronous stack-owned SetupCharacterHatEdit. */
#define CHARACTERPROPERTIES_WM_HAT_CHANGED (WM_APP + 82)
/* Synchronous stack-owned SetupCharacterBehaviorEdit. */
#define CHARACTERPROPERTIES_WM_BEHAVIOR_CHANGED (WM_APP + 83)
BOOL CharacterPropertiesRegisterClass(HINSTANCE instance);
HWND CharacterPropertiesCreate(HWND parent, HINSTANCE instance);
BOOL CharacterPropertiesSetSelection(HWND panel, const SetupFile *setup, DWORD index);
BOOL CharacterPropertiesHandleMessage(HWND panel, MSG *message);
#endif
