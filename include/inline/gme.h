/* Automatically generated header (sfdc 1.12)! Do not edit! */

#ifndef _INLINE_GME_H
#define _INLINE_GME_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef GME_BASE_NAME
#define GME_BASE_NAME GmeBase
#endif /* !GME_BASE_NAME */

#define GME_Open(___module, ___length, ___filename) \
      LP3(0x1e, struct GMEHandle *, GME_Open , CONST UBYTE *, ___module, a0, LONG, ___length, d0, CONST_STRPTR, ___filename, a1,\
      , GME_BASE_NAME)

#define GME_Close(___handle) \
      LP1NR(0x24, GME_Close , struct GMEHandle *, ___handle, a0,\
      , GME_BASE_NAME)

#define GME_GetInfo(___handle, ___info) \
      LP2(0x2a, LONG, GME_GetInfo , struct GMEHandle *, ___handle, a0, struct GMEInfo *, ___info, a1,\
      , GME_BASE_NAME)

#define GME_SetSong(___handle, ___song, ___durationMs) \
      LP3(0x30, LONG, GME_SetSong , struct GMEHandle *, ___handle, a0, LONG, ___song, d0, LONG, ___durationMs, d1,\
      , GME_BASE_NAME)

#define GME_Render(___handle, ___buffer, ___bytes, ___format) \
      LP4(0x36, LONG, GME_Render , struct GMEHandle *, ___handle, a0, APTR, ___buffer, a1, LONG, ___bytes, d0, ULONG, ___format, d1,\
      , GME_BASE_NAME)

#define GME_Seek(___handle, ___ms) \
      LP2(0x3c, LONG, GME_Seek , struct GMEHandle *, ___handle, a0, LONG, ___ms, d0,\
      , GME_BASE_NAME)

#define GME_GetPosition(___handle) \
      LP1(0x42, LONG, GME_GetPosition , struct GMEHandle *, ___handle, a0,\
      , GME_BASE_NAME)

#define GME_MuteChannels(___handle, ___mask) \
      LP2NR(0x48, GME_MuteChannels , struct GMEHandle *, ___handle, a0, ULONG, ___mask, d0,\
      , GME_BASE_NAME)

#define GME_SetSampleRate(___handle, ___hz) \
      LP2(0x4e, LONG, GME_SetSampleRate , struct GMEHandle *, ___handle, a0, LONG, ___hz, d0,\
      , GME_BASE_NAME)

#define GME_IsOurFile(___filename, ___module, ___length) \
      LP3(0x54, LONG, GME_IsOurFile , CONST_STRPTR, ___filename, a0, CONST UBYTE *, ___module, a1, LONG, ___length, d0,\
      , GME_BASE_NAME)

#define GME_GetStats(___stats) \
      LP1NR(0x5a, GME_GetStats , struct GMEStats *, ___stats, a0,\
      , GME_BASE_NAME)

#endif /* !_INLINE_GME_H */
