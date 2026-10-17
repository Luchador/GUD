#ifndef GUD_LEVELMENU_H
#define GUD_LEVELMENU_H
#include "../levelmenuformat.h"
extern LevelMenuConfig g_LevelMenuConfig;
int levelMenuBaseStage(int stage);
int levelMenuSelectStage(int stage, int multiplayer);
void levelMenuClearLaunch(void);
int levelMenuGetLaunchStage(int stage, int players);
void frontInitLevelMenu(void);
#endif
