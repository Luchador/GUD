#if 0
/*********************************************************************
 * isnt this file for the UZI? the UZI has 3 muzzleflashes as oposed 
 * to the standard 2 for every other gun
 *********************************************************************/
char* weapon_gun_kf7_JointNames[] = {
    "SKEL_TOP",
    "SKEL_MUZZLE",
    "SKEL_FLASH",
    "SKEL_EXTRAFLASH",
    "SKEL_CARTRIDGE",
    "",
    ""};
#endif

ModelJoint JOINTLIST(gun_kf7)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0015, 0x0012, 0x0012},
    {0x0002, 0x000C, 0x000C},
    {0x0002, 0x000F, 0x000F}
};

MODELSKELETON(gun_kf7, 7, 0x12)
