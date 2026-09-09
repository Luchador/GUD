/* Setup-character preview with authored heads and visible held equipment.
   Runtime AI and animation are not simulated. */
#include <windows.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <src/propconstants.h>

#include "characterload.h"
#include "gltf.h"
#include "modelload.h"

/* Reuse the game's complete CitemZ_entries order and metadata. The included
   model headers supply switch counts through local header placeholders;
   no N64 structs, skeletons or runtime symbols enter the editor build. */
#define MODELFILEHEADER(NAME, ROOT, SKELETON, SWITCHES, NUMSWITCHES, NUMMATRICES, RADIUS, RECORDS, TEXTURES) \
    static const int NAME ## _header = NUMSWITCHES;
#include <assets/obseg/chr/chrModelFileHeaders.inc.c>
#undef MODELFILEHEADER

typedef struct CharacterSourceDefinition {
    const int *header;
    const char *filename;
    float scale;
    float pov;
    unsigned char ismale, hashead, pad1, pad2;
} CharacterSourceDefinition;

#define ChrModelFileRecord CharacterSourceDefinition
#define CitemZ_entries g_CharacterModels
#include <assets/obseg/chr/chrModelFileRecords.inc.c>
#undef CitemZ_entries
#undef ChrModelFileRecord

#define CHARACTER_MODEL_COUNT \
    ((int)(sizeof(g_CharacterModels) / sizeof(g_CharacterModels[0])) - 1)

typedef struct CharacterPart {
    BOOL attempted;
    BgVertex *vertices;
    unsigned short *tags;
    BgRenderFlags *renderflags;
    DWORD tricount;
    float bottom;
    float headposition[3];
    BOOL hasheadposition;
    float handposition[2][3]; /* right, left: chrEquipWeapon Switches[3/5] */
    BOOL hashandposition[2];
} CharacterPart;

typedef struct CharacterEquipment {
    CharacterPart part;
    float origin[3];
    float scale;
    BOOL usesmodelscale;
} CharacterEquipment;

typedef struct CharacterBuilder {
    BgVertex *vertices;
    unsigned short *tags;
    BgRenderFlags *renderflags;
    DWORD *indices;
    DWORD count;
    DWORD capacity;
} CharacterBuilder;

BOOL CharacterGetModelDefinition(int modelid, CharacterModelDefinition *out)
{
    const CharacterSourceDefinition *source;

    if (modelid < 0 || modelid >= CHARACTER_MODEL_COUNT) { return FALSE; }
    source = &g_CharacterModels[modelid];
    if (out != NULL)
    {
        out->filename = source->filename;
        /* makeonebody applies this additional scale to character models. */
        out->scale = source->scale * 0.10000001f;
        out->ismale = source->ismale != 0;
        out->hashead = source->hashead != 0;
    }
    return TRUE;
}

static int CharacterFindModel(const char *filename)
{
    int i;

    for (i = 0; i < CHARACTER_MODEL_COUNT; i++)
    {
        if (strcmp(g_CharacterModels[i].filename, filename) == 0) { return i; }
    }
    return -1;
}

BOOL CharacterResolveModels(const SetupCharacter *character,
                             int *bodyid, int *headid)
{
    CharacterModelDefinition body;

    *bodyid = character->bodyid;
    /* 0xffff requests the game's random body pool. Use its first body for
       a stable editor preview, just as random heads have stable defaults. */
    if (*bodyid == 0xffff) { *bodyid = CharacterFindModel("CcamguardZ"); }
    if (!CharacterGetModelDefinition(*bodyid, &body)) { return FALSE; }
    *headid = -1;
    if (!body.hashead)
    {
        *headid = character->headid >= 0 ? character->headid
            : CharacterFindModel(body.ismale ? "CheadchrisZ" : "CheadvivienZ");
        if (!CharacterGetModelDefinition(*headid, NULL)) { return FALSE; }
    }
    return TRUE;
}

