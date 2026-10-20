/* Automatically generated header (sfdc 1.12)! Do not edit! */

#ifndef CLIB_GME_PROTOS_H
#define CLIB_GME_PROTOS_H

/*
**   $VER: gme_protos.h $VER: gme_lib.sfd 1.0 (19.10.2026) $VER: gme_lib.sfd 1.0 (19.10.2026)
**
**   C prototypes. For use with 32 bit integers only.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#include <exec/types.h>
#include <libraries/gme.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* "gme.library" */
struct GMEHandle * GME_Open(CONST UBYTE *module, LONG length, CONST_STRPTR filename);
VOID GME_Close(struct GMEHandle *handle);
LONG GME_GetInfo(struct GMEHandle *handle, struct GMEInfo *info);
LONG GME_SetSong(struct GMEHandle *handle, LONG song, LONG durationMs);
LONG GME_Render(struct GMEHandle *handle, APTR buffer, LONG bytes, ULONG format);
LONG GME_Seek(struct GMEHandle *handle, LONG ms);
LONG GME_GetPosition(struct GMEHandle *handle);
VOID GME_MuteChannels(struct GMEHandle *handle, ULONG mask);
LONG GME_SetSampleRate(struct GMEHandle *handle, LONG hz);
LONG GME_IsOurFile(CONST_STRPTR filename, CONST UBYTE *module, LONG length);
VOID GME_GetStats(struct GMEStats *stats);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_GME_PROTOS_H */
