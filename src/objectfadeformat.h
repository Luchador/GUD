#ifndef GUD_OBJECT_FADE_FORMAT_H
#define GUD_OBJECT_FADE_FORMAT_H

/* Optional authored data in two ObjectRecord matrix words, before objInit
 * and placement replace the matrix. No native record grows. Do not use the
 * prop/model pointer slots: skipped setup records must retain null pointers.
 * 0x28: tag; 0x2c: start/end camera distances, unsigned 16-bit centimetres.
 * The runtime retains these in the formerly unused PropRecord word at 0x30.
 * Untagged records use environment screen-size fading. */
#define OBJECT_FADE_TAG 0x47464431u /* GFD1 */
#define OBJECT_FADE_TAG_OFFSET 0x28u
#define OBJECT_FADE_DISTANCES_OFFSET 0x2cu
#define OBJECT_FADE_MAX_CM 65535u
#define OBJECT_FADE_MANIFEST_KIND 0x4f464144u /* OFAD */
#define OBJECT_FADE_VERSION 1u

#endif
