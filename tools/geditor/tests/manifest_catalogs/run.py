#!/usr/bin/env python3
"""Check manifest/catalog ABI and relocations in real IDO big-endian MIPS objects.

Run after building GUD, or its eight changed translation units. No third-party
Python modules are needed. This checks emitted bytes, not a host pointer ABI.
"""
from pathlib import Path
import struct
import sys


class Object:
    def __init__(self, path):
        self.data = data = path.read_bytes()
        assert data[:6] == b"\x7fELF\x01\x02", f"Not a big-endian ELF32 object: {path}"
        assert struct.unpack_from(">H", data, 18)[0] == 8, "Expected MIPS"
        start = struct.unpack_from(">I", data, 32)[0]
        stride, count = struct.unpack_from(">HH", data, 46)
        self.sections = [struct.unpack_from(">10I", data, start + i * stride) for i in range(count)]
        symtab = next(s for s in self.sections if s[1] == 2)
        strings = self.sections[symtab[6]]
        names = data[strings[4]:strings[4] + strings[5]]
        self.symbols = []
        for offset in range(symtab[4], symtab[4] + symtab[5], 16):
            name, value, size, info, other, section = struct.unpack_from(">IIIBBH", data, offset)
            name = names[name:names.index(0, name)].decode()
            self.symbols.append((name, value, size, section))
        self.named = {s[0]: s for s in self.symbols if s[0]}
        self.relocations = {}
        for section in self.sections:
            if section[1] != 9:  # SHT_REL
                continue
            for offset in range(section[4], section[4] + section[5], 8):
                at, info = struct.unpack_from(">II", data, offset)
                self.relocations[section[7], at] = (self.symbols[info >> 8][0], info & 255)

    def bytes(self, name, length=None):
        _, at, size, index = self.named[name]
        size = size if length is None else length
        assert at + size <= self.sections[index][5]
        at += self.sections[index][4]
        return self.data[at:at + size]

    def pointer(self, name, offset, target, addend=0):
        _, at, _, index = self.named[name]
        assert self.relocations[index, at + offset] == (target, 2), (name, offset, target)
        assert struct.unpack_from(">I", self.bytes(name, offset + 4), offset)[0] == addend


def main():
    root = Path(__file__).resolve().parents[4]
    build = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "build/u"
    # tag, unit, descriptor, native table, stride, trailing terminators, linker alias
    catalogs = [
        ("MUSV", "music", "g_GedMusicVolumes", "g_musicDefaultTrackVolume", 2, 0, "MusicVolumes"),
        ("ANIC", "game/initanitable", "g_GedCharacterAnimations", "animation_table_ptrs1", 4, 1, "CharacterAnimations"),
        ("ANIO", "game/initanitable", "g_GedObjectAnimations", "animation_table_ptrs2", 4, 1, "ObjectAnimations"),
        ("CHRM", "game/cobjdata", "g_GedCharacterModels", "CitemZ_entries", 20, 1, "CharacterModels"),
        ("PROP", "game/pobjdata", "g_GedPropModels", "PitemZ_entries", 12, 1, "PropModels"),
        ("ITEM", "game/gun", "g_GedItemModels", "gitem_structs", 56, 0, "ItemModels"),
        ("AIGL", "game/chraidata", "g_GedGlobalAILists", "g_GlobalAILists", 8, 1, "GlobalAILists"),
        ("TXBK", "game/language", "g_GedTextBanks", "g_TextBankNames", 4, 0, "TextBanks"),
    ]
    manifest = Object(build / "src/game/gedmanifest.o")
    blob = manifest.bytes("g_GedManifest")
    version, count = struct.unpack_from(">II", blob, 16)
    assert blob[:16] == b"GUDGEDITORMANIF\0" and version == 3 and count == 28
    # IDO's ELF symbol size excludes the leading initialized char array here;
    # the complete manifest bytes and relocations are present in the section.
    blob = manifest.bytes("g_GedManifest", 24 + count * 16)
    entries = {}
    for i in range(count):
        offset = 24 + i * 16
        tag = blob[offset:offset + 4].decode()
        assert tag not in entries, f"Duplicate manifest kind: {tag}"
        entries[tag] = offset
    assert set(entries) == set("IMGS OBSG MUSF STGT CMAP FTBL ENVT TXTB TXCF MONA MONT MOND "
                               "SFXC SFXT INSC INST MUST MUSD MUSV ANID ANIF ANIC ANIO CHRM PROP ITEM AIGL TXBK".split())
    for tag, unit, desc, table, stride, terminators, alias in catalogs:
        obj = Object(build / f"src/{unit}.o")
        metadata = obj.bytes(desc)
        records = obj.bytes(table)
        pointer, slots, actual_stride, schema = struct.unpack(">4I", metadata)
        assert schema == 1 and actual_stride == stride, (tag, schema, actual_stride)
        assert slots > 0 and len(records) == (slots + terminators) * stride, tag
        obj.pointer(desc, 0, table)
        if terminators:
            assert records[slots * stride:slots * stride + 4] == b"\0" * 4, tag
        offset = entries[tag]
        assert struct.unpack_from(">I", blob, offset + 12)[0] == 0x80000000
        manifest.pointer("g_GedManifest", offset + 4, f"_ged{alias}Rom")
        manifest.pointer("g_GedManifest", offset + 8, f"_ged{alias}Rom", 16)
        if tag == "ANIC":
            values = struct.unpack(f">{len(records)//4}I", records)
            assert values[0] > 1 and 1 in values[:-1] and values[-1] == 0
        if tag == "TXBK":
            assert records[:4] == b"\0" * 4 and slots > 1
            assert obj.relocations[obj.named[table][3], obj.named[table][1] + 4][1] == 2
        if tag == "MUSV":
            assert records[-2:] == b"\xff\xff", "Preserve control slot"
        print(f"{tag}: {slots} slots, {stride} bytes/slot, descriptor v{schema}")
    ranges = {
        "SFXC": ("_sfxctlSegmentRomStart", "_sfxctlSegmentRomEnd", 0),
        "SFXT": ("_sfxtblSegmentRomStart", "_sfxtblSegmentRomEnd", 0),
        "INSC": ("_instrumentsctlSegmentRomStart", "_instrumentsctlSegmentRomEnd", 0),
        "INST": ("_instrumentstblSegmentRomStart", "_instrumentstblSegmentRomEnd", 0),
        "MUST": ("_musicsampletblSegmentRomStart", "_musicsampletblSegmentRomEnd", 8),
        "MUSD": ("_gedMusicDataRomStart", "_gedMusicDataRomEnd", 0),
        "ANID": ("_animation_dataSegmentRomStart", "_animation_dataSegmentRomEnd", 0),
        "ANIF": ("_animation_entriesSegmentRomStart", "_animation_entriesSegmentRomEnd", 0),
        "MUSF": ("_musicfilesSegmentRomStart", "_musicfilesSegmentRomEnd", 0),
    }
    for tag, (start, end, flags) in ranges.items():
        offset = entries[tag]
        manifest.pointer("g_GedManifest", offset + 4, start)
        manifest.pointer("g_GedManifest", offset + 8, end)
        assert struct.unpack_from(">I", blob, offset + 12)[0] == flags, tag
    print("PASS: 28 manifest kinds, catalog ABI/counts/terminators, native pointer relocations and raw segment anchors.")


if __name__ == "__main__":
    main()
