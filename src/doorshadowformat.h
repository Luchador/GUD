#ifndef GUD_DOOR_SHADOW_FORMAT_H
#define GUD_DOOR_SHADOW_FORMAT_H

/* A fixed-size, pointer-free setup command. The first 128 bytes retain the
 * ObjectRecord layout for stable editor selection/deletion; no gameplay prop
 * is allocated. All words and native Vtx records are big endian on disk. */
#define DOOR_SHADOW_VERSION 1u
#define DOOR_SHADOW_MANIFEST_KIND 0x44534844u /* DSHD */
#define DOOR_SHADOW_BYTES 1024u
#define DOOR_SHADOW_MAX 64
#define DOOR_SHADOW_ORIGIN 16u /* Three room-origin f32 values. */
#define DOOR_SHADOW_ROOM 128u
#define DOOR_SHADOW_DOOR 132u /* Absolute setup command index; -1 = unlinked. */
#define DOOR_SHADOW_DIRECTION 136u
#define DOOR_SHADOW_LIGHT 140u /* 0x00RRGGBB */
#define DOOR_SHADOW_DARK 144u
#define DOOR_SHADOW_GDL_SIZE 148u
#define DOOR_SHADOW_LAYER 152u
#define DOOR_SHADOW_FORMAT 156u
#define DOOR_SHADOW_VERTICES 160u /* Original two triangles, six native Vtx. */
#define DOOR_SHADOW_GDL 256u
#define DOOR_SHADOW_GDL_CAPACITY (DOOR_SHADOW_BYTES - DOOR_SHADOW_GDL)
#define DOOR_SHADOW_POS_X 0u
#define DOOR_SHADOW_NEG_X 1u
#define DOOR_SHADOW_POS_Z 2u
#define DOOR_SHADOW_NEG_Z 3u
#define DOOR_SHADOW_OUTPUT_VERTICES 18

typedef struct DoorShadowPoint {
    float position[3];
    float uv[2]; /* Native 10.5 texture coordinates. */
    float alpha;
    unsigned int rgb;
} DoorShadowPoint;

/* Clip each original triangle independently, retaining its affine UV mapping
 * even when the two triangles have different UV gradients. Always fills two
 * nine-vertex batches; unused triangles are degenerate. */
void doorShadowSplit(const DoorShadowPoint source[6], unsigned int direction,
    float opening, unsigned int light, unsigned int dark,
    DoorShadowPoint output[DOOR_SHADOW_OUTPUT_VERTICES]);

#endif
