#ifndef GEDITOR_CHARACTERFLAGCATALOG_H
#define GEDITOR_CHARACTERFLAGCATALOG_H
/* Saved GuardRecord.flags, matching GUARD_SETUP_FLAG in bondconstants.h.
 * These are not the runtime character/AI flag words. Unknown bits are kept
 * individually editable without inventing gameplay meanings. */
static const ObjectFlagDefinition g_CharacterFlags[] = {
    {2, 0x0001, "GUARD_SETUP_FLAG_SUNGLASSES", "Sunglasses", "Always gives this guard sunglasses.", 0},
    {2, 0x0002, "GUARD_SETUP_FLAG_SUNGLASSES_50", "Sunglasses (50% chance)", "Gives this guard a 50% chance of sunglasses.", 0},
    {2, 0x0004, "GUARD_SETUP_FLAG_CHR_CLONE", "Clone flag", "Sets the character's clone flag on creation.", 0},
    {2, 0x0008, "GUARD_SETUP_FLAG_CHR_INVINCIBLE", "Invincible", "Creates the character with the invincible flag.", 0},
    {2, 0x0010, "GUARD_SETUP_FLAG_0010", "Unused (0x0010)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0020, "GUARD_SETUP_FLAG_0020", "Unused (0x0020)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0040, "GUARD_SETUP_FLAG_0040", "Unknown (0x0040)", "Unknown setup flag used in Train.", 0},
    {2, 0x0080, "GUARD_SETUP_FLAG_0080", "Unused (0x0080)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0100, "GUARD_SETUP_FLAG_0100", "Unused (0x0100)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0200, "GUARD_SETUP_FLAG_0200", "Unused (0x0200)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0400, "GUARD_SETUP_FLAG_0400", "Unused (0x0400)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x0800, "GUARD_SETUP_FLAG_0800", "Unused (0x0800)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x1000, "GUARD_SETUP_FLAG_1000", "Unused (0x1000)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x2000, "GUARD_SETUP_FLAG_2000", "Unused (0x2000)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x4000, "GUARD_SETUP_FLAG_4000", "Unused (0x4000)", "Unused setup flag. Preserved as authored.", 0},
    {2, 0x8000, "GUARD_SETUP_FLAG_8000", "Unused (0x8000)", "Unused setup flag. Preserved as authored.", 0}
};
#define CHARACTERFLAGS_COUNT ((int)(sizeof(g_CharacterFlags)/sizeof(*g_CharacterFlags)))
#endif
