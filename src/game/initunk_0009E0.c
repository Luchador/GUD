#include <ultra64.h>
#include "objecthandler.h"
#include "model.h"


void initAnimationsBuffer(struct ModelAnimationScratch *animBuffer)
{
    g_ModelAnimationScratch = animBuffer;
    modelResetAnimationFrameCache();
}
