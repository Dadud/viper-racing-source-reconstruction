#!/usr/bin/env python3
"""Extract the engine's 8x8 ASCII glyph table from race.bin."""

from __future__ import annotations

import argparse
import hashlib
import struct
from pathlib import Path


FONT_POINTER_TABLE = 0x004D7848
FIRST_CHARACTER = 0x20
CHARACTER_LIMIT = 0x80
EXPECTED_SHA256 = "f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56"


def read_pe_image(path: Path) -> tuple[bytes, int, list[tuple[int, int, int, int]]]:
    data = path.read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("input is not a PE image")
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_offset : pe_offset + 4] != b"PE\0\0":
        raise ValueError("invalid PE signature")

    coff_offset = pe_offset + 4
    section_count = struct.unpack_from("<H", data, coff_offset + 2)[0]
    optional_size = struct.unpack_from("<H", data, coff_offset + 16)[0]
    optional_offset = coff_offset + 20
    magic = struct.unpack_from("<H", data, optional_offset)[0]
    if magic != 0x10B:
        raise ValueError("expected a 32-bit PE optional header")
    image_base = struct.unpack_from("<I", data, optional_offset + 28)[0]

    sections = []
    section_offset = optional_offset + optional_size
    for index in range(section_count):
        offset = section_offset + index * 40
        virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
            "<IIII", data, offset + 8
        )
        sections.append((virtual_address, virtual_size, raw_size, raw_pointer))
    return data, image_base, sections


def read_va(
    data: bytes,
    image_base: int,
    sections: list[tuple[int, int, int, int]],
    address: int,
    length: int,
) -> bytes:
    rva = address - image_base
    for virtual_address, virtual_size, raw_size, raw_pointer in sections:
        mapped_size = max(virtual_size, raw_size)
        if virtual_address <= rva and rva + length <= virtual_address + mapped_size:
            offset = raw_pointer + rva - virtual_address
            if offset + length <= len(data):
                return data[offset : offset + length]
    raise ValueError(f"unmapped virtual address range {address:#x}+{length:#x}")


def extract_glyphs(
    data: bytes,
    image_base: int,
    sections: list[tuple[int, int, int, int]],
) -> list[list[int]]:
    glyphs = []
    for character in range(FIRST_CHARACTER, CHARACTER_LIMIT):
        pointer_bytes = read_va(
            data, image_base, sections,
            FONT_POINTER_TABLE + character * 4, 4
        )
        glyph_address = struct.unpack("<I", pointer_bytes)[0]
        rows = []
        for row in range(8):
            row_pointer = struct.unpack(
                "<I", read_va(data, image_base, sections, glyph_address + row * 4, 4)
            )[0]
            pattern = read_va(data, image_base, sections, row_pointer, 8)
            if any(pixel not in (ord("."), ord("x")) for pixel in pattern):
                raise ValueError(
                    f"unexpected bitmap bytes for character {character:#x}, row {row}"
                )
            rows.append(sum((pixel == ord("x")) << column
                            for column, pixel in enumerate(pattern)))
        glyphs.append(rows)
    return glyphs


def render_header(glyphs: list[list[int]]) -> str:
    lines = [
        "#pragma once",
        "",
        "#include <array>",
        "#include <cstdint>",
        "",
        "namespace viper::detail {",
        "",
        "// Extracted from race.bin at 0x4D7848; indexed by ASCII byte minus 0x20.",
        "inline constexpr std::array<std::array<std::uint8_t, 8>, 96>",
        "    scene_font_8x8{{",
    ]
    for rows in glyphs:
        masks = ", ".join(f"0x{mask:02X}" for mask in rows)
        lines.append(f"        {{{{{masks}}}}},")
    lines += ["    }};", "", "} // namespace viper::detail", ""]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to the extracted race.bin")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "include/viper/scene_font_8x8.hpp",
        help="generated C++ header path",
    )
    args = parser.parse_args()

    digest = hashlib.sha256(args.binary.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected race.bin SHA-256: {digest}")
    data, image_base, sections = read_pe_image(args.binary)
    glyphs = extract_glyphs(data, image_base, sections)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render_header(glyphs), encoding="utf-8", newline="\n")
    print(f"wrote {len(glyphs)} glyphs to {args.output}")


if __name__ == "__main__":
    main()
