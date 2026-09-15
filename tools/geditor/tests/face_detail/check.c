#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bgdocument.h"

/* Only face lookup is stubbed; selection aggregation and decoding are the
 * production functions. Fixtures use two faces with stable references. */
const BgDocumentFace *BgDocumentFindFace(const BgDocument *document,
                                        const BgFaceRef *ref, const BgDocumentRoom **roomout)
{
    if (roomout) { *roomout = &document->rooms[1]; }
    return ref->faceid >= 1 && ref->faceid <= 2 ? &document->rooms[1].faces[ref->faceid - 1] : NULL;
}

#include "selection.inc"

int main(void)
{
    BgMaterial material, original;
    BgDetailTexture detail;
    BgDocumentFace faces[2] = {{0}};
    BgDocumentRoom rooms[2] = {0};
    BgDocument document = {0};
    BgFaceRef refs[2] = {{1,1,0,0}, {2,1,0,0}};
    FaceDetailSelection selection;
    char scale[24];
    const char *scales[] = {"1x", "1/2x", "1/4x", "1/8x", "1/16x", "1/32x",
        "1/64x", "1/128x", "1/256x", "1/512x", "1/1024x", "32x", "16x", "8x", "4x", "2x"};
    unsigned int i;

    BgMaterialInit(&material);
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_NONE && detail.textureid == BG_TEX_NONE);
    /* Separate image 0456, base 0123, U shift 3, V shift 14,
     * half-texel offset and the full eight-bit minimum LOD. */
    assert(BgMaterialReadCommand(&material, 0xC008F801u, 0x80456123u));
    original = material;
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_SEPARATE_IMAGE && detail.textureid == 0x456);
    assert(detail.shiftu == 3 && detail.shiftv == 14 && detail.offset == 2 && detail.minlod == 128);
    assert(memcmp(&material, &original, sizeof(material)) == 0);
    faces[0].material = faces[1].material = material;

    /* Type 0 must never mistake the unused second ID for another image. */
    material.textureword0 &= ~7u;
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_BASE_IMAGE && detail.textureid == 0x123);
    material.modeword0 &= ~255u;
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_NONE && detail.textureid == BG_TEX_NONE);
    material = original;
    for (i = 2; i < 8; i++)
    {
        material.textureword0 = (original.textureword0 & ~7u) | i;
        BgMaterialGetDetail(&material, &detail);
        assert(detail.mode == (i <= 4 ? BG_DETAIL_NONE : BG_DETAIL_UNKNOWN));
    }
    material = original;
    material.textureword1 = 0xFFFFFFFFu;
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_NONE); /* No base image. */
    material.textureword1 = 0xFFFFF123u;
    BgMaterialGetDetail(&material, &detail);
    assert(detail.mode == BG_DETAIL_SEPARATE_IMAGE && detail.textureid == BG_TEX_NONE);
    assert(detail.minlod == 255); /* Preserve invalid/missing detail ID for display. */
    for (i = 0; i < 16; i++)
    {
        FacePropertiesDetailScale(i, scale, sizeof(scale));
        assert(strcmp(scale, scales[i]) == 0);
    }
    FacePropertiesDetailScale(-1, scale, sizeof(scale));
    assert(strcmp(scale, "Mixed") == 0);

    document.rooms = rooms;
    document.roomcount = 1;
    rooms[1].faces = faces;
    rooms[1].facecount = 2;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.mode == BG_DETAIL_SEPARATE_IMAGE && selection.textureid == 0x456);
    assert(selection.shiftu == 3 && selection.shiftv == 14 && selection.minlod == 128 && selection.offset == 1);
    /* Different base textures may still share the same detail image. */
    faces[1].material.textureword1 = 0x80456789u;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.textureid == 0x456);
    /* One mixed property must not hide the others, or the shared preview. */
    faces[1].material.textureword0 ^= 1u << 14;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.shiftu == -1 && selection.shiftv == 14 && selection.textureid == 0x456);
    faces[1].material.textureword1 ^= 1u << 12;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.textureid == -1 && selection.minlod == 128);
    faces[1].material.modeword0 &= ~255u;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.mode == -1 && selection.textureid == -1 && selection.minlod == -1);
    faces[0].material.modeword0 &= ~255u;
    FacePropertiesGetDetail(&document, refs, 2, &selection);
    assert(selection.mode == BG_DETAIL_NONE);
    assert(!document.dirty);
    puts("PASS: native detail decoding, LOD base-image reuse, UV shifts, missing images and mixed selections.");
    return 0;
}
