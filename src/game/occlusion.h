#ifndef GUD_OCCLUSION_H
#define GUD_OCCLUSION_H
#include <occluderformat.h>
struct BoundPadRecord;
struct ModelHitEntry;
void occlusionReset(void);
void occlusionLoad(struct BoundPadRecord *pads);
void occlusionBeginView(void);
int occlusionTestSphere(const float position[3], float radius);
/* Called after posing, before float matrices are converted for the RSP. */
int occlusionTestCharacter(struct ModelHitEntry *entries);
void occlusionToggle(void);
int occlusionEnabled(void);
int occlusionCount(void);
int occlusionRejected(void);
#endif
