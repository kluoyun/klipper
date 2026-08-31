#!/usr/bin/env python3
# Build an ESP32-S3 application image.
#
# Copyright (C) 2026  Xiaokui Zhao <xiaok@zxkxz.cn>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

import argparse
import hashlib
import struct


def read_entrypoint(filename):
    with open(filename, "rb") as elf_file:
        header = elf_file.read(52)
    if header[:6] != b"\x7fELF\x01\x01":
        raise SystemExit("ESP32-S3 input is not a little-endian ELF32 file")
    return struct.unpack_from("<I", header, 24)[0]


def read_segment(filename):
    with open(filename, "rb") as segment_file:
        data = segment_file.read()
    if len(data) & 3:
        data += b"\x00" * (-len(data) & 3)
    return data


def build_app_descriptor(elf_filename):
    descriptor = bytearray(256)
    struct.pack_into("<I", descriptor, 0, 0xABCD5432)

    def put_string(offset, size, value):
        encoded = value.encode("ascii")[:size - 1]
        descriptor[offset:offset + len(encoded)] = encoded

    put_string(16, 32, "klipper")
    put_string(48, 32, "klipper")
    put_string(112, 32, "bare-metal")
    with open(elf_filename, "rb") as elf_file:
        descriptor[144:176] = hashlib.sha256(elf_file.read()).digest()

    # Accept all ESP32-S3 eFuse revisions and use 64KiB MMU pages.
    struct.pack_into("<HHB", descriptor, 176, 0, 199, 16)
    return bytes(descriptor)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--iram", required=True)
    parser.add_argument("--dram", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--flash-size-code", required=True,
                        type=lambda value: int(value, 0))
    args = parser.parse_args()

    entrypoint = read_entrypoint(args.elf)
    segments = [
        # App descriptor with the page offset used at flash address 0x10000.
        (0x3C000020, build_app_descriptor(args.elf)),
        # Keep a minimal IROM segment for the second-stage bootloader.
        (0x42000128, b"\x00" * 4),
        (0x3FCA0000, read_segment(args.dram)),
        (0x40370000, read_segment(args.iram)),
    ]
    if any(not data for _, data in segments):
        raise SystemExit("ESP32-S3 image contains an empty load segment")

    image = bytearray()
    # Common ESP image header.
    image.extend(struct.pack("<BBBBI", 0xE9, len(segments), 2,
                             args.flash_size_code, entrypoint))
    # Extended header for chip ID 9 (ESP32-S3), with a SHA-256 trailer.
    image.extend(struct.pack("<BBBBHBHHBBBBB", 0xEE, 0, 0, 0, 9, 0,
                             0, 99, 0, 0, 0, 0, 1))

    checksum = 0xEF
    for address, data in segments:
        image.extend(struct.pack("<II", address, len(data)))
        image.extend(data)
        for value in data:
            checksum ^= value

    image.extend(b"\x00" * ((15 - len(image)) & 15))
    image.append(checksum)
    image.extend(hashlib.sha256(image).digest())
    with open(args.output, "wb") as output_file:
        output_file.write(image)


if __name__ == "__main__":
    main()
