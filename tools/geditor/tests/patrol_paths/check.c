#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "setupload.h"

void PatrolViewport(void);
void PadPreview(void);

static void Put(unsigned char *p, DWORD v)
{ p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static DWORD Read(const unsigned char *p)
{ return (DWORD)p[0] << 24 | (DWORD)p[1] << 16 | (DWORD)p[2] << 8 | p[3]; }

static void Reject(SetupFile *setup)
{
    SetupPatrolLink *links = NULL;
    DWORD count = 0;
    const char *why;
    assert(!SetupFileBuildPatrolLinks(setup, &links, &count, NULL, &why));
    assert(!links && !count && why[0]);
}

static void Synthetic(void)
{
    unsigned char data[256] = {0}, original[256];
    SetupPad pads[4] = {0};
    SetupFile setup = {0};
    SetupPatrolLink *links;
    DWORD count;
    unsigned char pathpads[4]={0};
    const char *why;
    setup.data = data; setup.size = sizeof(data); setup.pads = pads; setup.padcount = 4;
    Put(data, 40); Put(data + 16, 120);
    /* Waypoints 0,1,2 reference pads 3,0,2. Neighbours are irrelevant. */
    Put(data + 40, 3); Put(data + 56, 0); Put(data + 72, 2); Put(data + 88, 0xffffffff);
    Put(data + 120, 160); data[124] = 71; data[125] = 1; /* path ID is not its index */
    Put(data + 160, 0); Put(data + 164, 1); Put(data + 168, 2); Put(data + 172, 0xffffffff);
    memcpy(original, data, sizeof(data));
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && count == 3);
    assert(links[0].pads[0] == 0 && links[0].pads[1] == 2 && links[0].directions == 1);
    assert(links[1].pads[0] == 0 && links[1].pads[1] == 3 && links[1].directions == 2);
    assert(links[2].pads[0] == 2 && links[2].pads[1] == 3 && links[2].directions == 1);
    free(links);
    assert(!memcmp(data, original, sizeof(data)) && !setup.dirty);
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, pathpads, &why));
    assert(pathpads[0] && !pathpads[1] && pathpads[2] && pathpads[3]); free(links);
    /* Back-and-forth paths never close the last-to-first edge. */
    data[125] = 0;
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && count == 2);
    assert(links[0].directions == 3 && links[1].directions == 3);
    free(links);
    /* An overlapping loop merges with the two-way route, without duplicate lines. */
    Put(data + 128, 160); data[133] = 1;
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && count == 3);
    assert(links[0].directions == 3 && links[1].directions == 3 && links[2].directions == 1);
    free(links); Put(data + 128, 0);
    /* Empty/single-pad and repeated-pad routes do not emit self connections. */
    Put(data + 164, 0); Put(data + 168, 0xffffffff); data[125] = 1;
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && !count); free(links);
    Put(data + 164, 0xffffffff);
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && !count); free(links);
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, pathpads, &why) && !count);
    assert(!pathpads[0] && !pathpads[1] && !pathpads[2] && pathpads[3]); free(links);
    Put(data + 160, 0xffffffff);
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && !count); free(links);
    memcpy(data, original, sizeof(data));
    Put(data + 168, 3); Reject(&setup); Put(data + 168, 2); /* waypoint terminator */
    Put(data + 72, 4); Reject(&setup); Put(data + 72, 2); /* missing pad */
    Put(data + 120, 253); Reject(&setup); Put(data + 120, 160);
    Put(data, 252); Reject(&setup); Put(data, 40);
    Put(data + 16, 252); Reject(&setup); Put(data + 16, 120);
    setup.size = 172; Reject(&setup); setup.size = sizeof(data);
    setup.pads = NULL; Reject(&setup); setup.pads = pads;
    Put(data + 16, 0);
    assert(SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why) && !count && !links);
    puts("PASS: waypoint-to-pad mapping, loop closure, reversing routes, shared links, empty routes, invalid offsets/references and unchanged setup bytes.");
}

int main(int argc, char **argv)
{
    Synthetic();
    PatrolViewport();
    PadPreview();
    for (int i = 2; i < argc; i++)
    {
        SetupFile setup = {0};
        SetupPatrolLink *links;
        DWORD count, pad;
        const char *why;
        char path[1024];
        FILE *file;
        snprintf(path, sizeof(path), "%s/setup/%s.set", argv[1], argv[i]);
        file = fopen(path, "rb"); assert(file);
        assert(!fseek(file, 0, SEEK_END)); setup.size = ftell(file); rewind(file);
        setup.data = malloc(setup.size); assert(setup.data && setup.size >= 40);
        assert(fread(setup.data, 1, setup.size, file) == setup.size); fclose(file);
        /* Test native route data even in unused stock setups whose unrelated
         * stan-name strings do not pass the editor's complete project loader. */
        pad = Read(setup.data + 24);
        for (;; pad += 44)
        {
            assert(pad <= setup.size && setup.size - pad >= 44);
            if (!Read(setup.data + pad + 36)) { break; }
            setup.padcount++;
        }
        setup.pads = calloc(setup.padcount ? setup.padcount : 1, sizeof(*setup.pads)); assert(setup.pads);
        if (!SetupFileBuildPatrolLinks(&setup, &links, &count, NULL, &why))
        { fprintf(stderr, "%s: %s\n", argv[i], why); abort(); }
        printf("LINKS %s", argv[i]);
        for (DWORD j = 0; j < count; j++)
        { printf(" %u,%u,%u", (unsigned)links[j].pads[0], (unsigned)links[j].pads[1], links[j].directions); }
        putchar('\n');
        assert(!setup.dirty);
        free(links); SetupFileFree(&setup);
    }
    return 0;
}
