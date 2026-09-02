#!/usr/bin/env python3
"""Replace the individually compressed room streams in a BG segment with raw data."""

import argparse
import struct
import zlib
from pathlib import Path


BG_SEGMENT = 0x0F000000
ROOM_ENTRY_SIZE = 24
STREAM_POINTER_OFFSETS = (0, 4, 8)


def read_u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def align(data, alignment):
    data.extend(b"\0" * (-len(data) % alignment))


def find_stream_references(data):
    room_table = read_u32(data, 4) - BG_SEGMENT

    if room_table < 0 or room_table + ROOM_ENTRY_SIZE > len(data):
        return []

    references = []
    null_entries = 0
    entry = room_table

    while entry + ROOM_ENTRY_SIZE <= len(data):
        words = [read_u32(data, entry + offset) for offset in range(0, ROOM_ENTRY_SIZE, 4)]

        if not any(words):
            null_entries += 1
            if null_entries == 2:
                break

        for field_offset in STREAM_POINTER_OFFSETS:
            pointer = read_u32(data, entry + field_offset)

            if pointer:
                stream_offset = pointer - BG_SEGMENT

                if stream_offset < 0 or stream_offset >= len(data):
                    raise ValueError(f"invalid BG stream pointer 0x{pointer:08x}")

                references.append((entry + field_offset, stream_offset))

        entry += ROOM_ENTRY_SIZE

    return references


def decompress_stream(stream):
    if len(stream) >= 4 and read_u32(stream, 0) == 0:
        return b""

    if stream.startswith(b"\x11\x72"):
        header_size = 2
    elif stream.startswith(b"\x11\x73"):
        header_size = 5
    else:
        raise ValueError(f"room stream has unknown header {stream[:5].hex()}")

    inflater = zlib.decompressobj(-15)
    result = inflater.decompress(stream[header_size:]) + inflater.flush()

    if not inflater.eof:
        raise ValueError("unterminated deflate stream in BG segment")

    return result


def convert(source):
    references = find_stream_references(source)

    # Static single-display-list backgrounds contain no room stream table.
    if not references:
        return source

    old_offsets = sorted({stream_offset for _, stream_offset in references})
    output = bytearray(source[:old_offsets[0]])
    new_offsets = {}

    for index, old_offset in enumerate(old_offsets):
        old_end = old_offsets[index + 1] if index + 1 < len(old_offsets) else len(source)
        raw = decompress_stream(source[old_offset:old_end])

        align(output, 16)
        output.extend(struct.pack(">I", len(raw)))
        new_offsets[old_offset] = len(output)
        output.extend(raw)

    align(output, 16)

    for pointer_offset, old_offset in references:
        struct.pack_into(">I", output, pointer_offset, BG_SEGMENT + new_offsets[old_offset])

    return bytes(output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    result = convert(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