static CharacterPart *CharacterGetPart(CharacterPart *cache, int modelid,
                                        const char *projectdir, const RomFile *rom)
{
    CharacterPart *part = &cache[modelid];
    const CharacterSourceDefinition *definition = &g_CharacterModels[modelid];
    const char *why;
    char path[MAX_PATH];
    int written;
    DWORD i, offset, size;

    if (!part->attempted)
    {
        part->attempted = TRUE;
        written = snprintf(path, sizeof(path), "%s\\models\\characters\\%s.gltf",
                            projectdir, definition->filename);
        if (written < 0 || written >= (int)sizeof(path)) { return NULL; }
        part->vertices = GltfLoadModel(path, projectdir, &part->tricount,
                                       &part->tags, &part->renderflags, &why);
        if (part->vertices == NULL) { return NULL; }
        part->bottom = part->vertices[0].y;
        for (i = 1; i < part->tricount * 3; i++)
        {
            if (part->vertices[i].y < part->bottom) { part->bottom = part->vertices[i].y; }
        }
        /* Flattened glTFs retain edited surfaces. Read attachment points
           from the import ROM, as props do for authored placement boxes. */
        if (rom != NULL && rom->data != NULL
            && RomFindFile(rom, definition->filename, &offset, &size, &why))
        {
            if (!definition->hashead)
            {
                part->hasheadposition = ModelReadHeadAttachment(rom->data + offset,
                    size, part->headposition);
            }
            part->hashandposition[0] = ModelReadSwitchAttachment(rom->data + offset,
                size, *definition->header, 3, part->handposition[0]);
            part->hashandposition[1] = ModelReadSwitchAttachment(rom->data + offset,
                size, *definition->header, 5, part->handposition[1]);
        }
    }
    return part->vertices != NULL ? part : NULL;
}

static CharacterEquipment *CharacterGetEquipment(CharacterEquipment *cache,
    int count, int modelid, const char *projectdir, const RomFile *rom)
{
    CharacterEquipment *entry;
    const char *name, *why;
    DWORD offset, size;

    if (modelid < 0 || modelid >= count) { return NULL; }
    entry = &cache[modelid];
    if (!entry->part.attempted)
    {
        entry->part.attempted = TRUE;
        /* chrRenderHeldWeapon evaluates the root against the hand matrix.
           Restore that root's offset without baking it into the project asset. */
        if (rom == NULL || rom->data == NULL
            || !ModelGetPropDefinition(modelid, &name, NULL)
            || !RomFindFile(rom, name, &offset, &size, &why)
            || !ModelReadHeldPlacement(rom->data + offset, size,
                entry->origin, &entry->usesmodelscale)) { return NULL; }
        entry->part.vertices = ModelLoadProjectGeometry(projectdir, modelid,
            &entry->part.tricount, &entry->part.tags, &entry->part.renderflags,
            &entry->scale, &why);
    }
    return entry->part.vertices != NULL && entry->part.tricount > 0 ? entry : NULL;
}

static BOOL CharacterReserve(CharacterBuilder *builder, DWORD add)
{
    DWORD needed, capacity;
    BgVertex *vertices;
    unsigned short *tags;
    BgRenderFlags *renderflags;
    DWORD *indices;

    if (add > 1000000u - builder->count) { return FALSE; }
    needed = builder->count + add;
    if (needed <= builder->capacity) { return TRUE; }
    capacity = builder->capacity ? builder->capacity : 1024;
    while (capacity < needed) { capacity *= 2; }
    vertices = (BgVertex *)realloc(builder->vertices, (size_t)capacity * 3 * sizeof(*vertices));
    if (vertices == NULL) { return FALSE; }
    builder->vertices = vertices;
    tags = (unsigned short *)realloc(builder->tags, (size_t)capacity * sizeof(*tags));
    if (tags == NULL) { return FALSE; }
    builder->tags = tags;
    renderflags = (BgRenderFlags *)realloc(
        builder->renderflags, (size_t)capacity * sizeof(*renderflags));
    if (renderflags == NULL) { return FALSE; }
    builder->renderflags = renderflags;
    indices = (DWORD *)realloc(builder->indices, (size_t)capacity * sizeof(*indices));
    if (indices == NULL) { return FALSE; }
    builder->indices = indices;
    builder->capacity = capacity;
    return TRUE;
}

