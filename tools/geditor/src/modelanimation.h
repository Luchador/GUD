#ifndef GEDITOR_MODELANIMATION_H
#define GEDITOR_MODELANIMATION_H
#include "modelload.h"

typedef struct ModelAnimationClip {
    const char *name;
    DWORD header, frames, channels, width, framebytes, dataoffset;
} ModelAnimationClip;
typedef struct ModelAnimationPreview {
    unsigned char *headers, *frames, *model;
    DWORD headersize, framesize, modelsize;
    ModelAnimationClip *clips;
    DWORD count;
    int channels;
} ModelAnimationPreview;

/* No matching native skeleton/animation is a successful empty result.
 * All bytes are owned snapshots; playback never changes project data. */
BOOL ModelAnimationOpen(ModelAnimationPreview *preview, const char *project,
    const char *name, const char **why);
void ModelAnimationClose(ModelAnimationPreview *preview);
/* Return a newly allocated, complete source-order pose. Caller frees it. */
BgVertex *ModelAnimationPose(const ModelAnimationPreview *preview, DWORD clip,
    double frame, DWORD expectedcount, const char **why);
/* Bounded decoder shared by preview and native-frame regression checks. */
BOOL ModelAnimationReadFrame(const ModelAnimationPreview *preview,
    const ModelAnimationClip *clip, double frame, unsigned short angles[45], float *height);
#endif
