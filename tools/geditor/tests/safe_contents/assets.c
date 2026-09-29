/* Only replace asset transport and unrelated character/monitor systems.
 * Geometry, native placement, stan queries, persistence and history are real. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "modelload.h"
#include "characterload.h"
#include "bghistory.h"
static struct { const char *name; unsigned char *data; DWORD size,offset; } files[]={
    {.name="PsafeZ"}, {.name="PsafedoorZ"}
};
static DWORD romsize;
void Fixtures(const char *root)
{
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++)
    {
        char path[1024];
        snprintf(path,sizeof(path),"%s/assets/obseg/%s/%s.bin",root,files[i].name[0]=='C'?"chr":"prop",files[i].name);
        FILE *f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); files[i].size=ftell(f); rewind(f);
        files[i].data=malloc(files[i].size); assert(files[i].data);
        assert(fread(files[i].data,1,files[i].size,f)==files[i].size); fclose(f);
        files[i].offset=romsize; romsize+=files[i].size;
    }
}
BOOL RomLoad(const char *path, RomFile *rom, const char **reason)
{
    memset(rom,0,sizeof(*rom)); rom->data=malloc(romsize); rom->size=romsize; assert(rom->data);
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++)
    { memcpy(rom->data+files[i].offset,files[i].data,files[i].size); }
    *reason=""; return TRUE;
}
void RomFree(RomFile *rom) { free(rom->data); memset(rom,0,sizeof(*rom)); }
BOOL RomFindFile(const RomFile *rom, const char *name, DWORD *offset, DWORD *size, const char **reason)
{
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++) if (!strcmp(name,files[i].name))
    { *offset=files[i].offset; *size=files[i].size; return TRUE; }
    return FALSE;
}
const unsigned char *ModelEditsGetData(const char *dir, const char *name, DWORD *size, const char **reason)
{
    *reason="";
    for (unsigned i=0;i<sizeof(files)/sizeof(*files);i++) if (!strcmp(name,files[i].name))
    { *size=files[i].size; return files[i].data; }
    return NULL;
}
BgVertex *GltfLoadModel(const char *path, const char *dir, DWORD *count, unsigned short **tags,
    BgRenderFlags **flags, const char **reason) { *reason="No fixture for this model."; return NULL; }
BOOL MonitorBankLoadRom(MonitorBank *bank, const RomFile *rom, const char **reason)
{ memset(bank,0,sizeof(*bank)); return TRUE; }
void MonitorGeometryFree(MonitorGeometry *geometry)
{ free(geometry->surfaces); memset(geometry,0,sizeof(*geometry)); }

BOOL CharacterLoadSetupGeometry(const char *dir, const SetupFile *setup, const StanFile *stan,
    const RomFile *rom, float scale, SetupObjectGeometry *out, const char **why)
{ memset(out, 0, sizeof(*out)); return TRUE; }
void BgDocumentFree(BgDocument *d) { memset(d, 0, sizeof(*d)); }
void StanFileFree(StanFile *d) { memset(d, 0, sizeof(*d)); }
void FreeFixtures(void) { for (unsigned i=0; i<sizeof(files)/sizeof(*files); i++) free(files[i].data); }
