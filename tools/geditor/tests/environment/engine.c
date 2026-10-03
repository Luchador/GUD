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
typedef struct { f32 x,y,z; } coord3d;
#include "types.inc"
#include "../../../../src/levelids.h"
static EnvironmentRecord *g_MainEnvironment,*g_AlternateEnvironment,active;
static f32 g_SkyBodyAlpha = 255.0f;
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
static void SkyBodyFades(void)
{
    /* Egyptian's fade runs for 120 ticks. Exercise either body, with or
     * without fog, using the same repeated progress calls as the gas timer. */
    for (int fog=0;fog<=1;fog++) for (u32 type=1;type<=2;type++)
    {
        f32 previous=256;
        g_EnvTable[0].FogEnabled=g_EnvTable[1].FogEnabled=fog;
        g_EnvTable[0].SkyBody=(SkyBodySettings){type,5,255,210,160,0,{0,1,-1}};
        g_EnvTable[1].SkyBody=(SkyBodySettings){0};
        envLoadLevelEnvironment(1,0);assert(envGetSkyBodyAlpha()==255);
        for (int tick=0;tick<=120;tick++)
        {
            envSwitchToSoloSky2(tick/120.0f);
            assert(envGetSkyBodyAlpha()<previous&&envGetSkyBodyAlpha()>=0);
            previous=envGetSkyBodyAlpha();
            if (tick==60) { assert(previous==127.5f); }
            if (tick<120)
            {
                assert(active.SkyBody.Type==type&&active.SkyBody.AngularSize==5);
                assert(active.SkyBody.Red==255&&active.SkyBody.Green==210&&active.SkyBody.Blue==160);
                assert(active.SkyBody.Direction.y==1&&active.SkyBody.Direction.z==-1);
            }
        }
        assert(active.SkyBody.Type==0&&envGetSkyBodyAlpha()==0);
        envSwitchToSoloSky2(1);assert(envGetSkyBodyAlpha()==0);
        envSwitchToSoloSky2(.5f);envSwitchToSoloSky2(.5f);assert(envGetSkyBodyAlpha()==127.5f);
        envSwitchToSoloSky2(-1);assert(active.SkyBody.Type==type&&envGetSkyBodyAlpha()==255);
        envSwitchToSoloSky2(2);assert(active.SkyBody.Type==0&&envGetSkyBodyAlpha()==0);
        envLoadLevelEnvironment(1,0);assert(active.SkyBody.Type==type&&envGetSkyBodyAlpha()==255);
        envSwitchToSoloSky2(1);assert(active.SkyBody.Type==0&&envGetSkyBodyAlpha()==0); /* SwitchSky */

        /* Reverse transition uses the alternate's image/tint from the start. */
        g_EnvTable[1].SkyBody=g_EnvTable[0].SkyBody;
        g_EnvTable[0].SkyBody.Type=0;
        envLoadLevelEnvironment(1,0);assert(envGetSkyBodyAlpha()==0);
        envSwitchToSoloSky2(0);assert(envGetSkyBodyAlpha()==0);
        envSwitchToSoloSky2(.5f);assert(active.SkyBody.Type==type&&active.SkyBody.Green==210&&envGetSkyBodyAlpha()==127.5f);
        envSwitchToSoloSky2(1);assert(active.SkyBody.Type==type&&envGetSkyBodyAlpha()==255);
        envLoadLevelEnvironment(1,0);assert(envGetSkyBodyAlpha()==0);
    }
    envLoadLevelEnvironment(3,0);envSwitchToSoloSky2(.5f);assert(envGetSkyBodyAlpha()==0);
    g_EnvTable[3].SkyBody.Type=1;
    envLoadLevelEnvironment(3,0);envSwitchToSoloSky2(1);
    assert(active.SkyBody.Type==1&&envGetSkyBodyAlpha()==255); /* no alternate */
}
int main(void)
{
    g_EnvTable[0].SkyBody.Type=1;g_EnvTable[0].SkyBody.AngularSize=5;
    g_EnvTable[1].SkyBody.Type=2;g_EnvTable[1].SkyBody.AngularSize=8;
    envSwitchToSoloSky2(1);assert(!loads);
    envLoadLevelEnvironment(1,0);assert(loads==1&&!active.FogEnabled&&active.SkyBody.Type==1);
    envSwitchToSoloSky2(.5f);assert(loads==2&&!active.FogEnabled&&active.Visibility.NearClipDistance==20&&active.Visibility.FarClipDistance==2000&&active.SkyBody.Type==1&&envGetSkyBodyAlpha()==255);
    envSwitchToSoloSky2(1);assert(active.SkyBody.Type==2&&active.SkyBody.AngularSize==8&&envGetSkyBodyAlpha()==255);
    g_EnvTable[0].FogEnabled=1;g_EnvTable[0].Visibility.FogStart=996;g_EnvTable[0].Visibility.FogEnd=1000;
    envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(.99f);assert(active.FogEnabled&&active.Visibility.FogEnd-active.Visibility.FogStart==4);
    envSwitchToSoloSky2(1);assert(!active.FogEnabled&&active.Visibility.FogEnd==0);
    g_EnvTable[0].FogEnabled=0;g_EnvTable[1].FogEnabled=1;g_EnvTable[1].Visibility.FogStart=995;g_EnvTable[1].Visibility.FogEnd=1000;
    envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(1);assert(active.FogEnabled&&active.Visibility.FogStart==995&&active.Visibility.FogEnd==1000);
    envLoadLevelEnvironment(3,0);envSwitchToSoloSky2(1);assert(active.Id==3&&active.Visibility.FarClipDistance==10000);
    players=2;envLoadLevelEnvironment(1,0);envSwitchToSoloSky2(1);assert(active.Id==201&&active.Visibility.FarClipDistance==1500);
    players=1;envLoadLevelEnvironment(99,0);envSwitchToSoloSky2(1);assert(active.Id==(u32)-1&&active.Visibility.FarClipDistance==20000);
    SkyBodyFades();
    puts("PASS: Sun/Moon transparency follows the 120-tick environment fade, reverse fades, instant switches, repeat calls and level reloads.");
    puts("PASS: scripted transitions with fog disabled, enabling/disabling fog at transition endpoints, no alternate, MP/fallback rows and calls before initialization.");
    return 0;
}
