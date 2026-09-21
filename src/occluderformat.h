#ifndef GUD_OCCLUDER_FORMAT_H
#define GUD_OCCLUDER_FORMAT_H

/* A dedicated BoundPadRecord, never used for gameplay placement. The stan
 * pointer slot is a tag on disk and is cleared when the runtime loads it.
 * plink still points to an empty string; native pad indices remain stable. */
#define OCCLUDER_PAD_TAG 0x47454f43u /* GEOC */
#define OCCLUDER_MANIFEST_KIND 0x4f43434cu /* OCCL */
#define OCCLUDER_VERSION 1u
#define OCCLUDER_MAX 32

#endif
