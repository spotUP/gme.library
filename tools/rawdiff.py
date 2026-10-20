import struct, sys
# usage: rawdiff.py host.raw 68k.raw  -> first differing sample index and max abs diff (S16BE)
a = open(sys.argv[1], "rb").read()
b = open(sys.argv[2], "rb").read() if len(sys.argv) > 2 else b""
n = min(len(a), len(b)) // 2
first = None
mx = 0
for i in range(n):
    x = struct.unpack_from(">h", a, 2 * i)[0]
    y = struct.unpack_from(">h", b, 2 * i)[0]
    if x != y:
        if first is None:
            first = i
        mx = max(mx, abs(x - y))
print("first_diff_sample=%s max_abs_diff=%d len_host=%d len_68k=%d" % (first, mx, len(a), len(b)))
