#!/usr/bin/env python3

import argparse
import re
import struct
from pathlib import Path


ENTRY_ARRAY_RE = re.compile(
    r"u32\s+ANIM_ENTRY_(?P<name>[A-Za-z0-9_]+)\s*\[\]\s*=\s*"
    r"\{(?P<body>.*?)\};",
    re.DOTALL,
)
DATA_HEADER_RE = re.compile(
    r"(?P<prefix>u32\s+ANIM_DATA_(?P<name>[A-Za-z0-9_]+)\s*\[\]\s*=\s*"
    r"\{\s*PTR_ANIM_ENTRY_(?P=name)\s*,\s*)"
    r"(?P<info>0x[0-9a-fA-F]+)"
    r"(?P<middle>\s*,\s*0x[0-9a-fA-F]+\s*,\s*)"
    r"(?P<layout>0x[0-9a-fA-F]+)",
    re.DOTALL,
)
HEX_RE = re.compile(r"0x[0-9a-fA-F]+")


def parse_entry_arrays(source: str) -> list[tuple[str, bytes]]:
    entries = []

    for match in ENTRY_ARRAY_RE.finditer(source):
        words = [int(value, 16) for value in HEX_RE.findall(match.group("body"))]

        if not words:
            raise ValueError(f"ANIM_ENTRY_{match.group('name')} contains no data")

        data = b"".join(struct.pack(">I", word) for word in words)
        entries.append((match.group("name"), data))

    if not entries:
        raise ValueError("no ANIM_ENTRY arrays were found")

    return entries


def read_packed_value(frame: bytes, bit_offset: int, width: int) -> int:
    frame_value = int.from_bytes(frame, byteorder="big")
    shift = len(frame) * 8 - bit_offset - width
    return (frame_value >> shift) & ((1 << width) - 1)


def convert(
    entries_source: str,
    data_source: str,
    header_source: str,
) -> tuple[bytes, str, str]:
    entries = parse_entry_arrays(entries_source)
    entry_data = dict(entries)
    offsets = {}
    layouts = {}
    output = bytearray()

    def replace_data_header(match: re.Match[str]) -> str:
        name = match.group("name")

        if name not in entry_data:
            raise ValueError(f"ANIM_DATA_{name} has no matching ANIM_ENTRY array")

        info = int(match.group("info"), 16)
        layout = int(match.group("layout"), 16)
        frame_count = info >> 16
        angle_width = (info >> 8) & 0xff
        frame_bits = layout & 0xffff

        if angle_width <= 0 or angle_width > 16:
            raise ValueError(f"ANIM_DATA_{name} has invalid angle width {angle_width}")

        if frame_bits == 0 or frame_bits % 8 != 0:
            raise ValueError(f"ANIM_DATA_{name} has invalid frame size {frame_bits} bits")

        channel_count = frame_bits // angle_width
        unused_bits = frame_bits - channel_count * angle_width

        if channel_count == 0 or unused_bits >= 8:
            raise ValueError(f"ANIM_DATA_{name} has an invalid channel layout")

        frame_size = frame_bits // 8
        expected_size = frame_count * frame_size
        source = entry_data[name]

        if len(source) < expected_size:
            raise ValueError(
                f"ANIM_ENTRY_{name} is {len(source)} bytes; expected at least {expected_size}"
            )

        padding = source[expected_size:]

        if len(padding) > 3 or any(padding):
            raise ValueError(f"ANIM_ENTRY_{name} has unexpected trailing data")

        offsets[name] = len(output)
        layouts[name] = (frame_count, angle_width, frame_bits, channel_count)

        for frame_index in range(frame_count):
            frame_start = frame_index * frame_size
            frame = source[frame_start:frame_start + frame_size]

            for channel_index in range(channel_count):
                value = read_packed_value(frame, channel_index * angle_width, angle_width)
                output.extend(struct.pack(">H", value << (16 - angle_width)))

        raw_info = (info & 0xffff00ff) | (16 << 8)
        raw_layout = (layout & 0xffff0000) | (channel_count * 16)

        return (
            f"{match.group('prefix')}0x{raw_info:08x}"
            f"{match.group('middle')}0x{raw_layout:08x}"
        )

    raw_data_source, replacement_count = DATA_HEADER_RE.subn(replace_data_header, data_source)

    if replacement_count != len(entries):
        raise ValueError(
            f"found {len(entries)} animation entries but {replacement_count} data headers"
        )

    missing_layouts = [name for name, _ in entries if name not in layouts]

    if missing_layouts:
        raise ValueError(f"missing animation metadata for: {', '.join(missing_layouts)}")

    raw_header = header_source

    for name, _ in entries:
        pattern = re.compile(
            rf"^(#define\s+PTR_ANIM_ENTRY_{re.escape(name)}\s+)0x[0-9a-fA-F]+\s*$",
            re.MULTILINE,
        )
        raw_header, count = pattern.subn(
            rf"\g<1>0x{offsets[name]:x}",
            raw_header,
        )

        if count != 1:
            raise ValueError(f"could not update PTR_ANIM_ENTRY_{name}")

    return bytes(output), raw_header, raw_data_source


def write_output(path: Path, data: bytes | str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")

    if isinstance(data, str):
        temporary.write_text(data)
    else:
        temporary.write_bytes(data)

    temporary.replace(path)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Expand GoldenEye animation rotation frames to big-endian 16-bit angles."
    )
    parser.add_argument("entries_source", type=Path)
    parser.add_argument("data_source", type=Path)
    parser.add_argument("header_source", type=Path)
    parser.add_argument("output_entries", type=Path)
    parser.add_argument("output_header", type=Path)
    parser.add_argument("output_data_source", type=Path)
    args = parser.parse_args()

    entries, header, data_source = convert(
        args.entries_source.read_text(),
        args.data_source.read_text(),
        args.header_source.read_text(),
    )

    write_output(args.output_entries, entries)
    write_output(args.output_header, header)
    write_output(args.output_data_source, data_source)


if __name__ == "__main__":
    main()
