/* EZPK-packed GYMX: bzip2 (patched magics, see src/bzip2/bzlib_private.h) around de-interleaved
   GYM streams. Our own implementation from the format notes in thoughts/spot/gymx-packed.md and
   tools/unezpk.py. Plain C, no OS calls: shared by the library and the host tools. */
#ifndef GMELIB_EZPK_H
#define GMELIB_EZPK_H

#ifdef __cplusplus
extern "C" {
#endif

#define EZPK_GYMX_HEADER 428		/* GYMX header; the unpacked size is the LE32 at 424 */
#define EZPK_MAX_GYM (16L * 1024 * 1024)	/* bound on the unpacked GYM stream */

/* Size of the unpacked file (428-byte header + GYM stream) when m is a GYMX whose packed field is
   non-zero, followed by "EZPK", and whose stated size is within EZPK_MAX_GYM; else -1. */
long ezpk_unpacked_size(const unsigned char *m, long length);

/* Unpack into out (outsize == ezpk_unpacked_size). out gets the 424 header bytes, a zero packed
   field, then the GYM stream. Returns 0, or a negative error: -1 bad stream or CRC, -2 wrong size or
   inconsistent sections, -3 out of memory. out is not meaningful after an error. */
int ezpk_unpack(const unsigned char *m, long length, unsigned char *out, long outsize);

/* Peak bytes allocated by the last ezpk_unpack (decoder state plus the section buffer; not out). */
extern unsigned long ezpk_peak_alloc;

#ifdef __cplusplus
}
#endif

#endif
