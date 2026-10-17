#ifndef GUD_LEVELMENU_H
#define GUD_LEVELMENU_H
#include "../levelmenuassets.h"
extern LevelMenuConfig g_LevelMenuConfig;
extern LevelMenuAssetConfig g_LevelMenuAssets;
void frontApplyLevelMenuWallet(unsigned char *data,unsigned size,unsigned switches,unsigned textures);
int levelMenuBaseStage(int stage);
int levelMenuSelectStage(int stage, int multiplayer);
void levelMenuClearLaunch(void);
int levelMenuGetLaunchStage(int stage, int players);
void frontInitLevelMenu(void);
#endif
