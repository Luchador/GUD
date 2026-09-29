# Single-weapon dual-wield pickups

`PROPFLAG_WEAPON_GRANTS_DUAL` (`0x08000000` in the first object flag word)
makes one weapon pickup grant a matching dual-wield inventory entry. It does
not request an equipment change. Only weapons with `CAN_DUAL_WIELD` support
are eligible. The bit has separate existing meanings for doors and autoguns;
those behaviors are unchanged.

GEditor exposes **Grants dual wield on pickup** on weapon objects. Scripted
weapons use the same bit in the `PROPFLAG` parameter of `TRYGiveMeItem` (`0xBF`).
Ordinary two-weapon links keep their existing behavior; this flag grants the
matching pair independently, even if the marked weapon belongs to a link.

Control's stock action block `0x0411` now sets this bit on Boris's PP7.
For an existing customized project:

1. Rebuild GUD with the patch and rebase the project onto that ROM.
2. Open Control's action block `0x0411` and find `TRYGiveMeItem` with
   `PROP_NUM = 0xBF` and `ITEM_NUM = 4` (the regular PP7).
3. Set its `PROPFLAG` to `0x08000000`, retaining any other intentional bits.
   Enter this normal value in GEditor; the reversed literal in the generated
   C setup is specific to the old AI macro's byte encoding.
4. Save the project and create the playable ROM.

Run `python3 tools/tests/weapon_dual_pickup/run.py` from the repository root.
The ASan/UBSan host fixture executes the production inventory functions and
complete pickup eligibility gate. It checks full-ammo pickups, repeat pickups,
existing links, unsupported items, inventory-only grants, and the encoded
Boris spawn command. Platform services are stubbed; in-game testing remains
necessary. Editor flag persistence is covered by
`python3 tools/geditor/tests/object_flags/run.py`.
