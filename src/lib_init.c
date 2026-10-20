/* gme.library: ROMTag, InitTable and the standard library vectors.
   Port of playsap src/lib_init.c to gcc register syntax (D7). Linked with
   -nostartfiles, so no crt0: this file also supplies the static constructor
   list and library list headers and terminators that crt0 would provide, and
   opens the libraries libnix's startup would open.
   NOTE: the first code in the hunk must stay LibRunGuard so that running
   the file from a shell returns -1 harmlessly. Keep it the first function,
   keep this object first on the link line, and compile it with
   -fno-toplevel-reorder (the C4 link check disassembles the first word). */
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include "gmelib_int.h"
#include "gmelib_vec.h"

struct ExecBase *SysBase;

LONG LibRunGuard(void)
{
	return -1;
}

#define LIB_NAME "gme.library"
#define LIB_VER  1
#define LIB_REV  0
#define LIB_ID   "gme.library 1.0 (19.10.2026)"

/* const data lands in the code hunk (ld script puts .rodata in .text), which
   exec needs for the RomTag scan. */
static const char libName[] = LIB_NAME;
static const char libId[]   = "$VER: " LIB_ID "\r\n";

/* Static constructors (B5: GME has three global equalizer objects). The amiga
   ld script sorts every ".list___CTOR_LIST__" input section after this header
   and ends all lists with ".end_of_lists", so the entries follow ctorHead and
   end at a zero long. Same layout crt0 sets up for libnix programs. */
typedef void (*ctor_t)(void);
static const ctor_t ctorHead[1] __attribute__((section(".list___CTOR_LIST__"), used)) = { 0 };
static const long listsEnd __attribute__((section(".end_of_lists"), used)) = 0;

/* Counts constructors run by LibInit; the D1t test checks it (proves the walk ran). */
ULONG gmelib_ctorsrun;

/* Libraries libnix code calls through (soft-float and math via mathieee*,
   __mulsi3 via utility.library). Each libstubs object adds one entry
   {base, name} to ".dlist___LIB_LIST__", base -1 until opened; this header
   sorts before them and ".end_of_dlists" ends the list with a zero long.
   Which libraries appear is decided by the link, so walk the list instead of
   naming them. */
static long libHead __attribute__((section(".dlist___LIB_LIST__"), used)) = 0;
static long dlistsEnd __attribute__((section(".end_of_dlists"), used)) = 0;

struct LibEntry {
	struct Library *base;
	const char *name;
};

static struct LibEntry *firstLib(void)
{
	long *p = &libHead;
	__asm__("" : "+a" (p));
	return (struct LibEntry *) (p + 1);
}

static void closeLibs(void)
{
	struct LibEntry *e;
	for (e = firstLib(); e->base != NULL; e++)
		if (e->base != (struct Library *) -1) {
			CloseLibrary(e->base);
			e->base = (struct Library *) -1;
		}
}

static int openLibs(void)
{
	struct LibEntry *e;
	for (e = firstLib(); e->base != NULL; e++) {
		struct Library *l = OpenLibrary((CONST_STRPTR) e->name, 0);
		if (l == NULL) {
			closeLibs();
			return 0;
		}
		e->base = l;
	}
	return 1;
}

static void runCtors(void)
{
	const ctor_t *p = ctorHead;
	/* Hide the one-element array from the optimiser: the entries after it
	   come from other objects, placed there by the linker. */
	__asm__("" : "+a" (p));
	for (p++; *p != NULL; p++) {
		(*p)();
		gmelib_ctorsrun++;
	}
}

static struct GmeBase *LibInit(REG(d0, struct GmeBase *base),
	REG(a0, BPTR seglist), REG(a6, struct ExecBase *sysbase))
{
	SysBase = sysbase;
	base->sysbase = sysbase;
	base->seglist = seglist;
	base->lib.lib_Node.ln_Type = NT_LIBRARY;
	base->lib.lib_Node.ln_Name = (char *) libName;
	base->lib.lib_Flags = LIBF_SUMUSED | LIBF_CHANGED;
	base->lib.lib_Version = LIB_VER;
	base->lib.lib_Revision = LIB_REV;
	base->lib.lib_IdString = (APTR) libId;
	if (!openLibs()) {
		FreeMem((UBYTE *) base - base->lib.lib_NegSize,
			base->lib.lib_NegSize + base->lib.lib_PosSize);
		return NULL;
	}
	runCtors();	/* after openLibs: constructors may use floating point */
	return base;
}

static struct GmeBase *LibOpen(REG(a6, struct GmeBase *base))
{
	base->lib.lib_OpenCnt++;
	base->lib.lib_Flags &= ~LIBF_DELEXP;
	return base;
}

static BPTR LibExpunge(REG(a6, struct GmeBase *base))
{
	BPTR seg;
	if (base->lib.lib_OpenCnt != 0) {
		base->lib.lib_Flags |= LIBF_DELEXP;	/* delayed expunge */
		return 0;
	}
	seg = base->seglist;
	Remove(&base->lib.lib_Node);
	closeLibs();
	FreeMem((UBYTE *) base - base->lib.lib_NegSize,
		base->lib.lib_NegSize + base->lib.lib_PosSize);
	return seg;
}

static BPTR LibClose(REG(a6, struct GmeBase *base))
{
	if (base->lib.lib_OpenCnt != 0)
		base->lib.lib_OpenCnt--;
	if (base->lib.lib_OpenCnt == 0 && (base->lib.lib_Flags & LIBF_DELEXP))
		return LibExpunge(base);
	return 0;
}

static LONG LibNull(void)
{
	return 0;
}

static const APTR funcTable[] = {
	(APTR) LibOpen, (APTR) LibClose, (APTR) LibExpunge, (APTR) LibNull,
	(APTR) GME_Open, (APTR) GME_Close, (APTR) GME_GetInfo, (APTR) GME_SetSong,
	(APTR) GME_Render, (APTR) GME_Seek, (APTR) GME_GetPosition,
	(APTR) GME_MuteChannels, (APTR) GME_SetSampleRate, (APTR) GME_IsOurFile,
	(APTR) GME_GetStats,
	(APTR) -1
};

struct InitTab {
	ULONG baseSize;
	const APTR *funcs;
	APTR initData;
	APTR init;
};

static const struct InitTab initTab = {
	sizeof(struct GmeBase), funcTable, NULL, (APTR) LibInit
};

static const char endMark = 0;

const struct Resident romTag = {
	RTC_MATCHWORD, (struct Resident *) &romTag, (APTR) &endMark,
	RTF_AUTOINIT, LIB_VER, NT_LIBRARY, 0, (char *) libName, (char *) libId + 6,
	(APTR) &initTab
};
