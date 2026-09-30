#!/usr/bin/env python3
"""Verify the built PE's startup manifest, without requiring Windows or pefile."""
from pathlib import Path
import struct
import sys
import xml.etree.ElementTree as ET


def startup_manifest(path):
    data = Path(path).read_bytes()
    u16 = lambda p: struct.unpack_from('<H', data, p)[0]
    u32 = lambda p: struct.unpack_from('<I', data, p)[0]
    assert data[:2] == b'MZ', 'not a Windows executable'
    pe = u32(0x3c)
    assert data[pe:pe+4] == b'PE\0\0', 'missing PE header'
    opt = pe + 24
    magic = u16(opt)
    assert magic in (0x10b, 0x20b), 'unsupported PE optional header'
    directories = opt + (112 if magic == 0x20b else 96)
    resource_rva = u32(directories + 2*8)
    assert resource_rva, 'executable has no resource directory'
    sections = opt + u16(pe+20)

    def offset(rva):
        for i in range(u16(pe+6)):
            s = sections + i*40
            address, length, raw = u32(s+12), u32(s+16), u32(s+20)
            if address <= rva < address+length:
                return raw+rva-address
        raise AssertionError('resource address is outside file-backed sections')

    root = offset(resource_rva)

    def entries(relative):
        directory = root+relative
        return [struct.unpack_from('<II', data, directory+16+i*8)
                for i in range(u16(directory+12)+u16(directory+14))]

    def child(relative, resource_id, name):
        found = [value for key, value in entries(relative) if key == resource_id]
        assert len(found) == 1, f'missing {name}'
        assert found[0] & 0x80000000, f'{name} must be a resource directory'
        return found[0] & 0x7fffffff

    manifests = child(0, 24, 'RT_MANIFEST (type 24)')
    startup = child(manifests, 1, 'startup manifest (resource ID 1)')
    languages = entries(startup)
    assert languages, 'startup manifest has no language entries'
    for _, leaf in languages:
        assert not leaf & 0x80000000, 'invalid manifest data entry'
        address, length = struct.unpack_from('<II', data, root+leaf)
        pos = offset(address)
        content = data[pos:pos+length]
        assert len(content) == length, 'truncated manifest'
        yield content


def check(path):
    namespace = {'a': 'urn:schemas-microsoft-com:asm.v1'}
    source = Path(__file__).resolve().parents[2] / 'src/geditor.manifest'
    for content in startup_manifest(path):
        assert content == source.read_bytes(), 'embedded manifest differs from current source'
        assembly = ET.fromstring(content)
        identities = assembly.findall('a:dependency/a:dependentAssembly/a:assemblyIdentity', namespace)
        matches = [i for i in identities if i.get('name') == 'Microsoft.Windows.Common-Controls']
        assert len(matches) == 1, 'missing/ambiguous Common Controls dependency'
        identity = matches[0]
        assert identity.get('version') == '6.0.0.0', 'Common Controls v6 was not requested'
        assert identity.get('type') == 'win32'
        assert identity.get('processorArchitecture') in ('*', 'amd64')
        assert identity.get('publicKeyToken') == '6595b64144ccf1df'
    print('PASS: built executable embeds the exact Common Controls v6 manifest at RT_MANIFEST/1.')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: check_manifest.py path/to/GEditor.exe')
    try:
        check(sys.argv[1])
    except (AssertionError, OSError, struct.error, ET.ParseError) as error:
        raise SystemExit(f'FAIL: {error}')