static BOOL CharacterPlacePart(CharacterBuilder *builder, const CharacterPart *part,
                                 const float offset[3], const float position[3],
                                 float scale, float partscale, BOOL lefthand,
                                 float facingx, float facingz, DWORD index)
{
    DWORD tri;
    float sign = lefthand ? -1.0f : 1.0f;

    if (!CharacterReserve(builder, part->tricount)) { return FALSE; }
    for (tri = 0; tri < part->tricount; tri++)
    {
        DWORD output = builder->count + tri;
        int corner;

        builder->tags[output] = (unsigned short)(part->tags[tri] | BG_TRI_OBJECT);
        builder->renderflags[output] = part->renderflags[tri];
        builder->indices[output] = SETUP_CHARACTER_SELECTION_BIT | index;
        for (corner = 0; corner < 3; corner++)
        {
            const BgVertex *source = &part->vertices[tri * 3 + corner];
            BgVertex *dest = &builder->vertices[output * 3 + corner];
            float x = (sign * partscale * source->x + offset[0]) * scale;
            float y = (sign * partscale * source->y + offset[1]) * scale;
            float z = (partscale * source->z + offset[2]) * scale;

            *dest = *source;
            /* process_01_group_heading rotates only about world Y, using
               atan2(pad.look.x, pad.look.z); the pad's up vector is ignored. */
            dest->x = position[0] + facingz * x + facingx * z;
            dest->y = position[1] + y;
            dest->z = position[2] - facingx * x + facingz * z;
            /* Left-hand equipment rotates 180 degrees about local Z. */
            dest->environment.normal[0] = facingz * sign * source->environment.normal[0]
                + facingx * source->environment.normal[2];
            dest->environment.normal[1] = sign * source->environment.normal[1];
            dest->environment.normal[2] = -facingx * sign * source->environment.normal[0]
                + facingz * source->environment.normal[2];
        }
    }
    builder->count += part->tricount;
    return TRUE;
}

static BOOL CharacterPlaceEquipment(CharacterBuilder *builder,
    CharacterEquipment *cache, int count, const SetupFile *setup, DWORD index,
    const char *projectdir, const RomFile *rom, const CharacterPart *body,
    const float bodyoffset[3], const float position[3], float scale,
    float facingx, float facingz)
{
    const SetupCharacter *character = &setup->characters[index];
    BOOL occupied[2] = { FALSE, FALSE };
    DWORD i;

    for (i = 0; i < setup->objectcount; i++)
    {
        const SetupObject *object = &setup->objects[i];
        CharacterEquipment *equipment;
        float offset[3], partscale;
        int hand, axis;

        /* weaponAssignToHome uses a literal character ID in the pad field.
           Only collectables are hand attachments; keys/other inventory and
           concealed weapons must not claim a visible hand. Setup order decides
           which weapon occupies it, as in chrEquipWeapon. */
        if (object->deleted || object->type != PROPDEF_COLLECTABLE
            || !(object->flags & PROPFLAG_ASSIGNEDTOCHR)
            || (object->flags & PROPFLAG_CONCEAL_GUN)
            || object->pad < 0 || (unsigned short)object->pad != character->chrnum
            || object->sourceoffset <= character->sourceoffset) { continue; }
        hand = (object->flags & PROPFLAG_WEAPON_LEFTHANDED) ? 1 : 0;
        if (occupied[hand]) { continue; }
        occupied[hand] = TRUE;
        if (!body->hashandposition[hand]
            || (object->flags2 & PROPFLAG2_ONLYEXPLOSIONDAMAGE)) { continue; }
        equipment = CharacterGetEquipment(cache, count, object->modelid, projectdir, rom);
        if (equipment == NULL) { continue; }
        for (axis = 0; axis < 3; axis++)
        {
            float sign = hand == 1 && axis < 2 ? -1.0f : 1.0f;

            offset[axis] = bodyoffset[axis] + body->handposition[hand][axis]
                + sign * equipment->origin[axis];
        }
        /* GROUP/GROUPSIMPLE roots inherit scale from the hand. Applying the
           pickup's model scale again would shrink ordinary guns by 10x. */
        partscale = equipment->usesmodelscale
            ? equipment->scale * ((float)object->extrascale / 256.0f) : 1.0f;
        if (!CharacterPlacePart(builder, &equipment->part, offset, position,
            scale, partscale, hand == 1, facingx, facingz, index)) { return FALSE; }
    }
    return TRUE;
}

