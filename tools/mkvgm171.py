#!/usr/bin/env python3
"""Rewrite a VGM/VGZ (version >= 0x150) as a VGM 1.71 .vgz: header extended to 0x100
bytes, new chip/field slots zero, version 0x171, command stream and GD3 copied
byte-for-byte. Deterministic (gzip mtime 0). Exists because no real-world 1.71 file
with only SN76489/YM2413/YM2612 exists on vgmrips (scan 2026-10-19).
usage: mkvgm171.py in.vgm|in.vgz out.vgz"""
import gzip, struct, sys, zlib

def main(src, dst):
    d = open(src, "rb").read()
    if d[:2] == b"\x1f\x8b":
        d = gzip.decompress(d)
    if d[:4] != b"Vgm ":
        sys.exit("not a VGM file")
    ver = struct.unpack_from("<I", d, 8)[0]
    if ver < 0x150:
        sys.exit("version < 1.50")
    off = struct.unpack_from("<I", d, 0x34)[0]
    start = 0x34 + off
    delta = 0x100 - start
    if delta < 0:
        sys.exit("header already longer than 0x100")
    h = bytearray(0x100)
    h[0:start] = d[0:start]
    struct.pack_into("<I", h, 0x08, 0x171)
    struct.pack_into("<I", h, 0x34, 0x100 - 0x34)
    for field, rel in ((0x04, 0x04), (0x14, 0x14), (0x1C, 0x1C)):
        v = struct.unpack_from("<I", h, field)[0]
        if v or field == 0x04:
            struct.pack_into("<I", h, field, v + delta)
    out = bytes(h) + d[start:]
    co = zlib.compressobj(9, zlib.DEFLATED, -15)
    body = co.compress(out) + co.flush()
    gz = (b"\x1f\x8b\x08\x00\x00\x00\x00\x00\x02\xff" + body +
          struct.pack("<II", zlib.crc32(out) & 0xFFFFFFFF, len(out) & 0xFFFFFFFF))
    open(dst, "wb").write(gz)

if len(sys.argv) != 3:
    sys.exit(__doc__)
main(sys.argv[1], sys.argv[2])
