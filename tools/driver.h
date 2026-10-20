/* Shared command-line driver for vgm2raw (Amiga, via gme.library) and
   render-ref (host, links Game_Music_Emu directly). Both define the be_* backend
   before including this file, so the call sequence and the printed text are
   identical and results can be compared byte for byte. All file and console I/O
   uses POSIX open/read/write: libnix stdio output is corrupt under vamos.

   usage: PROG [-i] [-o] [-O n] [-p seek_ms] [-s before_ms seek_ms after_ms] [-m mask] in [out] [song] [ms] [rate] [fmt]
     -i       print module info (no output file)
     -o       print isour=0|1 (IsOurFile with the module bytes), nothing else
     -O n     as -o, but pass only the first n bytes (a player's detection probe)
     -p t     SetSong, render one block, seek to t ms, print pos= (GetPosition), render a block, print pos2=
     -s a b c render a ms, seek to b ms, render c ms more
     -m mask  GME_MuteChannels(mask) after SetSong
     fmt      s16 (default) | u8 | s8
   song < 0 = default song; ms = SetSong duration (default 10000, < 0 = unlimited). */
#include "driver_types.h"

static int be_open(const unsigned char *module, int len, const char *name);
static int be_isour(const unsigned char *module, int len, const char *name);
static void be_info(struct DInfo *info);
static int be_setrate(int hz);
static int be_setsong(int song, int ms);
static int be_render(unsigned char *buf, int bytes, int fmt);
static int be_seek(int ms);
static int be_pos(void);
static void be_mute(int mask);
static void be_close(void);
static void be_stats(void);

static void print_info(const struct DInfo *i)
{
	int s;
	SAYF("title=%s\nauthor=%s\ndate=%s\nsystem=%s\ngame=%s\ncopyright=%s\n", i->title, i->author, i->date, i->system, i->game, i->copyright);
	SAYF("songs=%d default=%d channels=%d ntsc=%d\n", i->songs, i->defaultSong, i->channels, i->ntsc);
	for (s = 0; s < i->songs && s < D_MAXSONGS; s++)
		SAYF("song %d duration=%d loop=%d\n", s, i->duration[s], i->loop[s]);
}

static unsigned char drv_buf[8192];
#define DRV_BLOCK_MS 47		/* one 8192-byte S16 stereo block at 44100 Hz is 46.4 ms */

static int drv_render_ms(int out, long ms, int rate, int frame, int fmt)
{
	long want = (ms * (long) rate / 1000) * frame;
	while (want > 0) {
		int chunk = want > (long) sizeof(drv_buf) ? (int) sizeof(drv_buf) : (int) want;
		int n = be_render(drv_buf, chunk, fmt);
		if (n <= 0)
			break;
		if (out >= 0 && write(out, drv_buf, (size_t) n) != n)
			return -1;
		want -= n;
	}
	return 0;
}

int main(int argc, char **argv)
{
	int info_mode = 0, seek_mode = 0, isour_mode = 0, pos_mode = 0, mute = 0, probe = 0, a = 1;
	long sb = 0, sk = 0, sa = 0, pt = 0;
	const char *pos[6] = {0, 0, 0, 0, 0, 0};
	int np = 0, song, ms, rate, fmt = FMT_S16BE, len, frame, f, out = -1;
	unsigned char *module;
	long flen;
	struct DInfo info;

	while (a < argc && argv[a][0] == '-' && argv[a][1] != '\0' && np == 0) {
		if (!strcmp(argv[a], "-i")) { info_mode = 1; a++; }
		else if (!strcmp(argv[a], "-o")) { isour_mode = 1; a++; }
		else if (!strcmp(argv[a], "-O") && a + 1 < argc) { isour_mode = 1; probe = atoi(argv[a + 1]); a += 2; }
		else if (!strcmp(argv[a], "-p") && a + 1 < argc) { pos_mode = 1; pt = atol(argv[a + 1]); a += 2; }
		else if (!strcmp(argv[a], "-s") && a + 3 < argc) {
			seek_mode = 1; sb = atol(argv[a + 1]); sk = atol(argv[a + 2]); sa = atol(argv[a + 3]); a += 4;
		} else if (!strcmp(argv[a], "-m") && a + 1 < argc) { mute = atoi(argv[a + 1]); a += 2; }
		else break;
	}
	while (a < argc && np < 6)
		pos[np++] = argv[a++];
	if (np < 1 || (!info_mode && !isour_mode && !pos_mode && np < 2)) {
		err("usage: prog [-i] [-o] [-O n] [-p t] [-s a b c] [-m mask] in [out] [song] [ms] [rate] [fmt]\n");
		return 20;
	}
	song = np > 2 ? atoi(pos[2]) : -1;
	ms = np > 3 ? atoi(pos[3]) : 10000;
	rate = np > 4 ? atoi(pos[4]) : 44100;
	if (np > 5) {
		if (!strcmp(pos[5], "s8")) fmt = FMT_S8;
		else if (!strcmp(pos[5], "u8")) fmt = FMT_U8;
		else if (strcmp(pos[5], "s16")) { err("bad fmt\n"); return 20; }
	}
	f = open(pos[0], O_RDONLY);
	if (f < 0) { err("cannot open input\n"); return 20; }
	flen = (long) lseek(f, 0, SEEK_END);
	lseek(f, 0, SEEK_SET);
	module = (unsigned char *) malloc((size_t) flen + 1);
	if (module == NULL) { err("no memory\n"); return 20; }
	len = 0;
	while (len < flen) {
		int n = (int) read(f, module + len, (size_t) (flen - len));
		if (n <= 0) break;
		len += n;
	}
	close(f);
	if (isour_mode) {
		SAYF("isour=%d\n", be_isour(module, probe > 0 && probe < len ? probe : len, pos[0]));
		return 0;
	}
	if (!be_open(module, len, pos[0])) { err("open failed\n"); return 20; }
	be_info(&info);
	if (info_mode) {
		print_info(&info);
		be_close();
		return 0;
	}
	if (be_setrate(rate) != 0) { err("bad rate\n"); return 20; }
	if (song < 0) song = info.defaultSong;
	if (be_setsong(song, ms) != 0) { err("setsong failed\n"); return 20; }
	if (mute) be_mute(mute);
	frame = info.channels * (fmt == FMT_S16BE ? 2 : 1);
	if (pos_mode) {
		drv_render_ms(-1, DRV_BLOCK_MS, rate, frame, fmt);
		if (be_seek((int) pt) != 0) { err("seek failed\n"); return 20; }
		SAYF("pos=%d\n", be_pos());
		drv_render_ms(-1, DRV_BLOCK_MS, rate, frame, fmt);
		SAYF("pos2=%d\n", be_pos());
		be_stats();
		be_close();
		return 0;
	}
	out = open(pos[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) { err("cannot write output\n"); return 20; }
	if (seek_mode) {
		drv_render_ms(out, sb, rate, frame, fmt);
		if (be_seek((int) sk) != 0) { err("seek failed\n"); return 20; }
		drv_render_ms(out, sa, rate, frame, fmt);
	} else {
		drv_render_ms(out, (long) (ms < 0 ? 10000 : ms) + 1000, rate, frame, fmt);	/* stops at song end */
	}
	close(out);
	be_stats();
	be_close();
	return 0;
}
