#ifndef GUD_OCCLUSION_H
#define GUD_OCCLUSION_H
#include <occluderformat.h>
struct BoundPadRecord;
void occlusionReset(void);
void occlusionLoad(struct BoundPadRecord *pads);
void occlusionBeginView(void);
int occlusionTestSphere(const float position[3], float radius);
void occlusionToggle(void);
int occlusionEnabled(void);
int occlusionCount(void);
int occlusionRejected(void);
#endif
