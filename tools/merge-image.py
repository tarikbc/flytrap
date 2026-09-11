#!/usr/bin/env python3
"""Merge ESP32 flash images into one image flashable at 0x0.

Usage: merge-image.py OUT.bin OFF1 IMG1 [OFF2 IMG2 ...]

Places each IMG at its flash OFFset (hex or decimal) in a single output image,
filling the gaps between them with 0xFF (erased-flash value). For a single chip
this is byte-identical to `esptool merge_bin`, but needs no esptool on PATH --
which matters in CI, where installing the 3.x core for the C5 removes the 2.0.17
core's bundled esptool. The C5's bootloader sits at 0x2000, not 0x1000; pass the
right offsets per board (see each board's flash_*.txt / the release workflow).
"""
import sys


def main(argv):
    if len(argv) < 4 or len(argv) % 2 != 0:
        sys.exit("usage: merge-image.py OUT.bin OFF1 IMG1 [OFF2 IMG2 ...]")
    out, pairs = argv[1], argv[2:]
    segs = []
    for i in range(0, len(pairs), 2):
        off = int(pairs[i], 0)
        with open(pairs[i + 1], "rb") as f:
            segs.append((off, f.read()))
    size = max(off + len(data) for off, data in segs)
    img = bytearray(b"\xff" * size)
    for off, data in segs:
        img[off:off + len(data)] = data
    with open(out, "wb") as f:
        f.write(img)
    print(f"merged {out} ({len(img)} bytes) from {len(segs)} images")


if __name__ == "__main__":
    main(sys.argv)
