#ifndef _CAM_H_
#define _CAM_H_

#include <ultra64.h>
#include "bondtypes.h"

void camSetPlayerScreenSize(f32 width, f32 height);
void camSetPlayerScreenPosition(f32 left, f32 top);
void camSetPlayerPerspective(f32 near, f32 fovy, f32 aspect);
void camSetPlayerCameraScale(void);
void camSetPlayerProjMtx(Mtx *matrix);
Mtx *camGetPlayerProjMtx(void);
void camSetPlayerProjViewMtx(Mtx *mtx);
Mtx *camGetPlayerProjViewMtx(void);
void camUpdateFrustumPlanes(void);
bool camIsPosInScreen(coord3d *pos, f32 margin);
bool camIsPosInScreenBox(coord3d *pos, f32 margin, bbox2d *box);
void camSetPlayerFrozenCam(bool isFrozen);

#endif