BOOL CharacterGetPadPosition(const SetupPad *pad, const StanFile *stan,
                              float levelscale, float position[3])
{
    int axis;

    if (pad == NULL || !isfinite(levelscale) || levelscale <= 0.0f) { return FALSE; }
    for (axis = 0; axis < 3; axis++) { position[axis] = pad->pos[axis] / levelscale; }
    if (stan != NULL && stan->tiles != NULL && stan->tilecount > 0)
    {
        DWORD tile = StanResolvePadTile(stan, pad->stanname, position);

        if (tile == STAN_TILE_NONE
            || !StanGetTileHeight(stan, tile, position[0], position[2], &position[1]))
        {
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CharacterLoadSetupGeometry(const char *projectdir, const SetupFile *setup,
                                 const StanFile *stan, const RomFile *rom,
                                 float levelscale, SetupObjectGeometry *out,
                                 const char **reasonout)
{
    CharacterPart cache[CHARACTER_MODEL_COUNT];
    CharacterEquipment *equipment = NULL;
    int equipmentcount = 0;
    CharacterBuilder builder;
    DWORD i;
    BOOL ok = FALSE;

    ZeroMemory(out, sizeof(*out));
    ZeroMemory(cache, sizeof(cache));
    ZeroMemory(&builder, sizeof(builder));
    *reasonout = "";
    if (setup == NULL || setup->charactercount == 0) { return TRUE; }
    if (projectdir == NULL || !(levelscale > 0.0f))
    {
        *reasonout = "the character preview has an invalid project or level scale.";
        return FALSE;
    }
    if (setup->objectcount > 0)
    {
        while (ModelGetPropDefinition(equipmentcount, NULL, NULL)) { equipmentcount++; }
        equipment = (CharacterEquipment *)calloc((size_t)equipmentcount, sizeof(*equipment));
        if (equipment == NULL) { goto done; }
    }
    if (setup->padcount > 0)
    {
        out->occupiedpads = (unsigned char *)calloc(setup->padcount, 1);
        if (out->occupiedpads == NULL) { goto done; }
    }
    for (i = 0; i < setup->charactercount; i++)
    {
        const SetupCharacter *character = &setup->characters[i];
        const SetupPad *pad;
        CharacterModelDefinition definition;
        CharacterPart *body, *head = NULL;
        float position[3], bodyoffset[3] = { 0.0f, 0.0f, 0.0f };
        float headoffset[3], length, facingx, facingz;
        int bodyid, headid, axis;

        if (character->deleted || character->pad >= setup->padcount
            || !CharacterResolveModels(character, &bodyid, &headid)) { continue; }
        pad = &setup->pads[character->pad];
        if (!CharacterGetPadPosition(pad, stan, levelscale, position)) { continue; }
        body = CharacterGetPart(cache, bodyid, projectdir, rom);
        if (body == NULL) { continue; }
        if (headid >= 0)
        {
            head = CharacterGetPart(cache, headid, projectdir, rom);
            if (head == NULL || !body->hasheadposition) { continue; }
        }
        if (!CharacterGetModelDefinition(bodyid, &definition)) { continue; }
        /* The unanimated pose is rooted at the pelvis. Seat its feet on
           the floor; animated root motion is deliberately not simulated. */
        bodyoffset[1] = -body->bottom;
        length = sqrtf(pad->look[0] * pad->look[0] + pad->look[2] * pad->look[2]);
        facingx = length > 0.000001f ? pad->look[0] / length : 0.0f;
        facingz = length > 0.000001f ? pad->look[2] / length : 1.0f;
        if (!CharacterPlacePart(&builder, body, bodyoffset, position,
                                 definition.scale, 1.0f, FALSE, facingx, facingz, i)) { goto done; }
        if (head != NULL)
        {
            for (axis = 0; axis < 3; axis++)
            {
                headoffset[axis] = body->headposition[axis] + bodyoffset[axis];
            }
            if (!CharacterPlacePart(&builder, head, headoffset, position,
                                     definition.scale, 1.0f, FALSE, facingx, facingz, i)) { goto done; }
        }
        if (!CharacterPlaceEquipment(&builder, equipment, equipmentcount, setup, i,
            projectdir, rom, body, bodyoffset, position, definition.scale,
            facingx, facingz)) { goto done; }
        out->occupiedpads[character->pad] = 1;
        out->objectcount++;
    }
    out->tris = builder.vertices;
    out->tritags = builder.tags;
    out->renderflags = builder.renderflags;
    out->objectindices = builder.indices;
    out->tricount = builder.count;
    ok = TRUE;

done:
    if (equipment != NULL)
    {
        for (i = 0; i < (DWORD)equipmentcount; i++)
        {
            free(equipment[i].part.vertices);
            free(equipment[i].part.tags);
            free(equipment[i].part.renderflags);
        }
        free(equipment);
    }
    for (i = 0; i < CHARACTER_MODEL_COUNT; i++)
    {
        free(cache[i].vertices);
        free(cache[i].tags);
        free(cache[i].renderflags);
    }
    if (!ok)
    {
        free(builder.vertices); free(builder.tags); free(builder.renderflags); free(builder.indices);
        ObjectGeometryFree(out);
        *reasonout = "out of memory building setup character geometry.";
    }
    return ok;
}
