#!/usr/bin/env python3
"""Write corpus/vgm-ay-only.vgm: a VGM 1.51 with only an AY8910 (no SN76489,
YM2413 or YM2612). GME has no AY8910 for VGM, so the library must refuse it.
usage: mk-ay-only-vgm.py [outfile]"""
import struct, sys
out = sys.argv[1] if len(sys.argv) > 1 else "corpus/vgm-ay-only.vgm"
body = bytes([0xA0,0x00,0x40, 0xA0,0x01,0x00, 0xA0,0x07,0x3E, 0xA0,0x08,0x0F]) + bytes([0x62]) * 60 + bytes([0x66])
h = bytearray(0x100)
h[0:4] = b"Vgm "
struct.pack_into("<I", h, 0x04, 0x100 + len(body) - 4)
struct.pack_into("<I", h, 0x08, 0x151)
struct.pack_into("<I", h, 0x18, 44100)
struct.pack_into("<I", h, 0x34, 0xCC)
struct.pack_into("<I", h, 0x74, 1789773)
open(out, "wb").write(bytes(h) + body)
