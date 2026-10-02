#ifndef GLASS_OPACITY_FORMAT_H
#define GLASS_OPACITY_FORMAT_H

/* Optional regular-glass opacity in a setup-only ObjectRecord matrix word.
 * objInit reads it before placement overwrites the matrix. Native records and
 * pointer slots keep their original sizes/values. Independent of GFD1 fades.
 * The low byte replaces vertex alpha; texture alpha still masks the surface.
 * Untagged glass retains the model's authored alpha. */
#define GLASS_OPACITY_OFFSET 0x30u
#define GLASS_OPACITY_TAG 0x474f5000u /* GOP + alpha */
#define GLASS_OPACITY_TAG_MASK 0xffffff00u
#define GLASS_OPACITY_MANIFEST_KIND 0x474f5041u /* GOPA */
#define GLASS_OPACITY_VERSION 1u

#endif
