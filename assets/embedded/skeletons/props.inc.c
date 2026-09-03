#if 0
char* prop_cctv_related_JointNames[] = {
    "SKEL_TOP",
    "SKEL_CAM" };
#endif

ModelJoint JOINTLIST(cctv)[2] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003}
};

MODELSKELETON(cctv, 2, 6)


//No Joint Nmaes
ModelJoint JOINTLIST(console_one_screen)[1] = {
    {0x0002, 0x0000, 0x0000}
};

MODELSKELETON(console_one_screen, 1, 3)


//No Joint Nmaes
ModelJoint JOINTLIST(console_four_screen)[] = {
    {0x0002, 0x0000, 0x0000}
};

MODELSKELETON(console_four_screen, 1, 3)

#if 0
char* prop_tv_holder_related_JointNames[] = {
    "SKEL_TOP",
    "SKEL_TV1",
    "SKEL_TV2",
    "SKEL_TV3",
    "SKEL_TV4" };
#endif

ModelJoint JOINTLIST(tv_holder)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C}
};

MODELSKELETON(tv_holder, 5, 0xF)


#if 0
char* prop_rotating_stuff_related_JointNames[] = {
    "SKEL_TOP",
    "SKEL_YROT",
    "SKEL_XROT",
    "SKEL_BARREL",
    "SKEL_CARTRIDGE",
    NULL };
#endif
#if 0
char* GroundGun_JointNames[] = {
    "SKEL_TOP",
    "SKEL_YROT",
    "SKEL_XROT",
    "SKEL_BARREL",
    "SKEL_BAR2",
    NULL };
#endif

ModelJoint JOINTLIST(rotating_stuff)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C},
    {0x0002, 0x000F, 0x000F}
};

MODELSKELETON(rotating_stuff, 6, 0x12)


#if 0
char* prop_eyelid_door_related_JointNames[] = {
    "SKEL_TOP",
    "SKEL_UPPER",
    "SKEL_LOWER" };
#endif

ModelJoint JOINTLIST(eyelid_door)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006}
};
MODELSKELETON(eyelid_door, 3, 9)


#if 0
char* prop_iris_door_related_JointNames[] = {
    "SKEL_TOP",
    "SKEL_OUT1",
    "SKEL_IN1",
    "SKEL_OUT2",
    "SKEL_IN2",
    "SKEL_OUT3",
    "SKEL_IN3",
    "SKEL_OUT4",
    "SKEL_IN4",
    "SKEL_OUT5",
    "SKEL_IN5",
    "SKEL_OUT6",
    "SKEL_IN6" };
#endif

ModelJoint JOINTLIST(iris_door)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C},
    {0x0002, 0x000F, 0x000F},
    {0x0002, 0x0012, 0x0012},
    {0x0002, 0x0015, 0x0015},
    {0x0002, 0x0018, 0x0018},
    {0x0002, 0x001B, 0x001B},
    {0x0002, 0x001E, 0x001E},
    {0x0002, 0x0021, 0x0021},
    {0x0002, 0x0024, 0x0024}
};

MODELSKELETON(iris_door, 0xD, 0x27)


//NoJointNames
ModelJoint JOINTLIST(walletbond)[] = {
    {0x0002, 0x0000, 0x0000}
};

MODELSKELETON(walletbond, 1, 3)


#if 0
char* prop_car_related_JointNames[] = {
    "base",
    "wheel1",
    "wheel2",
    "wheel3",
    "wheel4" };
#endif

ModelJoint JOINTLIST(car)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C}
};

MODELSKELETON(car, 5, 0xF)

#if 0
char* prop_flying_related_JointNames[] = {
    "SKEL_POSITION",
    "SKEL_TOP",
    "SKEL_ROTOR",
    "SKEL_TAILROTOR",
    "SKEL_GUN" };
#endif

ModelJoint JOINTLIST(flying)[] = {
    {0x0401, 0x0000, 0x0000},
    {0x0002, 0x0000, 0x0000},
    {0x0015, 0x0003, 0x0003},
    {0x0015, 0x0004, 0x0004},
    {0x0015, 0x0005, 0x0005}
};

MODELSKELETON(flying, 5, 3)


//No Joint Names
ModelJoint JOINTLIST(door)[] = {
    {0x0002, 0x0000, 0x0000}
};

MODELSKELETON(door, 1, 3)


#if 0
char* TankSkeleton_JointNames[] = {
    "SKEL_TOP",
    "SKEL_TURRET",
    "SKEL_HATCH",
    "SKEL_GUN",
    "SKEL_MUZZLE" };
#endif

ModelJoint JOINTLIST(tank)[] = {
    {0x0002, 0x0000, 0x0000},
    {0x0002, 0x0003, 0x0003},
    {0x0002, 0x0006, 0x0006},
    {0x0002, 0x0009, 0x0009},
    {0x0002, 0x000C, 0x000C}
};

MODELSKELETON(tank, 5, 0xF)


//No Joint Names
ModelJoint JOINTLIST(hat)[] = {
    {0x0015, 0x0000, 0x0000},
};

MODELSKELETON(hat, 1, 0)
