/* gme.library API (D9). One GME Music_Emu per handle; GME keeps no mutable
   globals, so handles are independent and the API is reentrant per handle.
   Library-wide statistics are updated under Forbid()/Permit(), same as
   playsap. */
#include <string.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include "gme.h"
#include "ezpk.h"
#include "gmelib_int.h"
#include "gmelib_vec.h"

struct GMEHandle {
	Music_Emu *emu;
	gme_type_t type;
	UBYTE *data;		/* private copy of the module: GME fixes the rate at load,
				   so GME_SetSampleRate reloads from it */
	LONG   length;
	LONG   rate;
	LONG   started;		/* GME_SetSong done (explicitly or implicitly) */
	short *tmp;		/* 8-bit formats render through this buffer */
};

#define MIN_RATE     4000
#define MAX_RATE     48000
#define DEFAULT_RATE 44100
#define TMP_SAMPLES  1024	/* shorts; even, so frames never split */
#define FADE_MS      4000	/* a timed song fades out over its last FADE_MS */

/* The D1 set. SAP is playsap.library's, so it is never ours. */
static int isOurType(gme_type_t t)
{
	return t != NULL && (t == gme_ay_type || t == gme_gbs_type || t == gme_gym_type ||
		t == gme_hes_type || t == gme_kss_type || t == gme_nsf_type ||
		t == gme_nsfe_type || t == gme_spc_type || t == gme_vgm_type ||
		t == gme_vgz_type);
}

static ULONG le32(const UBYTE *p)
{
	return (ULONG) p[0] | (ULONG) p[1] << 8 | (ULONG) p[2] << 16 | (ULONG) p[3] << 24;
}

/* GME's VGM emulator drives only SN76489, YM2413 and YM2612 (header_t ends
   before the AY8910 clock at 0x74). A plain VGM >= 1.10 with none of those
   clocks has nothing GME can render, so it is not ours. Only header bytes
   inside length are read; a short or older header gets the benefit of the doubt. */
static int vgmHasNoGmeChip(const UBYTE *m, LONG length)
{
	if (length < 0x30 || memcmp(m, "Vgm ", 4) != 0 || le32(m + 0x08) < 0x110)
		return 0;
	return le32(m + 0x0C) == 0 && le32(m + 0x10) == 0 && le32(m + 0x2C) == 0;
}

/* GME reads GYMX only with the packed field (offset 424) zero. A GYMX packed the EZPK way (the
   modland shinobi dumps) is unpacked by Open (src/ezpk.c) before GME sees it; any other packed
   GYMX (zlib) makes Open fail, so IsOurFile must refuse it too: HiP claims a file on IsOurFile
   alone. An EZPK file is judged by its header only, the stream is checked by Open. Needs the full
   428-byte header plus one data byte, as Gym_Emu's check_header does. */
static int gymxUnreadable(const UBYTE *m, LONG length)
{
	if (length < 4 || memcmp(m, "GYMX", 4) != 0)
		return 0;
	if (length >= 429 && le32(m + 424) != 0)
		return ezpk_unpacked_size(m, length) < 0;
	return length < 429;
}

/* Header magic first (same test as gme_open_data); the extension only for
   files without one (raw GYM). */
static gme_type_t identify(const UBYTE *module, LONG length, const char *filename)
{
	gme_type_t t = NULL;
	if (module != NULL && length >= 4)
		t = gme_identify_extension(gme_identify_header(module));
	if (t == NULL && filename != NULL)
		t = gme_identify_extension(filename);
	if (t == gme_vgm_type && module != NULL && vgmHasNoGmeChip(module, length))
		return NULL;
	if (t == gme_gym_type && module != NULL && gymxUnreadable(module, length))
		return NULL;
	return isOurType(t) ? t : NULL;
}

static Music_Emu *loadEmu(gme_type_t type, const UBYTE *data, LONG length, LONG rate)
{
	Music_Emu *emu = gme_new_emu(type, (int) rate);
	if (emu == NULL)
		return NULL;
	if (gme_load_data(emu, data, (long) length) != NULL) {
		gme_delete(emu);
		return NULL;
	}
	return emu;
}

static void copyText(char *dst, LONG size, const char *src)
{
	LONG n = src != NULL ? (LONG) strlen(src) : 0;
	if (n > size - 1)
		n = size - 1;
	memcpy(dst, src, (size_t) n);
	dst[n] = '\0';
}

