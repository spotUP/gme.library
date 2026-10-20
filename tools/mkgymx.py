#!/usr/bin/env python3
"""Wrap a raw (headerless) GYM in an unpacked GYMX header (packed field 0).
usage: mkgymx.py raw.gym out.gym
The corpus has packed GYMX files only in the EZPK scheme GME cannot read; this derives
the accepted counterpart, so identify and Open are tested on a GYMX header both ways."""
import sys
raw = open(sys.argv[1], "rb").read()
hdr = b"GYMX" + b"Vectorman 2 Lose".ljust(32, b"\0") + b"\0" * (32 * 4 + 256) + b"\0" * 8
assert len(hdr) == 428
open(sys.argv[2], "wb").write(hdr + raw)
