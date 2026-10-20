/* Host tool: unpack an EZPK GYMX with src/ezpk.c. usage: ezpk-unpack in.gym out.gym
   Prints "<gym bytes> peak <bytes>" (gym bytes = file size minus the 428-byte header). */
#include <stdio.h>
#include <stdlib.h>
#include "../src/ezpk.h"

int main(int argc, char **argv)
{
	FILE *f;
	unsigned char *in, *out;
	long n, usz;
	int r;
	if (argc != 3) {
		fprintf(stderr, "usage: ezpk-unpack in.gym out.gym\n");
		return 2;
	}
	f = fopen(argv[1], "rb");
	if (f == NULL) {
		fprintf(stderr, "cannot open %s\n", argv[1]);
		return 1;
	}
	in = malloc(EZPK_MAX_GYM + 4096);
	n = (long) fread(in, 1, EZPK_MAX_GYM + 4096, f);
	fclose(f);
	usz = ezpk_unpacked_size(in, n);
	if (usz < 0) {
		fprintf(stderr, "not an EZPK GYMX\n");
		return 1;
	}
	out = malloc((size_t) usz);
	r = ezpk_unpack(in, n, out, usz);
	if (r != 0) {
		fprintf(stderr, "ezpk error %d\n", r);
		return 1;
	}
	f = fopen(argv[2], "wb");
	fwrite(out, 1, (size_t) usz, f);
	fclose(f);
	printf("%ld peak %lu\n", usz - EZPK_GYMX_HEADER, ezpk_peak_alloc);
	return 0;
}