/* Report why GME_Open failed through the calling process's secondary result
   (what dos.library IoErr() returns), so no new LVO is needed. Skipped for
   plain tasks, which have no pr_Result2. */
static void setOpenError(LONG code)
{
	struct Task *t = FindTask(NULL);
	if (t != NULL && t->tc_Node.ln_Type == NT_PROCESS)
		((struct Process *) t)->pr_Result2 = code;
}

static void freeHandle(struct GMEHandle *h)
{
	if (h->emu != NULL)
		gme_delete(h->emu);
	if (h->data != NULL)
		FreeVec(h->data);
	if (h->tmp != NULL)
		FreeVec(h->tmp);
	FreeVec(h);
}

struct GMEHandle *GME_Open(REG(a6, struct GmeBase *base), REG(a0, const UBYTE *module), REG(d0, LONG length), REG(a1, const char *filename))
{
	struct GMEHandle *h;
	gme_type_t type;
	ULONG fails = gmelib_allocfails;
	long unpacked;
	int perr = 0;
	if (module == NULL || length <= 0) {
		setOpenError(ERROR_BAD_NUMBER);
		return NULL;
	}
	type = identify(module, length, filename);
	if (type == NULL) {
		setOpenError(ERROR_OBJECT_WRONG_TYPE);
		return NULL;
	}
	h = (struct GMEHandle *) AllocVec(sizeof(*h), MEMF_PUBLIC | MEMF_CLEAR);
	if (h == NULL) {
		setOpenError(ERROR_NO_FREE_STORE);
		return NULL;
	}
	h->type = type;
	h->length = length;
	h->rate = DEFAULT_RATE;
	/* A packed GYMX (EZPK) is unpacked straight into the handle's own buffer, which GME then reads. */
	unpacked = type == gme_gym_type ? ezpk_unpacked_size(module, length) : -1;
	if (unpacked > 0)
		h->length = unpacked;
	h->data = (UBYTE *) AllocVec((ULONG) h->length, MEMF_PUBLIC);
	h->tmp = (short *) AllocVec(TMP_SAMPLES * sizeof(short), MEMF_PUBLIC);
	if (h->data != NULL && h->tmp != NULL) {
		if (unpacked > 0)
			perr = ezpk_unpack(module, length, h->data, unpacked);
		else
			memcpy(h->data, module, (size_t) length);
		if (perr == 0)
			h->emu = loadEmu(type, h->data, h->length, h->rate);
	}
	if (h->emu == NULL) {
		int oom = h->data == NULL || h->tmp == NULL || perr == -3 || gmelib_allocfails != fails;
		freeHandle(h);
		setOpenError(oom ? ERROR_NO_FREE_STORE : ERROR_OBJECT_WRONG_TYPE);
		return NULL;
	}
	Forbid();
	base->stats.opens++;
	Permit();
	return h;
}

void GME_Close(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h))
{
	if (h != NULL)
		freeHandle(h);
}

/* Durations per D10: GME's play_length; loop = 1 when the length is derived
   (intro + 2 loops, or GME's 150 s default) rather than stated in the file. */
LONG GME_GetInfo(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(a1, struct GMEInfo *info))
{
	LONG i, songs;
	gme_info_t *t;
	if (h == NULL || info == NULL)
		return -1;
	memset(info, 0, sizeof(*info));
	songs = gme_track_count(h->emu);
	if (songs > GME_MAX_SONGS)
		songs = GME_MAX_SONGS;
	info->version = GMELIB_VERSION;
	info->channels = 2;
	info->songs = (WORD) songs;
	info->defaultSong = 0;
	for (i = 0; i < songs; i++) {
		if (gme_track_info(h->emu, &t, (int) i) != NULL) {
			info->durationMs[i] = -1;
			continue;
		}
		info->durationMs[i] = t->play_length;
		info->loop[i] = t->length <= 0 ? 1 : 0;
		if (i == 0) {
			copyText(info->title, sizeof(info->title), t->song);
			copyText(info->author, sizeof(info->author), t->author);
			copyText(info->system, sizeof(info->system), t->system);
			copyText(info->game, sizeof(info->game), t->game);
			copyText(info->copyright, sizeof(info->copyright), t->copyright);
		}
		gme_free_info(t);
	}
	return 0;
}

