#include <ultra64.h>


/**
 * Decode RLE-compressed 8-bit image data into a raw byte buffer.
 */
void rle_expand_8bit(u8 *src, u8 *dst)
{
    u8 *in;
    s32 remaining;
    s32 count;
    s32 more;
    u16 w, h;
    u8 value;

    w = *(u16 *)&src[0];
    h = *(u16 *)&src[2];
    remaining = w * h;
    in = &src[10];

    do {
        count = *in++;
        value = *in++;
        remaining -= count;
        count--;

        do {
            more = 0 < count;
            count--;
            *dst++ = value;
        } while (more);
    } while (remaining > 0);
}
