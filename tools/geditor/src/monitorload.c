#include "monitorload.h"
#include <stdlib.h>
#include <string.h>

static DWORD Read32(const unsigned char *p)
{
    return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3];
}

static const RomManifestEntry *Entry(const RomFile *rom, DWORD kind)
{
    DWORD i;
    const RomManifestEntry *entry = NULL;
    if (!rom || !rom->data || rom->info.entrycount > ROM_MAX_ENTRIES) { return NULL; }
    for (i = 0; i < rom->info.entrycount; i++)
    {
        const RomManifestEntry *e = &rom->info.entries[i];
        if (e->kind != kind) { continue; }
        if (entry || e->romstart >= e->romend || e->romend > rom->size) { return NULL; }
        entry = e;
    }
    return entry;
}

BOOL MonitorBankLoadRom(MonitorBank *out, const RomFile *rom, const char **reason)
{
    const RomManifestEntry *code = Entry(rom, 0x434d4150), *scripts = Entry(rom, 0x4d4f4e41);
    const RomManifestEntry *images = Entry(rom, 0x4d4f4e54), *initial = Entry(rom, 0x4d4f4e44);
    DWORD i;
    ZeroMemory(out, sizeof(*out));
    *reason = "Invalid monitor animation data in the ROM. Rebuild GUD with current sources.";
    if (!code || !scripts || !images || !initial || !scripts->flags || scripts->flags > 256
        || scripts->romend - scripts->romstart != scripts->flags * 4
        || images->flags != 12 || (images->romend - images->romstart) % 12
        || images->romend - images->romstart > 100 * 12
        || initial->flags != 116 || initial->romend - initial->romstart != 116) { return FALSE; }
    out->codesize = code->romend - code->romstart;
    out->address = code->flags;
    out->scriptcount = scripts->flags;
    out->imagecount = (images->romend - images->romstart) / 12;
    if (!out->imagecount || out->address > UINT32_MAX - out->codesize
        || !MonitorAnimationDecode(&out->initial, rom->data + initial->romstart, 116)) { goto fail; }
    out->code = malloc(out->codesize);
    out->images = malloc(out->imagecount * 12);
    out->scripts = malloc(out->scriptcount * sizeof(*out->scripts));
    if (!out->code || !out->images || !out->scripts)
    { *reason = "Out of memory loading monitor animations."; goto fail; }
    memcpy(out->code, rom->data + code->romstart, out->codesize);
    memcpy(out->images, rom->data + images->romstart, out->imagecount * 12);
    for (i = 0; i < out->scriptcount; i++)
    {
        DWORD ptr = Read32(rom->data + scripts->romstart + i * 4);
        if ((ptr & 3) || ptr < out->address || ptr - out->address >= out->codesize) { goto fail; }
        out->scripts[i] = ptr;
    }
    for (i = 0; i < out->imagecount; i++)
    {
        const unsigned char *image = out->images + i * 12;
        if (!image[4] || !image[5] || Read32(image) > 0xffff) { goto fail; }
    }
    *reason = "";
    return TRUE;
fail:
    MonitorBankFree(out);
    return FALSE;
}

void MonitorGeometryFree(MonitorGeometry *geometry)
{
    MonitorBankFree(&geometry->bank);
    free(geometry->surfaces);
    ZeroMemory(geometry, sizeof(*geometry));
}
