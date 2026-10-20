/* EZPK unpack. Layout (all little-endian), derived from the format notes and tools/unezpk.py:
     GYMX header (428 bytes, u32 at 424 = unpacked GYM size N), "EZPK", u32 L, stream.
   The stream is bzip2 with patched magics and decompresses to N+0x24 bytes: a 0x24-byte header of
   u32 section lengths, then the sections type, reg, datm, dats, sng, dacm, dacs. The GYM is rebuilt
   by re-interleaving them. An optional DAC block (header u32 at 0x20, length at 0x14) is itself
   "EZPK" + u32 + bzip2 (top nibble of the u32 zero) or zlib and is expanded in place first. */
#include <stdlib.h>
#include <string.h>
#include "ezpk.h"
#include "bzip2/bzlib.h"
#include "zlib.h"

unsigned long ezpk_peak_alloc;

static unsigned long ezpk_cur;

static unsigned long rd32(const unsigned char *p)
{
	return (unsigned long) p[0] | (unsigned long) p[1] << 8 | (unsigned long) p[2] << 16 | (unsigned long) p[3] << 24;
}

/* libbzip2 requires this symbol (BZ_NO_STDIO): internal consistency failure, never expected. */
void bz_internal_error(int errcode)
{
	(void) errcode;
	abort();
}

/* Counted allocation: an 8-byte prefix keeps the size so frees can be subtracted. The
   library's malloc is AllocVec (mem_glue.cpp); on the host it is libc. */
static void *cnt_alloc(unsigned long n)
{
	unsigned char *p = (unsigned char *) malloc(n + 8);
	if (p == NULL)
		return NULL;
	memcpy(p, &n, sizeof n);
	ezpk_cur += n;
	if (ezpk_cur > ezpk_peak_alloc)
		ezpk_peak_alloc = ezpk_cur;
	return p + 8;
}

static void cnt_free(void *q)
{
	unsigned char *p = (unsigned char *) q - 8;
	unsigned long n;
	if (q == NULL)
		return;
	memcpy(&n, p, sizeof n);
	ezpk_cur -= n;
	free(p);
}

static void *bz_alloc(void *opaque, int items, int size)
{
	(void) opaque;
	return cnt_alloc((unsigned long) items * (unsigned long) size);
}

static void bz_free(void *opaque, void *p)
{
	(void) opaque;
	cnt_free(p);
}

static void *z_alloc(void *opaque, unsigned items, unsigned size)
{
	(void) opaque;
	return cnt_alloc((unsigned long) items * size);
}

static void z_free(void *opaque, void *p)
{
	(void) opaque;
	cnt_free(p);
}

/* Decompress src (the stream after "EZPK"+u32) into dst; *outlen gets the byte count. kind is the
   u32's top nibble: 0 = bzip2 variant, else zlib. Output beyond cap is an error. */
static int inflateStream(int kind, const unsigned char *src, unsigned long srclen, unsigned char *dst, unsigned long cap, unsigned long *outlen)
{
	if (kind == 0) {
		bz_stream s;
		int r;
		memset(&s, 0, sizeof s);
		s.bzalloc = bz_alloc;
		s.bzfree = bz_free;
		/* small=1: ll16/ll4 BWT inverse, 2.5 bytes per block byte instead of 4, so a '2'
		   (200k) block costs ~500 KB, not 800 KB. On a 68k with chip/fast RAM shared with
		   the song this is the difference that matters; speed is irrelevant for a ~13 KB
		   stream decoded once at Open. */
		if (BZ2_bzDecompressInit(&s, 0, 1) != BZ_OK)
			return -3;
		s.next_in = (char *) src;
		s.avail_in = (unsigned int) srclen;
		s.next_out = (char *) dst;
		s.avail_out = (unsigned int) cap;
		r = BZ2_bzDecompress(&s);
		*outlen = cap - s.avail_out;
		BZ2_bzDecompressEnd(&s);
		return r == BZ_STREAM_END ? 0 : (r == BZ_MEM_ERROR ? -3 : -1);
	} else {
		z_stream z;
		int r;
		memset(&z, 0, sizeof z);
		z.zalloc = z_alloc;
		z.zfree = z_free;
		if (inflateInit(&z) != Z_OK)
			return -3;
		z.next_in = (Bytef *) src;
		z.avail_in = (uInt) srclen;
		z.next_out = dst;
		z.avail_out = (uInt) cap;
		r = inflate(&z, Z_FINISH);
		*outlen = cap - z.avail_out;
		inflateEnd(&z);
		return r == Z_STREAM_END ? 0 : (r == Z_MEM_ERROR ? -3 : -1);
	}
}

