#ifndef GUD_OCCLUSION_MATH_H
#define GUD_OCCLUSION_MATH_H

/* Portable geometry shared with the editor's export validation and tests.
 * Each camera-facing box face defines a separate five-plane shadow. Testing
 * a complete sphere inside any one shadow is conservative, even at corners. */
typedef struct OcclusionBox {
    float corners[8][3];
    float shadows[3][5][4];
    int count;
} OcclusionBox;

int occlusionBoxInit(OcclusionBox *box, const float pos[3], const float up[3],
    const float look[3], const float bounds[6]);
/* Positive nearplane distance is in front of the actual near clip plane. */
void occlusionBoxView(OcclusionBox *box, const float eye[3], const float nearplane[4]);
int occlusionBoxContainsSphere(const OcclusionBox *box, const float pos[3], float radius);

#endif
