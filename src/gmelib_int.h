/* gme.library internals shared by lib_init.c, gmelib.c and the memory glue. */
#ifndef GMELIB_INT_H
#define GMELIB_INT_H
#include <exec/types.h>
#include <dos/dos.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <libraries/gme.h>

/* gcc register-argument syntax (D7): REG(d0, LONG x) declares x passed in d0. */
#define REG(r, decl) register decl __asm(#r)

struct GmeBase {
	struct Library lib;
	BPTR   seglist;
	struct ExecBase *sysbase;
	struct GMEStats stats;		/* updated under Forbid()/Permit() */
};

/* Failed-allocation counter, defined in the memory glue. */
extern ULONG gmelib_allocfails;

/* The library is single-instance; set once in LibInit. */
extern struct ExecBase *SysBase;

#endif