long ezpk_unpacked_size(const unsigned char *m, long length)
{
	unsigned long n;
	if (length < EZPK_GYMX_HEADER + 8 || memcmp(m, "GYMX", 4) != 0 || memcmp(m + EZPK_GYMX_HEADER, "EZPK", 4) != 0)
		return -1;
	n = rd32(m + 424);
	if (n == 0 || n > (unsigned long) EZPK_MAX_GYM)
		return -1;
	return EZPK_GYMX_HEADER + (long) n;
}

int ezpk_unpack(const unsigned char *m, long length, unsigned char *out, long outsize)
{
	unsigned long n, cap, got, o, packlen, dacl, p, i;
	unsigned long len[6], pos[7];
	unsigned long cur[7];
	unsigned char *b;
	unsigned char *w;
	int r;
	static const int lenoff[6] = { 0x00, 0x04, 0x08, 0x0c, 0x10, 0x18 };

	if (ezpk_unpacked_size(m, length) != outsize)
		return -2;
	n = (unsigned long) (outsize - EZPK_GYMX_HEADER);
	ezpk_cur = ezpk_peak_alloc = 0;

	/* Every GYM byte comes from one section byte, so the sections total at most N; plus the header. */
	cap = n + 0x24;
	b = (unsigned char *) cnt_alloc(cap);
	if (b == NULL)
		return -3;
	r = inflateStream((int) (rd32(m + EZPK_GYMX_HEADER + 4) >> 28), m + EZPK_GYMX_HEADER + 8,
			(unsigned long) length - (EZPK_GYMX_HEADER + 8), b, cap, &got);
	if (r == 0 && got < 0x24)
		r = -2;
	if (r != 0) {
		cnt_free(b);
		return r;
	}

	dacl = rd32(b + 0x18) + rd32(b + 0x1c);
	if (dacl > 0) {
		/* UNTESTED: neither sample has a DAC block. Expands the inner EZPK block over itself. */
		o = rd32(b + 0x20);
		packlen = rd32(b + 0x14);
		if (o > got || packlen > got - o || packlen < 8 || memcmp(b + o, "EZPK", 4) != 0 || o + dacl > cap) {
			cnt_free(b);
			return -2;
		}
		{
			unsigned char *tmp = (unsigned char *) cnt_alloc(dacl);
			unsigned long dgot = 0;
			if (tmp == NULL) {
				cnt_free(b);
				return -3;
			}
			r = inflateStream((int) (rd32(b + o + 4) >> 28), b + o + 8, packlen - 8, tmp, dacl, &dgot);
			if (r == 0 && dgot != dacl)
				r = -2;
			if (r == 0) {
				memcpy(b + o, tmp, dacl);
				if (o + dacl > got)
					got = o + dacl;
			}
			cnt_free(tmp);
			if (r != 0) {
				cnt_free(b);
				return r;
			}
		}
	}

	/* Section starts: type, reg, datm, dats, sng, dacm, dacs. */
	p = 0x24;
	for (i = 0; i < 6; i++) {
		len[i] = rd32(b + lenoff[i]);
		if (len[i] > got - p) {
			cnt_free(b);
			return -2;
		}
		pos[i] = p;
		cur[i] = p;
		p += len[i];
	}
	pos[6] = p;
	cur[6] = p;
	/* dacs follows dacm; its end is dacm + the u32 at 0x1c. */
	if (rd32(b + 0x1c) > got - p) {
		cnt_free(b);
		return -2;
	}
	{
		/* indices: 0 type, 1 reg, 2 datm, 3 dats, 4 sng, 5 dacm, 6 dacs */
		unsigned long end[7];
		for (i = 0; i < 6; i++)
			end[i] = pos[i] + len[i];
		end[6] = p + rd32(b + 0x1c);

		memcpy(out, m, 424);
		memset(out + 424, 0, 4);
		w = out + EZPK_GYMX_HEADER;
		{
			unsigned char *wend = out + outsize;
#define NEXT(k) (cur[k] < end[k] ? b[cur[k]++] : (r = -2, 0))
			r = 0;
			while (w < wend && r == 0) {
				unsigned char t = NEXT(0);
				if (r != 0)
					break;
				*w++ = t;
				if (t == 1 || t == 2) {
					unsigned char reg, val;
					if (w + 2 > wend) {
						r = -2;
						break;
					}
					reg = NEXT(1);
					*w++ = reg;
					if (reg == 0x2a && dacl > 0)
						val = NEXT(t == 1 ? 5 : 6);
					else
						val = NEXT(t == 1 ? 2 : 3);
					*w++ = val;
				} else if (t == 3) {
					if (w + 1 > wend) {
						r = -2;
						break;
					}
					*w++ = NEXT(4);
				}
			}
#undef NEXT
		}
	}
	cnt_free(b);
	return r;
}
