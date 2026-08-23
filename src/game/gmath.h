#ifndef GMATH_H
#define GMATH_H

#include <ultra64.h>
#include "bondtypes.h"

f32 acosf(f32 cosinef);
f32 asinf(f32 sinef);

void lerp_rgba_s32_with_rgba_f32(rgba_s32 *arg0, s32 arg1, rgba_f32 *arg2);

#endif
