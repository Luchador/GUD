#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sfxcatalog.h"

enum { CTL = 0x100, CTL_SIZE = 0x2000, TBL = CTL + CTL_SIZE, TBL_SIZE = 0x1000,
       BANK = 8, INST = 24, SOUND = 0x1000, WAVE = SOUND + 16 };
static void put16(unsigned char *p, DWORD n) { p[0] = n >> 8; p[1] = n; }
static void put32(unsigned char *p, DWORD n)
{ p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n; }
static DWORD get32(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }

static void fixture(RomFile *rom, DWORD count)
{
    memset(rom, 0, sizeof(*rom));
    rom->size = TBL + TBL_SIZE;
    rom->data = calloc(1, rom->size);
    assert(rom->data);
    rom->info.entrycount = 2;
    rom->info.entries[0] = (RomManifestEntry){0x53465843, CTL, CTL + CTL_SIZE, 0};
    rom->info.entries[1] = (RomManifestEntry){0x53465854, TBL, TBL + TBL_SIZE, 0};
    unsigned char *c = rom->data + CTL;
    put16(c, 0x4231); put16(c + 2, 1); put32(c + 4, BANK);
    put16(c + BANK, 1); put32(c + BANK + 4, 22050); put32(c + BANK + 12, INST);
    put16(c + INST + 14, count);
    for (DWORD i = 0; i < count; i++) { put32(c + INST + 16 + i * 4, SOUND); }
    put32(c + SOUND + 8, WAVE);
    put32(c + WAVE, 12); put32(c + WAVE + 4, 90);
}

static void readRealRom(const char *path)
{
    RomFile rom = {0}; SfxCatalog catalog = {0}; const char *why = "";
    FILE *f = fopen(path, "rb"); assert(f);
    assert(!fseek(f, 0, SEEK_END)); long size = ftell(f);
    assert(size > 0 && size <= 64 * 1024 * 1024); rewind(f);
    rom.size = (DWORD)size; rom.data = malloc(rom.size); assert(rom.data);
    assert(fread(rom.data, 1, rom.size, f) == rom.size); fclose(f);
    DWORD m;
    for (m = 0; m + 24 <= rom.size; m += 4)
        if (!memcmp(rom.data + m, "GUDGEDITORMANIF", 16)) { break; }
    assert(m + 24 <= rom.size);
    rom.info.entrycount = get32(rom.data + m + 20);
    assert(rom.info.entrycount <= ROM_MAX_ENTRIES && m + 24 + rom.info.entrycount * 16 <= rom.size);
    for (DWORD i = 0; i < rom.info.entrycount; i++)
    {
        unsigned char *e = rom.data + m + 24 + i * 16;
        rom.info.entries[i] = (RomManifestEntry){get32(e), get32(e + 4), get32(e + 8), get32(e + 12)};
    }
    assert(SfxCatalogReadRom(&rom, &catalog, &why));
    assert(catalog.count > 0 && catalog.entries[0].id == 1);
    for (DWORD i = 0; i < catalog.count; i++)
    {
        assert(catalog.entries[i].id == i + 1);
        assert(catalog.entries[i].sampleOffset <= rom.size);
        assert(catalog.entries[i].sampleBytes <= rom.size - catalog.entries[i].sampleOffset);
    }
    printf("PASS: real ROM contains %u indexed sound effects (%s through %s).\n",
           catalog.count, SfxCatalogName(1), SfxCatalogName(catalog.count));
    SfxCatalogFree(&catalog); free(rom.data);
}

int main(int argc, char **argv)
{
    RomFile rom; SfxCatalog catalog = {0}; const char *why = "";
    fixture(&rom, 300); /* More sounds than the compiled name table, sharing one sample. */
    assert(SfxCatalogReadRom(&rom, &catalog, &why));
    assert(!why[0] && catalog.count == 300);
    assert(catalog.entries[0].id == 1 && catalog.entries[299].id == 300);
    assert(catalog.entries[0].sampleOffset == TBL + 12 && catalog.entries[0].sampleBytes == 90);
    assert(!strcmp(SfxCatalogName(1), "Rocket launch"));
    assert(!strcmp(SfxCatalogName(261), "Big clank"));
    assert(!strcmp(SfxCatalogName(300), "Unnamed sound effect"));
    assert(SfxCatalogMatches(&catalog.entries[0], "ROCKET"));
    assert(SfxCatalogMatches(&catalog.entries[0], "0X0001"));
    assert(SfxCatalogMatches(&catalog.entries[260], "261"));
    assert(SfxCatalogMatches(&catalog.entries[0], ""));
    assert(!SfxCatalogMatches(&catalog.entries[0], "camera"));
    free(rom.data); /* Catalog metadata survives releasing the source ROM. */
    assert(catalog.entries[299].sampleBytes == 90);

    fixture(&rom, 1);
    rom.data[CTL + WAVE + 8] = 1;
    assert(SfxCatalogReadRom(&rom, &catalog, &why) && catalog.count == 1 && catalog.entries[0].format == 1);
    rom.data[CTL + WAVE + 8] = 7; /* Listing does not require an audio decoder. */
    assert(SfxCatalogReadRom(&rom, &catalog, &why) && catalog.entries[0].format == 7);
    put16(rom.data + CTL + INST + 14, 0);
    assert(SfxCatalogReadRom(&rom, &catalog, &why) && !catalog.count);
    free(rom.data);

    for (int damage = 0; damage < 17; damage++)
    {
        fixture(&rom, 2);
        assert(SfxCatalogReadRom(&rom, &catalog, &why));
        unsigned char *c = rom.data + CTL;
        switch (damage)
        {
        case 0: rom.info.entrycount = 1; break;
        case 1: rom.info.entries[1] = rom.info.entries[0]; break;
        case 2: rom.info.entries[0].romend = rom.size + 1; break;
        case 3: put16(c, 0x4232); break;
        case 4: put16(c + 2, 0); break;
        case 5: put32(c + 4, 0xfffffff8); break;
        case 6: put32(c + BANK + 12, CTL_SIZE - 8); break;
        case 7: put16(c + INST + 14, 32767); break;
        case 8: put32(c + INST + 16, 0); break;
        case 9: put32(c + INST + 16, CTL_SIZE - 8); break;
        case 10: put32(c + SOUND + 8, 0xfffffff0); break;
        case 11: put32(c + WAVE, TBL_SIZE + 1); break;
        case 12: put32(c + WAVE + 4, TBL_SIZE); break;
        case 13: c[WAVE + 9] = 1; break;
        case 14: rom.info.entries[0].romend = CTL + 4; break;
        case 15: rom.info.entries[1].flags = 1; break;
        case 16: rom.info.entrycount = ROM_MAX_ENTRIES + 1; break;
        }
        assert(!SfxCatalogReadRom(&rom, &catalog, &why));
        assert(why[0] && !catalog.entries && !catalog.count); /* No stale project data. */
        free(rom.data);
    }
    SfxCatalogFree(&catalog); SfxCatalogFree(&catalog);
    puts("PASS: one-based IDs, all entries, shared samples, names/filtering, replacement and malformed banks.");
    if (argc == 2) { readRealRom(argv[1]); }
    return 0;
}
