#ifndef GEDITOR_STUDIOOUTPUT_H
#define GEDITOR_STUDIOOUTPUT_H
#include "studioscene.h"
#include "texload.h"

/* Display-oriented, bottom-up RGBA pixels (as returned by OpenGL). Writes a
 * BMP V4 with explicit alpha, including when every pixel is transparent.
 * Publishes the lowest unused render0001.bmp...render9999.bmp atomically. */
BOOL StudioOutputSave(const char *project,const TexPixel *pixels,const StudioRenderSettings *settings,
    char path[MAX_PATH],const char **why);
#endif
