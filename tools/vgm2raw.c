/* vgm2raw: command-line client of gme.library (links no GME code). */
const char vgm2raw_ver[] = "$VER: vgm2raw 1.0 (19.10.2026)";
#include "driver_types.h"
#include <exec/types.h>
#include <proto/exec.h>
#include <libraries/gme.h>
#include <proto/gme.h>

struct Library *GmeBase;
static struct GMEHandle *h;
static struct GMEInfo pinfo;	/* static: 4 KB, keep the caller's stack small */

static int be_libopen(void)
{
	if (GmeBase == NULL)
		GmeBase = OpenLibrary(GMENAME, GMELIB_VERSION);
	return GmeBase != NULL;
}

static int be_open(const unsigned char *module, int len, const char *name)
{
	if (!be_libopen()) return 0;
	h = GME_Open((CONST UBYTE *) module, len, (CONST_STRPTR) name);
	return h != NULL;
}

static int be_isour(const unsigned char *module, int len, const char *name)
{
	if (!be_libopen()) return -1;
	return (int) GME_IsOurFile((CONST_STRPTR) name, (CONST UBYTE *) module, len);
}

static void be_info(struct DInfo *d)
{
	int s;
	GME_GetInfo(h, &pinfo);
	memset(d, 0, sizeof(*d));
	d->channels = pinfo.channels; d->songs = pinfo.songs; d->defaultSong = pinfo.defaultSong; d->ntsc = pinfo.ntsc;
	for (s = 0; s < D_MAXSONGS; s++) { d->duration[s] = pinfo.durationMs[s]; d->loop[s] = pinfo.loop[s]; }
	memcpy(d->title, pinfo.title, sizeof(d->title));
	memcpy(d->author, pinfo.author, sizeof(d->author));
	memcpy(d->date, pinfo.date, sizeof(d->date));
	memcpy(d->system, pinfo.system, sizeof(d->system));
	memcpy(d->game, pinfo.game, sizeof(d->game));
	memcpy(d->copyright, pinfo.copyright, sizeof(d->copyright));
}

static int be_setrate(int hz) { return (int) GME_SetSampleRate(h, hz); }
static int be_setsong(int song, int ms) { return (int) GME_SetSong(h, song, ms); }
static int be_render(unsigned char *buf, int bytes, int fmt) { return (int) GME_Render(h, buf, bytes, (ULONG) fmt); }
static int be_seek(int ms) { return (int) GME_Seek(h, ms); }
static int be_pos(void) { return (int) GME_GetPosition(h); }
static void be_mute(int mask) { GME_MuteChannels(h, (ULONG) mask); }

static void be_stats(void)
{
	struct GMEStats st;
	GME_GetStats(&st);
	SAYF("stats opens=%lu renders=%lu bytes=%lu\n", (unsigned long) st.opens, (unsigned long) st.renders, (unsigned long) st.bytes);
}

static void be_close(void)
{
	GME_Close(h);
	if (GmeBase) CloseLibrary(GmeBase);
}

#include "driver.h"
