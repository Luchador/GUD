#include "../doorshadowformat.h"

static int doorShadowClip(const DoorShadowPoint input[3], int axis, float edge,
    int lower, DoorShadowPoint output[4])
{
    int i, count = 0;
    for (i = 0; i < 3; i++)
    {
        const DoorShadowPoint *a = &input[i], *b = &input[(i + 1) % 3];
        float da = a->position[axis] - edge, db = b->position[axis] - edge;
        int ina = lower ? da <= 0 : da >= 0;
        int inb = lower ? db <= 0 : db >= 0;
        if (ina) { output[count++] = *a; }
        if (ina != inb)
        {
            DoorShadowPoint *p = &output[count++];
            float t = da / (da - db);
            int j;
            for (j = 0; j < 3; j++) p->position[j] = a->position[j] + t * (b->position[j] - a->position[j]);
            for (j = 0; j < 2; j++) p->uv[j] = a->uv[j] + t * (b->uv[j] - a->uv[j]);
            p->position[axis] = edge;
            p->alpha = a->alpha + t * (b->alpha - a->alpha);
            p->rgb = 0;
        }
    }
    return count;
}

void doorShadowSplit(const DoorShadowPoint source[6], unsigned int direction,
    float opening, unsigned int light, unsigned int dark,
    DoorShadowPoint output[DOOR_SHADOW_OUTPUT_VERTICES])
{
    int axis = direction < 2 ? 0 : 2;
    int reverse = direction & 1;
    float low = source[0].position[axis], high = low, edge;
    int i, triangle, side;
    if (!(opening >= 0)) opening = 0;
    if (opening > 1) opening = 1;
    for (i = 1; i < 6; i++)
    {
        if (source[i].position[axis] < low) low = source[i].position[axis];
        if (source[i].position[axis] > high) high = source[i].position[axis];
    }
    edge = reverse ? high - opening * (high - low) : low + opening * (high - low);
    for (triangle = 0; triangle < 2; triangle++)
    {
        int used = 0;
        DoorShadowPoint *out = output + triangle * 9;
        for (i = 0; i < 9; i++) { out[i] = source[triangle * 3]; out[i].rgb = dark; }
        for (side = 0; side < 2; side++)
        {
            DoorShadowPoint polygon[4];
            int count;
            if ((side == 0 && opening == 0) || (side == 1 && opening == 1)) continue;
            count = doorShadowClip(source + triangle * 3, axis, edge, side == reverse, polygon);
            for (i = 1; i + 1 < count && used <= 6; i++)
            {
                out[used] = polygon[0]; out[used + 1] = polygon[i]; out[used + 2] = polygon[i + 1];
                out[used].rgb = out[used + 1].rgb = out[used + 2].rgb = side ? dark : light;
                used += 3;
            }
        }
    }
}
