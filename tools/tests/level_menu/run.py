#!/usr/bin/env python3
"""Compile the production menu tables, assignment logic and stage loader."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[3]
front = (root / 'src/game/front.c').read_text()
lv = (root / 'src/game/lv.c').read_text()
constants = (root / 'src/bondconstants.h').read_text()
def function(source, name):
    m = re.search(r'^[\w *]+\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert m, name
    end = source.index('{', m.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[m.start():end] + '\n'
source = '''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "campaign.h"
#include "levelmenu.h"
typedef uint8_t u8; typedef uint16_t u16; typedef int16_t s16;
typedef int32_t s32; typedef uint32_t u32; typedef float f32;
#define TRUE 1
#define FALSE 0
#define ARRAYCOUNT(a) (sizeof(a)/sizeof((a)[0]))
#define getStringID(bank,string) 1
enum { MISSION_PART, MISSION_HEADER };
'''
for name in ('DIFFICULTY', 'MUSIC_TRACKS'):
    source += re.search(r'typedef enum ' + name + r'\s*\{.*?\}[^;]*;', constants, re.S)[0] + '\n'
source += re.search(r'enum WALLETBOND_SWITCHNAMES\s*\{.*?\}\s*;', constants, re.S)[0] + '\n'
for name in sorted(set(re.findall(r'\bIMG_MP_\w+', front))):
    source += f'#define {name} {len(source)}\n'
source += re.search(r'struct mission_folder_setup mission_folder_setup_entries\[\] = \{.*?\n\};', front, re.S)[0] + '\n'
source += 'const int g_CampaignEntryCount=ARRAYCOUNT(mission_folder_setup_entries)-1;\n'
source += re.search(r'struct mp_stage_setup\s*\{.*?\};', (root/'src/game/front.h').read_text(), re.S)[0] + '\n'
source += re.search(r'struct mp_stage_setup multi_stage_setups\[\] = \{.*?\n\};', front, re.S)[0] + '\n'
header=(root/'src/game/lv.h').read_text()
source += re.search(r'struct LevelEntry\s*\{.*?\};',header,re.S)[0]+'\n'
source += re.search(r'enum LevelCatalogSlot\s*\{.*?\};',header,re.S)[0]+'\n'
source += '#define LEVEL_MEMORY_STRING_SIZE 64\n'
source += lv[lv.index('#define LEVEL('):lv.index('struct LevelEntry *lvFindLevelInfo')]
for name in ('lvFindLevelInfo','lvFindStageInfo','lvGetMemoryAllocationString'):
    source += function(lv,name)
source += '''static int g_LevelMenuInitialized, g_LevelMenuActive;
static int g_LevelMenuBriefingStage[20];
static struct {unsigned textureCount;} g_TextureRomConfig={4096};
static int players=4, selected_folder_num, unlocked=1;
static u32 randomValue;
static u32 randomGetNext(void) { return randomValue; }
static int get_selected_num_players(void) {return players;}
static int fileIsStageUnlockedAtDifficulty(int folder,int slot,int difficulty)
{(void)folder;(void)slot;(void)difficulty;return unlocked;}
static char *localized="localized";
static char *langGet(int preset) {(void)preset;return localized;}
'''
for name in ('frontMenuAsset','frontBriefingStage','frontInitLevelMenu','frontLevelMenuNameFor','frontLevelMenuName',
             'frontSoloPartName','frontSoloMissionName','frontSoloLevelName','frontSelectSoloStage',
             'check_if_mp_stage_unlocked','frontSelectMultiStage'):
    source += function(front,name)
# Exercise the real text measurement and label layout, including the solo
# screen's own trailing newline, rather than checking returned names alone.
font_types=(root/'src/bondtypes.h').read_text()
for name in ('fontchar','font'):
    source += re.search(r'struct '+name+r'\s*\{.*?\};',font_types,re.S)[0]+'\n'
source += '''static struct font testfont;
static struct font *ptrFontBankGothic=&testfont;
static struct fontchar *ptrFontBankGothicChars=testfont.chars;
static int text_spacing;
#define SPACE_WIDTH 5
'''
source += function((root/'src/game/textrelated.c').read_text(),'textMeasure')
for axis in 'xy':
    source += re.search(r's32 cursor_'+axis+r'pos_table_mission_select\[\] = .*?;',front)[0]+'\n'
solo=function(front,'constructor_menu07_missionsel')
source += '''static void SoloLabel(int temp_s0,int spD8,int i,int out[4]) {
    char sp90[64];int temp_v0_3,spC4,spC8,sp8C,sp88;
'''
source += solo[solo.index('                if (mission_folder_setup_entries[temp_s0].icon_text_preset)'):
               solo.index('                DL = gfxDrawTranslucentRect')]
source += 'out[0]=sp8C;out[1]=sp88;out[2]=spC4;out[3]=spC8;}\n'
mp=function(front,'constructor_menu12_mpstage')
start=mp.index('                    textMeasure(&sp128, &sp124, frontLevelMenuName')
source += 'static void MultiLabel(int count_2,int var_s6,int sp90,int out[4]) {int sp128,sp124,sp130,sp12C;\n'
source += mp[start:mp.index('                    DL = gfxDrawTranslucentRect',start)]
source += 'out[0]=sp130;out[1]=sp12C;out[2]=sp124;out[3]=sp128;}\n'
source += r'''
static void CheckLabelAlignment(void)
{
    const char *names[]={"Archives","A","Night Harbour","1234567890123456789012345678901"};
    int expected[4],actual[4],entry=campaignGetEntryByOrder(0);
    const int defaults[32]=LEVEL_MENU_DEFAULT_STAGES;
    char *saved=lvFindLevelInfo(LEVELID_ARCHIVES)->levelName;
    g_LevelMenuActive=1;
    for(int i=0;i<94;i++){testfont.chars[i].width=6;testfont.chars[i].height=10;testfont.chars[i].baseline=1;}
    for(int i=0;i<13*13;i++)testfont.kerning[i]=1;
    for(unsigned n=0;n<ARRAYCOUNT(names);n++) {
        localized=(char *)names[n];
        for(int slot=0;slot<20;slot++) {
            LevelMenuAsset *a=&g_LevelMenuAssets.slots[0];memset(a,0,sizeof(*a));
            g_LevelMenuConfig.stages[0]=defaults[0];SoloLabel(entry,slot%5,slot/5,expected);
            assert(expected[3]==11); /* One line using deterministic font metrics. */
            a->flags=LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_NAME);strcpy(a->names[0],names[n]);
            SoloLabel(entry,slot%5,slot/5,actual);assert(!memcmp(actual,expected,sizeof(actual)));
            memset(a,0,sizeof(*a));g_LevelMenuConfig.stages[0]=LEVELID_ARCHIVES;
            lvFindLevelInfo(LEVELID_ARCHIVES)->levelName=(char *)names[n];
            SoloLabel(entry,slot%5,slot/5,actual);assert(!memcmp(actual,expected,sizeof(actual)));
        }
        for(int slot=20;slot<32;slot++) {
            LevelMenuAsset *a=&g_LevelMenuAssets.slots[slot];memset(a,0,sizeof(*a));
            g_LevelMenuConfig.stages[slot]=defaults[slot];MultiLabel(slot-20,86+(slot-20)%4*85,151+(slot-20)/4*70,expected);
            a->flags=LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_NAME);strcpy(a->names[0],names[n]);
            MultiLabel(slot-20,86+(slot-20)%4*85,151+(slot-20)/4*70,actual);assert(!memcmp(actual,expected,sizeof(actual)));
            memset(a,0,sizeof(*a));g_LevelMenuConfig.stages[slot]=LEVELID_ARCHIVES;
            MultiLabel(slot-20,86+(slot-20)%4*85,151+(slot-20)/4*70,actual);assert(!memcmp(actual,expected,sizeof(actual)));
        }
    }
    lvFindLevelInfo(LEVELID_ARCHIVES)->levelName=saved;localized="localized";
    memcpy(g_LevelMenuConfig.stages,defaults,sizeof(defaults));g_LevelMenuActive=0;
    puts("PASS: stock, custom and reassigned-level labels have identical bounds/positions in all 20 solo and 12 MP boxes, including 31-character names.");
}
int main(void)
{
    CheckLabelAlignment();
    struct mission_folder_setup original[20];struct mp_stage_setup arenas[12];
    for(int i=0;i<20;i++)original[i]=mission_folder_setup_entries[campaignGetEntryByOrder(i)];
    memcpy(arenas,multi_stage_setups,sizeof(arenas));
    g_LevelMenuConfig.stages[0]=LEVELID_FRIGATE;
    g_LevelMenuConfig.stages[1]=LEVELID_FRIGATE; /* Same stage, distinct progress. */
    g_LevelMenuConfig.stages[2]=LEVELID_COMPLEX; /* No solo briefing. */
    g_LevelMenuConfig.stages[3]=LEVELID_FACILITY_MP;
    g_LevelMenuConfig.stages[20]=LEVELID_FACILITY; /* Exact SOLO setup in MP. */
    g_LevelMenuConfig.stages[21]=LEVELID_NONE; /* Random is movable. */
    g_LevelMenuConfig.stages[22]=LEVELID_FACILITY_MP;
    g_LevelMenuConfig.stages[23]=LEVELID_BUNKER2_MP;
    g_LevelMenuAssets.slots[4].flags=LEVEL_ASSET_BIT(LEVEL_ASSET_BRIEF_SOURCE);
    g_LevelMenuAssets.slots[4].briefingStage=LEVELID_FRIGATE;
    frontInitLevelMenu();assert(g_LevelMenuActive);
    assert(frontBriefingStage(campaignGetEntryByOrder(0))==LEVELID_FRIGATE);
    assert(frontBriefingStage(campaignGetEntryByOrder(2))==LEVELID_NONE);
    assert(frontBriefingStage(campaignGetEntryByOrder(4))==LEVELID_FRIGATE);
    assert(mission_folder_setup_entries[campaignGetEntryByOrder(4)].briefing_name_ptr==original[6].briefing_name_ptr);
    for(int i=0;i<20;i++) {
        struct mission_folder_setup *entry=&mission_folder_setup_entries[campaignGetEntryByOrder(i)];
        assert(entry->save_slot==original[i].save_slot && entry->photo_switch==original[i].photo_switch);
        assert(entry->is_bonus==original[i].is_bonus && entry->minimum_difficulty==original[i].minimum_difficulty);
        assert(!memcmp(entry->cheat_times,original[i].cheat_times,sizeof(entry->cheat_times)));
        assert(campaignGetOrderBySaveSlot(entry->save_slot)==i);
    }
    assert(mission_folder_setup_entries[campaignGetEntryByOrder(0)].briefing_name_ptr==original[6].briefing_name_ptr);
    assert(!mission_folder_setup_entries[campaignGetEntryByOrder(2)].briefing_name_ptr);
    assert(!strcmp(frontSoloLevelName(campaignGetEntryByOrder(0),1),"Frigate"));
    assert(!strcmp(frontLevelMenuName(21,1),"localized"));
    char *saved=lvFindLevelInfo(LEVELID_FRIGATE)->levelName;
    lvFindLevelInfo(LEVELID_FRIGATE)->levelName="12345678901234567890123456789012345678901234567890";
    assert(strlen(frontLevelMenuName(0,1))==31);lvFindLevelInfo(LEVELID_FRIGATE)->levelName=saved;
    assert(frontSelectSoloStage(campaignGetEntryByOrder(3))==LEVELID_FACILITY);
    assert(lvFindStageInfo(LEVELID_FACILITY,1)->levelID==LEVELID_FACILITY_MP);
    assert(frontSelectMultiStage(0)==LEVELID_FACILITY);
    assert(lvFindStageInfo(LEVELID_FACILITY,4)->levelID==LEVELID_FACILITY);
    assert(frontSelectMultiStage(2)==LEVELID_FACILITY);
    assert(lvFindStageInfo(LEVELID_FACILITY,4)->levelID==LEVELID_FACILITY_MP);
    assert(lvGetMemoryAllocationString(LEVELID_FACILITY,4)==lvFindLevelInfo(LEVELID_FACILITY_MP)->memoryAllocationString);
    assert(lvFindStageInfo(LEVELID_FRIGATE,4)->levelID==LEVELID_FRIGATE);
    assert(!check_if_mp_stage_unlocked(3));players=3;assert(check_if_mp_stage_unlocked(3));players=4;
    int count=0,seen[12]={0};
    for(int i=0;i<12;i++)if(multi_stage_setups[i].stage_id>=0 && check_if_mp_stage_unlocked(i))count++;
    for(randomValue=0;randomValue<(u32)count;randomValue++) {
        int stage=frontSelectMultiStage(1),exact=levelMenuGetLaunchStage(stage,4),found=0;
        for(int i=0;i<12;i++)if(!seen[i] && check_if_mp_stage_unlocked(i) && g_LevelMenuConfig.stages[20+i]==exact)
        {seen[i]=1;found=1;break;}
        assert(found);
    }
    assert(seen[0]&&seen[2]&&!seen[1]&&!seen[3]);
    for(int i=0;i<12;i++)multi_stage_setups[i].max_player=0;
    assert(!check_if_mp_stage_unlocked(1)&&frontSelectMultiStage(1)==LEVELID_NONE);
    levelMenuClearLaunch();assert(lvFindStageInfo(LEVELID_FACILITY,4)->levelID==LEVELID_FACILITY_MP);
    memcpy(multi_stage_setups,arenas,sizeof(arenas));g_LevelMenuActive=0;
    assert(frontSelectMultiStage(7)==LEVELID_FACILITY);
    assert(lvFindStageInfo(LEVELID_FACILITY,4)->levelID==LEVELID_FACILITY_MP);
    /* Independent names and malformed asset fallback. */
    g_LevelMenuActive=1;
    LevelMenuAsset *a=&g_LevelMenuAssets.slots[0];
    a->flags=LEVEL_ASSET_BIT(LEVEL_ASSET_MENU_NAME);strcpy(a->names[0],"Custom menu");
    assert(!strcmp(frontSoloLevelName(campaignGetEntryByOrder(0),1),"Custom menu"));
    assert(!strcmp(frontSoloPartName(campaignGetEntryByOrder(0),1),"Frigate"));
    assert(!strcmp(frontSoloMissionName(campaignGetEntryByOrder(0),1),"localized"));
    a->flags|=LEVEL_ASSET_BIT(LEVEL_ASSET_PART_NAME)|LEVEL_ASSET_BIT(LEVEL_ASSET_MISSION_NAME);
    strcpy(a->names[1],"Custom mission");strcpy(a->names[2],"Custom part");
    assert(!strcmp(frontSoloPartName(campaignGetEntryByOrder(0),1),"Custom part"));
    assert(!strcmp(frontSoloMissionName(campaignGetEntryByOrder(0),1),"Custom mission"));
    memset(a->names[0],'x',32);assert(!frontMenuAsset(0));
    assert(!strcmp(frontSoloLevelName(campaignGetEntryByOrder(0),1),"Frigate"));
    memset(a,0,sizeof(*a));
    /* Malformed input must leave both menus alone. */
    for(int i=0;i<20;i++)mission_folder_setup_entries[campaignGetEntryByOrder(i)]=original[i];
    g_LevelMenuInitialized=0;g_LevelMenuActive=0;g_LevelMenuConfig.stages[0]=LEVELID_TITLE;
    frontInitLevelMenu();assert(!g_LevelMenuActive);
    assert(mission_folder_setup_entries[campaignGetEntryByOrder(0)].stage_id==LEVELID_DAM);
    puts("PASS: campaign slot identity, duplicate levels, briefing source/bank routing, independent names, invalid asset fallback, labels, exact SP/MP resources and budgets, moved Random, player limits, empty pool and legacy fallback.");
}
'''
with tempfile.TemporaryDirectory(prefix='gud-level-menu-') as temp:
    work=Path(temp);(work/'check.c').write_text(source)
    subprocess.run([os.environ.get('CC','cc'),'-std=c99','-O1','-g','-Wall','-Wextra','-Werror',
        '-Wno-sign-compare','-fsanitize=address,undefined',f'-I{root / "src/game"}',str(work/'check.c'),
        str(root/'src/game/campaign.c'),'-o',str(work/'check')],check=True)
    subprocess.run([str(work/'check')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'))