/* song < 0: default song. durationMs < 0: play without end (silence
   detection still ends the song). durationMs >= 0: the song fades out over
   its last FADE_MS (or last half, if shorter) and ends at durationMs. */
LONG GME_SetSong(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG song), REG(d1, LONG durationMs))
{
	if (h == NULL)
		return -1;
	if (song < 0)
		song = 0;
	if (song >= gme_track_count(h->emu))
		return -1;
	gme_set_autoload_playback_limit(h->emu, 0);
	if (gme_start_track(h->emu, (int) song) != NULL)
		return -1;
	if (durationMs >= 0) {
		LONG fade = durationMs / 2 < FADE_MS ? durationMs / 2 : FADE_MS;
		gme_set_fade_msecs(h->emu, (int) (durationMs - fade), (int) fade);
	}
	h->started = 1;
	return 0;
}

LONG GME_Render(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(a1, APTR buf), REG(d0, LONG bytes), REG(d1, ULONG format))
{
	LONG n = 0, i, chunk;
	if (h == NULL || buf == NULL || bytes <= 0 || format > GME_FMT_S16BE)
		return 0;
	if (!h->started && GME_SetSong(base, h, -1, -1) != 0)
		return 0;
	if (gme_track_ended(h->emu))
		return 0;
	if (format == GME_FMT_S16BE) {
		/* 68k is big-endian: GME's native shorts are already S16BE. */
		n = (bytes / 4) * 4;
		if (n == 0 || gme_play(h->emu, (int) (n / 2), (short *) buf) != NULL)
			return 0;
	} else {
		UBYTE *p = (UBYTE *) buf;
		UBYTE flip = format == GME_FMT_U8 ? 0x80 : 0;
		LONG want = bytes & ~1L;
		while (n < want) {
			chunk = want - n < TMP_SAMPLES ? want - n : TMP_SAMPLES;
			if (gme_play(h->emu, (int) chunk, h->tmp) != NULL)
				break;
			for (i = 0; i < chunk; i++)
				p[n + i] = (UBYTE) ((h->tmp[i] >> 8) ^ flip);
			n += chunk;
		}
		if (n == 0)
			return 0;
	}
	Forbid();
	base->stats.renders++;
	base->stats.bytes += (ULONG) n;
	Permit();
	return n;
}

LONG GME_Seek(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG ms))
{
	if (h == NULL || !h->started || ms < 0)
		return -1;
	return gme_seek(h->emu, (int) ms) == NULL ? 0 : -1;
}

LONG GME_GetPosition(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h))
{
	return h != NULL && h->started ? (LONG) gme_tell(h->emu) : 0;
}

/* Bit n mutes GME voice n. Applies to the running song and stays set across
   GME_SetSong; GME_SetSampleRate reloads the emulator and clears it. */
void GME_MuteChannels(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, ULONG mask))
{
	if (h != NULL)
		gme_mute_voices(h->emu, (int) mask);
}

/* GME fixes the rate when a module loads, so a new rate reloads the emulator
   from the handle's copy. The song must be set again afterwards. */
LONG GME_SetSampleRate(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG hz))
{
	Music_Emu *emu;
	if (h == NULL || hz < MIN_RATE || hz > MAX_RATE)
		return -1;
	if (hz == h->rate)
		return 0;
	emu = loadEmu(h->type, h->data, h->length, hz);
	if (emu == NULL)
		return -1;
	gme_delete(h->emu);
	h->emu = emu;
	h->rate = hz;
	h->started = 0;
	return 0;
}

/* A detection test: callers pass only a header probe (HippoPlayer reads the
   first 1084 bytes), so the answer comes from the header magic, never from a
   load that needs the whole file (an SPC is 66 KB). The extension decides
   only for data without a known magic (raw GYM), or when no data is given.
   GME_Open still rejects a file whose body is broken. */
LONG GME_IsOurFile(REG(a6, struct GmeBase *base), REG(a0, const char *filename), REG(a1, const UBYTE *module), REG(d0, LONG length))
{
	if (module != NULL && length >= 4)
		return identify(module, length, filename) != NULL ? 1 : 0;
	return filename != NULL && isOurType(gme_identify_extension(filename)) ? 1 : 0;
}

void GME_GetStats(REG(a6, struct GmeBase *base), REG(a0, struct GMEStats *stats))
{
	if (stats == NULL)
		return;
	Forbid();
	*stats = base->stats;
	Permit();
}
