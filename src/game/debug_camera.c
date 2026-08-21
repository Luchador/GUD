#include <ultra64.h>
#include <bondtypes.h>
#include "bondview.h"
#include "player.h"
#include "gun.h"
#include "math.h"
#include "fr.h"

#ifndef DEBUG
    #define osSyncPrintf()
#endif

// bss
//CODE.bss:80079E20
s32 debugCameraStateCounter;


// data
//D:80037010
s32 D_80037010 = 0; //unused

// 0x80037014
coord3d debugCameraPosition = {0.0f,0.0f,0.0f};

coord3d debugCameraForward = {0.0f,0.0f,-1.0f};

//D:8003702C
coord3d debugCameraUp = {0.0f,1.0f,0.0f};

//D:80037038
f32 debugCameraHorizontalAngle = 0.0f;
//D:8003703C
f32 debugCameraHorizontalCosine = 1.0f;
//D:80037040
f32 debugCameraHorizontalSine = 0.0f;
//D:80037044
f32 debugCameraVerticalAngle = 0.0f;
//D:80037048
f32 debugCameraVerticalCosine = 1.0f;
//D:8003704C
f32 debugCameraVerticalSine = 0.0f;
//D:80037058
f32 debugCameraScale = 1.0f;
//D:8003705C
f32 debugCameraInverseScale = 1.0f;
//D:80037060
coord3d debugCameraPreviousPosition = {0.0f,0.0f,0.0f};


/*
* Address: 0x7f091618
*/
void initializeDebugCameraPosition(void)
{
    coord3d *pos = bondviewGetCurrentPlayersPosition();
    f32      r   = M_TAU_F - bondviewGetPlayerYawRadians();
    f32      x   = ((debugCameraPosition.x - pos->x) * cosf(r)) + ((debugCameraPosition.x - pos->x) * sinf(r));
    f32      y   = debugCameraPosition.y - pos->y;
    f32      z   = (debugCameraPosition.z - pos->z) * cosf(r) - (debugCameraPosition.x - pos->x) * sinf(r);
    osSyncPrintf("propBondIntroKey(INTROKEY_RELBONDDIR,%ff,%ff,%ff,0.75f,40.0f) ", x, y, z);
}


/*
* Address: 0x7f09166c
*/
void updateDebugCameraWorldPosition(void)
{
    f32 dist = sqrtf((debugCameraPosition.x - debugCameraPreviousPosition.x) * (debugCameraPosition.x - debugCameraPreviousPosition.x) +
                     (debugCameraPosition.y - debugCameraPreviousPosition.y) * (debugCameraPosition.y - debugCameraPreviousPosition.y) +
                     (debugCameraPosition.z - debugCameraPreviousPosition.z) * (debugCameraPosition.z - debugCameraPreviousPosition.z));

    osSyncPrintf("world pos = %f,%f,%f\n", debugCameraPosition.x, debugCameraPosition.y, debugCameraPosition.z);
    osSyncPrintf("world theta = %f verta = %f\n", (debugCameraPreviousPosition.x * 6.283185) / 360.0, (debugCameraPreviousPosition.z * 6.283185) / 360.0);
    osSyncPrintf("dist from prev = %f\n",dist);

    debugCameraPreviousPosition.x = debugCameraPosition.x;
    debugCameraPreviousPosition.y = debugCameraPosition.y;
    debugCameraPreviousPosition.z = debugCameraPosition.z;
}

/*
* Address: 0x7F0916F4
*/
void resetDebugCameraToPlayerPosition(void)
{
    coord3d *pos = bondviewGetCurrentPlayersPosition();
    debugCameraPosition.x = pos->x;
    debugCameraPosition.y = pos->y;
    debugCameraPosition.z = pos->z;
    debugCameraStateCounter = 0;
}



#undef osSyncPrintf
