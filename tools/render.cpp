// Host reference renderer: gme.h C API only. Raw output is S16BE stereo.
#include "gme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include "../src/ezpk.h"
// File I/O uses POSIX open/read/write: libnix stdio output is corrupt under vamos (measured: puts() writes 64K garbage).

static bool load(const char* path, std::vector<char>& buf)
{
	int f = open(path, O_RDONLY);
	if (f < 0) { fprintf(stderr, "cannot open %s\n", path); return false; }
	static char tmp[65536];
	long n;
	while ((n = read(f, tmp, sizeof tmp)) > 0) buf.insert(buf.end(), tmp, tmp + n);
	close(f);
	// Packed GYMX (EZPK): unpack like GME_Open does, so GME sees the plain file.
	long usz = ezpk_unpacked_size((const unsigned char*)buf.data(), (long)buf.size());
	if (usz > 0) {
		std::vector<char> out((size_t)usz);
		int r = ezpk_unpack((const unsigned char*)buf.data(), (long)buf.size(), (unsigned char*)out.data(), usz);
		if (r) { fprintf(stderr, "ezpk error %d in %s\n", r, path); return false; }
		buf.swap(out);
	}
	return true;
}

static int check(gme_err_t e)
{
	if (e) { fprintf(stderr, "gme error: %s\n", e); return 1; }
	return 0;
}

// Header magic first (gme_open_data); the extension only for files without
// one (raw GYM), the same order as GME_Open and render-ref.
static gme_err_t openEmu(const char* path, const std::vector<char>& buf, Music_Emu** emu, int rate)
{
	gme_err_t e = gme_open_data(buf.data(), (long)buf.size(), emu, rate);
	gme_type_t t = gme_identify_extension(path);
	if (!e || !t || gme_identify_extension(gme_identify_header(buf.data()))) return e;
	*emu = gme_new_emu(t, rate);
	if (!*emu) return "out of memory";
	e = gme_load_data(*emu, buf.data(), (long)buf.size());
	if (e) { gme_delete(*emu); *emu = 0; }
	return e;
}

static int info(const char* path)
{
	std::vector<char> buf;
	if (!load(path, buf)) return 1;
	Music_Emu* emu = 0;
	if (check(openEmu(path, buf, &emu, 44100))) return 1;
	int n = gme_track_count(emu);
	printf("tracks %d\n", n);
	for (int i = 0; i < n; i++) {
		gme_info_t* t = 0;
		if (check(gme_track_info(emu, &t, i))) { gme_delete(emu); return 1; }
		if (i == 0) {
			printf("system %s\ngame %s\nsong %s\nauthor %s\ncopyright %s\n",
				t->system, t->game, t->song, t->author, t->copyright);
		}
		printf("track %d play_length %d length %d intro %d loop %d derived %d\n",
			i, t->play_length, t->length, t->intro_length, t->loop_length, t->length <= 0 ? 1 : 0);
		gme_free_info(t);
	}
	gme_delete(emu);
	return 0;
}

int main(int argc, char** argv)
{
	if (argc == 3 && !strcmp(argv[1], "--info")) return info(argv[2]);
	if (argc != 6) {
		fprintf(stderr, "usage: render <file> <track> <ms> <rate> <out.raw>\n       render --info <file>\n");
		return 2;
	}
	int track = atoi(argv[2]);
	long ms = atol(argv[3]);
	int rate = atoi(argv[4]);
	std::vector<char> buf;
	if (!load(argv[1], buf)) return 1;
	Music_Emu* emu = 0;
	if (check(openEmu(argv[1], buf, &emu, rate))) return 1;
	if (check(gme_start_track(emu, track))) { gme_delete(emu); return 1; }
	int out = open(argv[5], O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) { fprintf(stderr, "cannot write %s\n", argv[5]); gme_delete(emu); return 1; }
	long frames = (long)((long long)ms * rate / 1000);
	short pcm[2 * 1024];
	unsigned char be[sizeof pcm];
	while (frames > 0) {
		int f = frames > 1024 ? 1024 : (int)frames;
		if (check(gme_play(emu, f * 2, pcm))) { close(out); gme_delete(emu); return 1; }
		for (int i = 0; i < f * 2; i++) {
			unsigned short v = (unsigned short)pcm[i];
			be[2 * i] = v >> 8;
			be[2 * i + 1] = v & 255;
		}
		if (write(out, be, 4 * f) != 4 * f) { close(out); gme_delete(emu); return 1; }
		frames -= f;
	}
	close(out);
	gme_delete(emu);
	return 0;
}
