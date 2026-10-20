/* Library vector prototypes (a6 = library base on every entry point). */
#ifndef GMELIB_VEC_H
#define GMELIB_VEC_H
#include "gmelib_int.h"

struct GMEHandle *GME_Open(REG(a6, struct GmeBase *base), REG(a0, const UBYTE *module), REG(d0, LONG length), REG(a1, const char *filename));
void GME_Close(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h));
LONG GME_GetInfo(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(a1, struct GMEInfo *info));
LONG GME_SetSong(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG song), REG(d1, LONG durationMs));
LONG GME_Render(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(a1, APTR buf), REG(d0, LONG bytes), REG(d1, ULONG format));
LONG GME_Seek(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG ms));
LONG GME_GetPosition(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h));
void GME_MuteChannels(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, ULONG mask));
LONG GME_SetSampleRate(REG(a6, struct GmeBase *base), REG(a0, struct GMEHandle *h), REG(d0, LONG hz));
LONG GME_IsOurFile(REG(a6, struct GmeBase *base), REG(a0, const char *filename), REG(a1, const UBYTE *module), REG(d0, LONG length));
void GME_GetStats(REG(a6, struct GmeBase *base), REG(a0, struct GMEStats *stats));

#endif
