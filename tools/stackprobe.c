/* stackprobe: measures caller-stack bytes used by each gme.library LVO.
   Paints the stack below the caller with a pattern, calls the LVO, scans for the
   lowest overwritten byte. One line: stack open=N getinfo=N setsong=N render=N seek=N close=N
   (maximum over the calls made). Floor of the measurement is MARGIN bytes below the
   probe's own frame, so values below MARGIN mean "at most about MARGIN".
   usage: stackprobe file   (run with a generous vamos stack, e.g. -s 64) */
#include <exec/types.h>
#include <exec/tasks.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <libraries/gme.h>
#include <proto/gme.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

struct Library *GmeBase;

#define STKSIZE 400000
#define MARGIN 192
#define PAT 0xA5

enum { S_OPEN, S_INFO, S_SONG, S_RENDER, S_SEEK, S_CLOSE, S_N };
static const char *const names[S_N] = { "open", "getinfo", "setsong", "render", "seek", "close" };
static unsigned char *module;
static unsigned char buf[8192];
static char line[256];
static void say(const char *s) { if (write(1, s, strlen(s)) < 0) {} }
#define SAYF(...) do { snprintf(line, sizeof(line), __VA_ARGS__); say(line); } while (0)
static struct GMEHandle *h;
static struct GMEInfo pinfo;
static long used[S_N];
static unsigned char *stkbuf;
static struct StackSwapStruct sss;
static int rc;
static long mlen;
static const char *mname;
static long SONG = -1;

static void run(int which)
{
	volatile unsigned char marker;
	unsigned char *top = (unsigned char *) &marker - MARGIN;
	unsigned char *lo = stkbuf;
	unsigned char *p;
	long u;
	for (p = lo; p < top; p++) *p = PAT;
	switch (which) {
	case S_OPEN: h = GME_Open((CONST UBYTE *) module, (LONG) mlen, (CONST_STRPTR) mname); break;
	case S_INFO: GME_GetInfo(h, &pinfo); break;
	case S_SONG: GME_SetSong(h, SONG, 10000); break;
	case S_RENDER: GME_Render(h, buf, sizeof(buf), GME_FMT_S16BE); break;
	case S_SEEK: GME_Seek(h, 5000); break;
	case S_CLOSE: GME_Close(h); break;
	}
	for (p = lo; p < top && *p == PAT; p++) ;
	u = (p >= top) ? 0 : ((long) ((unsigned char *) &marker - p));
	if (u > used[which]) used[which] = u;
}

static void probe(void)
{
	int i;
	run(S_OPEN);
	if (h == NULL) { say("open failed\n"); rc = 20; return; }
	run(S_INFO);
	GME_SetSampleRate(h, 44100);
	run(S_SONG);
	for (i = 0; i < 4; i++) run(S_RENDER);
	run(S_SEEK);
	for (i = 0; i < 2; i++) run(S_RENDER);
	run(S_CLOSE);
	say("stack");
	for (i = 0; i < S_N; i++) SAYF(" %s=%ld", names[i], used[i]);
	say("\n");
}

/* All state static: after StackSwap the old frame is not addressable via sp. */
static void swapped(void)
{
	sss.stk_Lower = stkbuf;
	sss.stk_Upper = (ULONG) (stkbuf + STKSIZE);
	sss.stk_Pointer = (APTR) (stkbuf + STKSIZE);
	StackSwap(&sss);
	probe();
	StackSwap(&sss);
}

int main(int argc, char **argv)
{
	int f;
	if (argc < 2) return 20;
	f = open(argv[1], O_RDONLY);
	if (f < 0) return 20;
	mlen = (long) lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);
	module = (unsigned char *) malloc((size_t) mlen + 1);
	if (module == NULL || read(f, module, (size_t) mlen) != mlen) return 20;
	close(f);
	mname = argv[1];
	GmeBase = OpenLibrary(GMENAME, GMELIB_VERSION);
	if (GmeBase == NULL) return 20;
	stkbuf = (unsigned char *) AllocMem(STKSIZE, MEMF_PUBLIC | MEMF_CLEAR);
	if (stkbuf == NULL) return 20;
	swapped();
	FreeMem(stkbuf, STKSIZE);
	CloseLibrary(GmeBase);
	return rc;
}
