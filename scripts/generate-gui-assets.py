#!/usr/bin/env python3
"""Generate deterministic binary GUI samples from source-controlled PPM data."""
import pathlib
import struct
import sys

source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
tokens = []
for line in source.read_text(encoding="ascii").splitlines():
    tokens.extend(line.split("#", 1)[0].split())
if len(tokens) < 4 or tokens[0] != "P3":
    raise SystemExit("GUI asset source must be a P3 PPM")
width, height, maximum = map(int, tokens[1:4])
samples = list(map(int, tokens[4:]))
if maximum != 255 or len(samples) != width * height * 3:
    raise SystemExit("invalid GUI asset sample")
stride = (width * 3 + 3) & ~3
pixels = bytearray()
for y in range(height - 1, -1, -1):
    row = samples[y * width * 3:(y + 1) * width * 3]
    for x in range(width):
        red, green, blue = row[x * 3:x * 3 + 3]
        pixels.extend((blue, green, red))
    pixels.extend(b"\0" * (stride - width * 3))
header = b"BM" + struct.pack("<IHHI", 54 + len(pixels), 0, 0, 54)
dib = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0,
                  len(pixels), 2835, 2835, 0, 0)
output.parent.mkdir(parents=True, exist_ok=True)
output.write_bytes(header + dib + pixels)
