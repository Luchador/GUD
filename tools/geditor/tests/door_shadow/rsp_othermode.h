#include <stdint.h>

/* Model the scalar mask sequence in rsp/graphics/gmain.s, lbl_1264.
 * The RSP stores the entire EF command word, including its opcode. Its
 * variable shifts use only five bits, and the payload is ORed unmasked.
 * The editor/converter's logical state readers do not model those details. */
static uint32_t shadowRspOtherMode(uint32_t previous, uint32_t w0, uint32_t w1)
{
    uint32_t length = w0 & 31u, shift = (w0 >> 8) & 31u;
    uint32_t mask = ((1u << length) - 1u) << shift;
    return (previous & ~mask) | w1;
}
