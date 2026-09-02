#!/usr/bin/env python3
"""Expand a GoldenEye 8-bit RLE image into a raw, DMA-ready byte array."""

import argparse
import struct
from pathlib import Path


def align(data, alignment):
    data.extend(b"\0" * (-len(data) % alignment))


def expand_rle_image(source):
    if len(source) < 10:
        raise ValueError("RLE image is missing its header")

    width, height = struct.unpack_from(">HH", source)
    expected_size = width * height

    if expected_size != 440 * 299:
        raise ValueError(f"expected a 440x299 image, found {width}x{height}")

    result = bytearray()
    offset = 10

    while len(result) < expected_size:
        if offset + 2 > len(source):
            raise ValueError("RLE image ended before all pixels were decoded")

        count = source[offset]
        value = source[offset + 1]
        offset += 2

        if count == 0 or len(result) + count > expected_size:
            raise ValueError("RLE image contains an invalid run length")

        result.extend(bytes((value,)) * count)

    align(result, 64)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    result = expand_rle_image(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(result)


if __name__ == "__main__":
    main()
