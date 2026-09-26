#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef float f32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
#define bool int
#include "types.inc"
#include "../../../../src/levelids.h"
static EnvironmentRecord *g_MainEnvironment,*g_AlternateEnvironment,active;
static int players=1,loads;
static int getPlayerCount(void) { return players; }
static void envLoadCurrentEnvironment(EnvironmentRecord *r) { active=*r;loads++; }
static EnvironmentRecord g_EnvTable[]={
    {.Id=1,.FogEnabled=0,.Visibility={.NearClipDistance=10,.FarClipDistance=1000}},
    {.Id=101,.FogEnabled=0,.Visibility={.NearClipDistance=30,.FarClipDistance=3000}},
    {.Id=201,.FogEnabled=1,.Visibility={.NearClipDistance=5,.FarClipDistance=1500}},
    {.Id=3,.FogEnabled=0,.Visibility={.NearClipDistance=15,.FarClipDistance=10000}},
    {.Id=(u32)-1,.FogEnabled=0,.Visibility={.NearClipDistance=15,.FarClipDistance=20000}},
    {0}
};
#include "engine.inc"
int main(void)
{
    envSwitchToSoloSky2(1);assert(!loads);
    envLoadLevelEnvironment(1,0);assert(loads==1&&!active.FogEnabled);
    envSwitchToSoloSky2(.5f);assert(loads==2&&!active.FogEnabled&&active.Visibility.NearClipDistance==20&&active.Visibility.FarClipDistance==2000);
    g_EnvTable[0].FogEnabled=1;g_EnvTable[0].Visibility.FogStart=996;g_EnvTable[0].Visibility.FogEnd=1000;
    envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(.99f);assert(active.FogEnabled&&active.Visibility.FogEnd-active.Visibility.FogStart==4);
    envSwitchToSoloSky2(1);assert(!active.FogEnabled&&active.Visibility.FogEnd==0);
    g_EnvTable[0].FogEnabled=0;g_EnvTable[1].FogEnabled=1;g_EnvTable[1].Visibility.FogStart=995;g_EnvTable[1].Visibility.FogEnd=1000;
    envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(1);assert(active.FogEnabled&&active.Visibility.FogStart==995&&active.Visibility.FogEnd==1000);
    envLoadLevelEnvironment(3,0);envSwitchToSoloSky2(1);assert(active.Id==3&&active.Visibility.FarClipDistance==10000);
    players=2;envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(1);assert(active.Id==201&&active.Visibility.FarClipDistance==1500);
    players=1;envLoadLevelEnvironment(99,0);envSwitchToSoloSky2(1);assert(active.Id==(u32)-1&&active.Visibility.FarClipDistance==20000);
    puts("PASS: scripted transitions with fog disabled, enabling/disabling fog at transition endpoints, no alternate, MP/fallback rows and calls before initialization.");
    return 0;
}
