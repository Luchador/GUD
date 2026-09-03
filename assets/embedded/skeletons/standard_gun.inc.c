ModelJoint JOINTLIST(standard_gun)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0015, 0x000F, 0x000F},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C}
};

#if 0
char* weapon_gun_standard_gun_JointNames[] = {
    "SKEL_TOP",
    "SKEL_MUZZLE",
    "SKEL_FLASH",
    "SKEL_CARTRIDGE",
    "SKEL_SLIDE",
    "SKEL_FINGER" };
#endif

MODELSKELETON(standard_gun, 6, 15)
