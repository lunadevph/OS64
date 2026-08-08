#!/usr/bin/env python3
"""Generate OS64's GRUB-only fonts and artwork from repository sources."""

import struct
import subprocess
import sys
import zlib
from pathlib import Path


def png(path, width, height, pixel):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        for x in range(width):
            raw.extend(pixel(x, y))

    def chunk(kind, data):
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def selection_assets(output):
    green = (77, 224, 145, 255)
    fill = (25, 57, 68, 238)
    transparent = (0, 0, 0, 0)
    specs = {
        "c": (8, 8), "n": (8, 4), "s": (8, 4), "e": (4, 8), "w": (4, 8),
        "nw": (4, 4), "ne": (4, 4), "sw": (4, 4), "se": (4, 4)}
    for part, (width, height) in specs.items():
        def pixel(x, y, part=part, width=width, height=height):
            edge = (("n" in part and y == 0) or ("s" in part and y == height - 1) or
                    ("w" in part and x == 0) or ("e" in part and x == width - 1))
            if part == "n" and y == 0 or part == "s" and y == height - 1:
                edge = True
            if part == "w" and x == 0 or part == "e" and x == width - 1:
                edge = True
            return green if edge else (fill if part == "c" else transparent)
        png(output / f"select_{part}.png", width, height, pixel)


def background(output):
    def pixel(x, y):
        # Deep navy vertical gradient with a quiet OS64-green horizon and grid.
        glow = max(0, 90 - abs(y - 585))
        r = 8 + y * 7 // 768
        g = 17 + y * 10 // 768 + glow // 16
        b = 28 + y * 14 // 768 + glow // 12
        if y in (116, 117):
            return (58, 117, 103, 255)
        if y > 600 and (x % 64 == 0 or y % 48 == 0):
            return (16, 42, 50, 255)
        return (r, g, b, 255)
    png(output / "background.png", 1024, 768, pixel)


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: generate-grub-theme.py OUTPUT_DIR")
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    font_root = Path("/usr/share/fonts/truetype/dejavu")
    mono = font_root / "DejaVuSansMono.ttf"
    display = font_root / "DejaVuSans-Bold.ttf"
    if not mono.is_file() or not display.is_file():
        raise SystemExit("DejaVu fonts are required to generate the GRUB theme")
    common = ["-r", "0x20-0x7e"]
    subprocess.run(["grub-mkfont", *common, "-s", "14", "-n", "OS64 Mono",
                    "-o", str(output / "os64-mono-14.pf2"), str(mono)], check=True)
    subprocess.run(["grub-mkfont", *common, "-s", "30", "-n", "OS64 Display",
                    "-o", str(output / "os64-display-30.pf2"), str(display)], check=True)
    background(output)
    selection_assets(output)


if __name__ == "__main__":
    main()
