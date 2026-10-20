/* bench: times GME_Render of the first N seconds of each file at 22050 and 11025 Hz.
   usage: bench <cpu-label> <out.tsv> <seconds> <file>...
   Client of gme.library (links no GME code). Clock: timer.device ReadEClock.
   Output (tab separated): cpu file rate audio_ms wall_ms pct (pct = wall/audio*100, < 100 = faster than real time). */
const char bench_ver[] = "$VER: bench 1.0 (19.10.2026)";
#include <exec/types.h>
#include <exec/memory.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
#include <libraries/gme.h>
#include <proto/gme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Library *GmeBase;
struct Device *TimerBase;
static struct GMEInfo pinfo;	/* static: 4 KB, keep the stack small */
static UBYTE blk[8192];

static unsigned long long eclock(unsigned long *freq)
{
	struct EClockVal ev;
	*freq = ReadEClock(&ev);
	return ((unsigned long long) ev.ev_hi << 32) | ev.ev_lo;
}

static unsigned char *slurp(const char *path, long *len)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;
	long n;
	if (!f) return NULL;
	fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
	b = (unsigned char *) AllocVec(n > 0 ? n : 1, MEMF_PUBLIC);
	if (b && fread(b, 1, n, f) != (size_t) n) { FreeVec(b); b = NULL; }
	fclose(f);
	*len = n;
	return b;
}

/* returns 0 ok */
static int run(FILE *tsv, const char *cpu, const char *path, const unsigned char *mod, long len, long rate, long seconds)
{
	struct GMEHandle *h = GME_Open((CONST UBYTE *) mod, len, (CONST_STRPTR) path);
	unsigned long freq;
	unsigned long long t0, t1, want, got = 0, wall_ms;
	long audio_ms, pct, frame, n;
	if (!h) { printf("%s open failed\n", path); return 1; }
	GME_GetInfo(h, &pinfo);
	frame = pinfo.channels * 1;	/* S8 */
	if (GME_SetSampleRate(h, rate) != 0 || GME_SetSong(h, pinfo.defaultSong, -1) != 0) {
		printf("%s setup failed\n", path); GME_Close(h); return 1;
	}
	want = (unsigned long long) seconds * rate * frame;
	t0 = eclock(&freq);
	while (got < want) {
		long chunk = (want - got) > sizeof(blk) ? (long) sizeof(blk) : (long) (want - got);
		n = GME_Render(h, blk, chunk, GME_FMT_S8);
		if (n <= 0) break;
		got += n;
	}
	t1 = eclock(&freq);
	wall_ms = (t1 - t0) * 1000ULL / freq;
	audio_ms = (long) (got * 1000ULL / ((unsigned long long) rate * frame));
	pct = audio_ms > 0 ? (long) (wall_ms * 100ULL / (unsigned long long) audio_ms) : -1;
	printf("%s\t%s\t%ld\t%ld\t%lu\t%ld\n", cpu, path, rate, audio_ms, (unsigned long) wall_ms, pct);
	if (tsv) fprintf(tsv, "%s\t%s\t%ld\t%ld\t%lu\t%ld\n", cpu, path, rate, audio_ms, (unsigned long) wall_ms, pct);
	GME_Close(h);
	return 0;
}

int main(int argc, char **argv)
{
	static const long rates[2] = { 22050, 11025 };
	struct MsgPort *mp;
	struct timerequest *tr;
	FILE *tsv;
	long seconds, i, r, len, rc = 0;
	if (argc < 5) { printf("usage: bench <cpu> <out.tsv> <seconds> <file>...\n"); return 5; }
	seconds = atol(argv[3]);
	mp = CreateMsgPort();
	tr = mp ? (struct timerequest *) CreateIORequest(mp, sizeof(*tr)) : NULL;
	if (!tr || OpenDevice((STRPTR) "timer.device", UNIT_MICROHZ, (struct IORequest *) tr, 0)) { printf("timer.device failed\n"); return 20; }
	TimerBase = tr->tr_node.io_Device;
	GmeBase = OpenLibrary((STRPTR) GMENAME, GMELIB_VERSION);
	if (!GmeBase) { printf("no gme.library\n"); CloseDevice((struct IORequest *) tr); return 20; }
	tsv = fopen(argv[2], "w");
	if (tsv) fprintf(tsv, "cpu\tfile\trate\taudio_ms\twall_ms\tpct\n");
	for (i = 4; i < argc; i++) {
		unsigned char *mod = slurp(argv[i], &len);
		if (!mod) { printf("%s unreadable\n", argv[i]); rc = 10; continue; }
		for (r = 0; r < 2; r++)
			if (run(tsv, argv[1], argv[i], mod, len, rates[r], seconds)) rc = 10;
		FreeVec(mod);
		if (tsv) fflush(tsv);
	}
	if (tsv) fclose(tsv);
	CloseLibrary(GmeBase);
	CloseDevice((struct IORequest *) tr);
	DeleteIORequest((struct IORequest *) tr);
	DeleteMsgPort(mp);
	return rc;
}
