#ifndef LIBRARIES_GME_H
#define LIBRARIES_GME_H
/*
**	$VER: gme.h 1.0 (19.10.2026)
**
**	gme.library public structures. The library wraps Game_Music_Emu
**	(LGPL v2.1 or later) and plays VGM, VGZ, GYM, AY, GBS, HES, KSS, NSF,
**	NSFE and SPC modules.
**
**	STACK: library calls run on the CALLER's stack. Callers need 6144
**	bytes of stack below the call. Measured maximum over all formats is
**	4415 bytes (GME_SetSong on SPC), GME_Open up to 3131 (VGM), other
**	entries under 760 (tools/stackprobe, enforced by tools/run-tests.sh
**	case g).
*/
#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif

#define GMENAME	"gme.library"
#define GMELIB_VERSION	1
#define GME_MAX_SONGS	256

/* Output formats for GME_Render. Stereo output is interleaved L/R. */
#define GME_FMT_S8	0	/* signed 8-bit, ready for Paula */
#define GME_FMT_U8	1	/* unsigned 8-bit */
#define GME_FMT_S16BE	2	/* signed 16-bit big-endian */

/* Opaque per-module handle. */
struct GMEHandle;

struct GMEInfo {
	WORD  version;			/* GMELIB_VERSION of this struct */
	WORD  channels;			/* 1 = mono, 2 = stereo */
	WORD  songs;			/* number of songs, 1..GME_MAX_SONGS */
	WORD  defaultSong;		/* 0-based */
	LONG  durationMs[GME_MAX_SONGS];	/* -1 = unknown */
	BYTE  loop[GME_MAX_SONGS];	/* 1 = song loops, duration is a hint */
	char  title[128];
	char  author[128];
	char  date[32];
	WORD  ntsc;			/* 1 = NTSC timing */
	WORD  pad;
	char  system[64];		/* e.g. "Sega Mega Drive / Genesis" */
	char  game[128];
	char  copyright[128];
};

/* Library-wide counters since the library was loaded. */
struct GMEStats {
	ULONG opens;			/* successful GME_Open calls */
	ULONG renders;			/* GME_Render calls that ran the core */
	ULONG bytes;			/* bytes returned by GME_Render */
};

#endif /* LIBRARIES_GME_H */
