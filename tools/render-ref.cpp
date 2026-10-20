/* render-ref: host reference with the same driver as vgm2raw. The backend calls
   Game_Music_Emu directly and reproduces src/gmelib.c exactly (type detection,
   default rate, reload on SetSampleRate, fade policy, 8-bit conversion, seek, mute,
   track end, GetInfo fields). Output is S16 big-endian bytes whatever the host
   byte order, so it compares with the 68k output. */
#include "driver_types.h"
#include "gme.h"
#include <vector>
#include "../src/ezpk.h"

#define MIN_RATE     4000
#define MAX_RATE     48000
#define DEFAULT_RATE 44100
#define TMP_SAMPLES  1024
#define FADE_MS      4000
#define MAX_SONGS    D_MAXSONGS

static Music_Emu *emu;
static gme_type_t htype;
static std::vector<unsigned char> hdata;
static int hrate, started;
static short tmp[TMP_SAMPLES];

static int isOurType(gme_type_t t)
{
	return t != NULL && (t == gme_ay_type || t == gme_gbs_type || t == gme_gym_type ||
		t == gme_hes_type || t == gme_kss_type || t == gme_nsf_type ||
		t == gme_nsfe_type || t == gme_spc_type || t == gme_vgm_type ||
		t == gme_vgz_type);
}

static gme_type_t identify(const unsigned char *module, int length, const char *filename)
{
	gme_type_t t = NULL;
	if (module != NULL && length >= 4)
		t = gme_identify_extension(gme_identify_header(module));
	if (t == NULL && filename != NULL)
		t = gme_identify_extension(filename);
	return isOurType(t) ? t : NULL;
}

static Music_Emu *loadEmu(gme_type_t type, const unsigned char *data, int length, int rate)
{
	Music_Emu *e = gme_new_emu(type, rate);
	if (e == NULL) return NULL;
	if (gme_load_data(e, data, (long) length) != NULL) { gme_delete(e); return NULL; }
	return e;
}

static void copyText(char *dst, int size, const char *src)
{
	int n = src != NULL ? (int) strlen(src) : 0;
	if (n > size - 1) n = size - 1;
	memcpy(dst, src, (size_t) n);
	dst[n] = '\0';
}

static int be_open(const unsigned char *module, int len, const char *name)
{
	htype = identify(module, len, name);
	if (htype == NULL) return 0;
	long usz = ezpk_unpacked_size(module, len);
	if (usz > 0) {
		hdata.resize((size_t) usz);
		if (ezpk_unpack(module, len, hdata.data(), usz) != 0) return 0;
		len = (int) usz;
	} else {
		hdata.assign(module, module + len);
	}
	hrate = DEFAULT_RATE;
	emu = loadEmu(htype, hdata.data(), len, hrate);
	return emu != NULL;
}

static int be_isour(const unsigned char *module, int len, const char *name)
{
	if (module != NULL && len >= 4) return identify(module, len, name) != NULL ? 1 : 0;
	return name != NULL && isOurType(gme_identify_extension(name)) ? 1 : 0;
}

static void be_info(struct DInfo *d)
{
	int i, songs;
	gme_info_t *t;
	memset(d, 0, sizeof(*d));
	songs = gme_track_count(emu);
	if (songs > MAX_SONGS) songs = MAX_SONGS;
	d->channels = 2;
	d->songs = songs;
	d->defaultSong = 0;
	for (i = 0; i < songs; i++) {
		if (gme_track_info(emu, &t, i) != NULL) { d->duration[i] = -1; continue; }
		d->duration[i] = t->play_length;
		d->loop[i] = t->length <= 0 ? 1 : 0;
		if (i == 0) {
			copyText(d->title, sizeof(d->title), t->song);
			copyText(d->author, sizeof(d->author), t->author);
			copyText(d->system, sizeof(d->system), t->system);
			copyText(d->game, sizeof(d->game), t->game);
			copyText(d->copyright, sizeof(d->copyright), t->copyright);
		}
		gme_free_info(t);
	}
}

static int be_setrate(int hz)
{
	if (hz < MIN_RATE || hz > MAX_RATE) return -1;
	if (hz == hrate) return 0;
	Music_Emu *e = loadEmu(htype, hdata.data(), (int) hdata.size(), hz);
	if (e == NULL) return -1;
	gme_delete(emu);
	emu = e;
	hrate = hz;
	started = 0;
	return 0;
}

static int be_setsong(int song, int ms)
{
	if (song < 0) song = 0;
	if (song >= gme_track_count(emu)) return -1;
	gme_set_autoload_playback_limit(emu, 0);
	if (gme_start_track(emu, song) != NULL) return -1;
	if (ms >= 0) {
		int fade = ms / 2 < FADE_MS ? ms / 2 : FADE_MS;
		gme_set_fade_msecs(emu, ms - fade, fade);
	}
	started = 1;
	return 0;
}

static void put16(unsigned char *p, short s)
{
	unsigned short v = (unsigned short) s;
	p[0] = (unsigned char) (v >> 8);
	p[1] = (unsigned char) (v & 255);
}

static int be_render(unsigned char *buf, int bytes, int fmt)
{
	int n = 0, i, chunk;
	if (bytes <= 0 || fmt > FMT_S16BE || fmt < 0) return 0;
	if (!started && be_setsong(-1, -1) != 0) return 0;
	if (gme_track_ended(emu)) return 0;
	if (fmt == FMT_S16BE) {
		static short big[4096];
		n = (bytes / 4) * 4;
		if (n == 0 || n / 2 > 4096) return 0;
		if (gme_play(emu, n / 2, big) != NULL) return 0;
		for (i = 0; i < n / 2; i++) put16(buf + 2 * i, big[i]);
	} else {
		unsigned char flip = fmt == FMT_U8 ? 0x80 : 0;
		int want = bytes & ~1;
		while (n < want) {
			chunk = want - n < TMP_SAMPLES ? want - n : TMP_SAMPLES;
			if (gme_play(emu, chunk, tmp) != NULL) break;
			for (i = 0; i < chunk; i++)
				buf[n + i] = (unsigned char) ((tmp[i] >> 8) ^ flip);
			n += chunk;
		}
		if (n == 0) return 0;
	}
	return n;
}

static int be_seek(int ms)
{
	if (!started || ms < 0) return -1;
	return gme_seek(emu, ms) == NULL ? 0 : -1;
}
static int be_pos(void) { return started ? gme_tell(emu) : 0; }
static void be_mute(int mask) { gme_mute_voices(emu, mask); }
static void be_stats(void) {}
static void be_close(void) { gme_delete(emu); }

#include "driver.h"
