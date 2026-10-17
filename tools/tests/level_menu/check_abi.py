#!/usr/bin/env python3
"""Verify LVSL layout/relocations in actual big-endian N64 objects."""
import importlib.util
from pathlib import Path
import struct
import sys
sys.dont_write_bytecode = True
root=Path(__file__).resolve().parents[3]
spec=importlib.util.spec_from_file_location('objects',root/'tools/geditor/tests/manifest_catalogs/run.py')
objects=importlib.util.module_from_spec(spec);spec.loader.exec_module(objects)
build=Path(sys.argv[1]) if len(sys.argv)>1 else root/'build/u'
manifest=objects.Object(build/'src/game/gedmanifest.o')
version,count=struct.unpack_from('>II',manifest.bytes('g_GedManifest',24),16)
assert version==3 and count==36
blob=manifest.bytes('g_GedManifest',24+16*count)
entries=[24+i*16 for i in range(count) if blob[24+i*16:28+i*16]==b'LVSL']
assert len(entries)==1
entry=entries[0]
assert struct.unpack_from('>I',blob,entry+12)[0]==1
manifest.pointer('g_GedManifest',entry+4,'_gedLevelMenuRom')
manifest.pointer('g_GedManifest',entry+8,'_gedLevelMenuRom',132)
obj=objects.Object(build/'src/game/campaign.o')
expected=(1,33,34,35,36,9,20,26,43,27,22,24,29,30,25,37,23,39,41,28,32,
          -1,38,31,50,48,45,46,434,427,424,439,432)
assert struct.unpack('>33i',obj.bytes('g_LevelMenuConfig',132))==expected
assert obj.named['g_LevelMenuConfig'][2]==132
print('PASS: real IDO N64 objects contain the 132-byte big-endian LVSL defaults and both manifest relocations.')
