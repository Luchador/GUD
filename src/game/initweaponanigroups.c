#include <ultra64.h>
#include "chr.h"
#include "model.h"


void init_weapon_animation_groups_maybe(void)
{
    modelSetVertexAllocator(chrAllocateVertices);
    initWeaponAnimGroups();
}
