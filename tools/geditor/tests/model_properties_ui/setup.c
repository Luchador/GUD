#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "objectload.h"
#include "characterload.h"
#include "modelload.h"

int main(void)
{
    SetupFile setup = {0};
    SetupObject object = {0};
    SetupCharacter character = {0};
    const char *expected, *name;
    CharacterModelDefinition definition;
    int id, count = 0;
    setup.objects = &object; setup.objectcount = 1;
    setup.characters = &character; setup.charactercount = 1;
    character.headid = -1;
    for (id = 0; ModelGetPropDefinition(id, &expected, NULL); id++)
    {
        object.modelid = id;
        assert(ObjectGetSetupModelName(&setup, 0, &name));
        assert(!strcmp(name, expected));
        count++;
    }
    assert(count > 100);
    for (id = 0; CharacterGetModelDefinition(id, &definition); id++)
    {
        character.bodyid = id;
        assert(ObjectGetSetupModelName(&setup, SETUP_CHARACTER_SELECTION_BIT, &name));
        assert(!strcmp(name, definition.filename));
    }
    assert(id > 10);
    character.bodyid = 0xffff;
    assert(ObjectGetSetupModelName(&setup, SETUP_CHARACTER_SELECTION_BIT, &name));
    assert(!strcmp(name, "CcamguardZ")); /* same stable random-body preview */
    assert(!ObjectGetSetupModelName(&setup, (DWORD)-1, &name) && !name);
    assert(!ObjectGetSetupModelName(&setup, 1, &name) && !name);
    assert(!ObjectGetSetupModelName(&setup, SETUP_CHARACTER_SELECTION_BIT | 1, &name) && !name);
    object.deleted = TRUE;
    assert(!ObjectGetSetupModelName(&setup, 0, &name) && !name);
    character.deleted = TRUE;
    assert(!ObjectGetSetupModelName(&setup, SETUP_CHARACTER_SELECTION_BIT, &name) && !name);
    object.deleted = FALSE; object.modelid = -1;
    assert(!ObjectGetSetupModelName(&setup, 0, &name) && !name);
    assert(!ObjectGetSetupModelName(NULL, 0, &name) && !name);
    assert(!ObjectGetSetupModelName(&setup, 0, NULL));
    puts("PASS setup model names: real prop/character tables, random bodies, deleted/invalid selections (ASan + UBSan)");
    return 0;
}
