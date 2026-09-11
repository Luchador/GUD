#ifndef GEDITOR_MONITORANIM_H
#define GEDITOR_MONITORANIM_H

#include <stddef.h>
#include <stdint.h>

/* Host state only. No runtime state is written into project/setup files. */
typedef struct MonitorTween {
    float value, fraction, increment, old, target;
} MonitorTween;

typedef struct MonitorAnimation {
    uint32_t script, offset, texture, random;
    int pause, stopped;
    float rotation;
    MonitorTween xscale, yscale, xmid, ymid;
    unsigned char color[4], oldcolor[4], targetcolor[4];
    float colorfraction, colorincrement;
} MonitorAnimation;

typedef struct MonitorBank {
    unsigned char *code, *images;
    uint32_t codesize, address, imagecount, scriptcount;
    uint32_t *scripts;
    MonitorAnimation initial;
} MonitorBank;

void MonitorBankFree(MonitorBank *bank);
int MonitorBankClone(MonitorBank *out, const MonitorBank *source);
int MonitorAnimationDecode(MonitorAnimation *out, const unsigned char *data, size_t size);
void MonitorAnimationStart(MonitorAnimation *out, const MonitorBank *bank, int id, uint32_t seed);
/* One NTSC 60-Hz tick, independent of viewport frame rate. Malformed scripts
   stop locally; the instruction budget also contains zero-time jump loops. */
void MonitorAnimationTick(MonitorAnimation *state, const MonitorBank *bank);
const unsigned char *MonitorAnimationImage(const MonitorAnimation *state, const MonitorBank *bank);
void MonitorAnimationUvs(const MonitorAnimation *state, float uv[4][2]);

#endif
