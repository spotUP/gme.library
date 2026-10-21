# gme.library

An AmigaOS shared library (68020 and up) that plays video game music formats from memory. It wraps
Game_Music_Emu (GME, commit f68963b). A player hands it a module and gets back PCM that it can feed to
Paula or write to a file. The API has the same shape as playsap.library: Open, GetInfo, SetSong, Render,
Seek, Close.

## Formats

VGM, VGZ (gzip-compressed VGM), GYM (raw, unpacked GYMX, and EZPK-packed GYMX, which Open unpacks itself; zlib-packed GYMX is refused), AY, GBS, HES,
KSS (non-FM; whether MSX FM sound is in scope is undecided), NSF, NSFE, SPC.

SAP is not handled here; it belongs to playsap.library. `GME_IsOurFile` never claims a SAP file.

## Requirements

- 68020 or better.
- Stack: library calls run on the caller's stack. Callers need 6144 bytes free below the call. Measured
  maximum is 4415 bytes (`GME_SetSong` on SPC); `GME_Open` needs up to 3131 (VGM); all other entries
  need under 760.
- Memory: every allocation is an exec `AllocVec`, owned by the handle. A handle holds a private copy of
  the module plus the emulator state. An SPC module is about 66 KB.

## API

Base name `_GmeBase`, vectors from -30. Structures and constants in `include/libraries/gme.h`,
register layout in `sfd/gme_lib.sfd` (and `fd/`). Calls are reentrant per handle. Output formats:
`GME_FMT_S8` (signed 8-bit, ready for Paula), `GME_FMT_U8`, `GME_FMT_S16BE`. Stereo is interleaved L/R.

| LVO | Function | Registers |
|-----|----------|-----------|
| -30 | `GME_Open(module, length, filename)` | A0, D0, A1 |
| -36 | `GME_Close(handle)` | A0 |
| -42 | `GME_GetInfo(handle, info)` | A0, A1 |
| -48 | `GME_SetSong(handle, song, durationMs)` | A0, D0, D1 |
| -54 | `GME_Render(handle, buffer, bytes, format)` | A0, A1, D0, D1 |
| -60 | `GME_Seek(handle, ms)` | A0, D0 |
| -66 | `GME_GetPosition(handle)` | A0 |
| -72 | `GME_MuteChannels(handle, mask)` | A0, D0 |
| -78 | `GME_SetSampleRate(handle, hz)` | A0, D0 |
| -84 | `GME_IsOurFile(filename, module, length)` | A0, A1, D0 |
| -90 | `GME_GetStats(stats)` | A0 |

- `GME_Open`: identifies the format from the header magic first, from the file name extension only for
  data without a magic (raw GYM). Copies the module, so the caller may free its buffer. Returns a handle
  or NULL; the reason is in `IoErr()`: `ERROR_NO_FREE_STORE`, `ERROR_OBJECT_WRONG_TYPE` (not one of our
  formats or broken body) or `ERROR_BAD_NUMBER` (bad arguments). The default sample rate is 44100 Hz.
- `GME_Close`: frees the handle. NULL is accepted.
- `GME_GetInfo`: fills `struct GMEInfo`: channels, song count (1..256), default song, per-song duration
  in ms (-1 unknown) and loop flag, title, author, date, system, game, copyright, NTSC flag. Duration is
  GME's play length; `loop` is 1 when the length is derived (intro plus two loops, or GME's 150 s
  default) instead of stated in the file. Returns 0 on success, -1 on failure.
- `GME_SetSong`: song < 0 selects the default (first) song; a song number beyond the count returns -1.
  durationMs < 0 plays without end (silence detection still ends the song). durationMs >= 0 fades the
  song out over its last 4000 ms (or its last half, if shorter) and ends it at durationMs. Restarts the
  song. Returns 0 or -1. If it was never called, the first `GME_Render` starts song 0 without a limit.
- `GME_Render`: writes up to `bytes` bytes in the given format and returns the number of bytes written.
  S16BE writes whole 4-byte stereo frames (bytes rounded down to a multiple of 4); the 8-bit formats
  round down to even. Returns 0 at the end of the song, and also for bad arguments or a failed core call.
  Callers loop until 0.
- `GME_Seek`: move to `ms` from the song start. Needs a started song. 0 or -1. Seeking forward renders
  and discards audio, so it costs time.
- `GME_GetPosition`: position in ms, 0 before the first `GME_SetSong`.
- `GME_MuteChannels`: bit n mutes GME voice n. Applies to the running song and stays set across
  `GME_SetSong`. `GME_SetSampleRate` clears it.
- `GME_SetSampleRate`: 4000 to 48000 Hz. GME fixes the rate at load, so a new rate reloads the emulator
  from the handle's copy: call `GME_SetSong` again afterwards. Same rate: no-op, returns 0. 0 or -1.
- `GME_IsOurFile`: a detection test that needs only a header probe (HippoPlayer passes the first 1084
  bytes). Answer comes from the header magic; the extension decides only for raw GYM or when no data is
  given. Returns 1 or 0. `GME_Open` can still reject a file whose body is broken.
- `GME_GetStats`: library-wide counters (opens, renders, bytes) since load.

## Building and testing

Interim toolchain: bebbo gcc 6.5 in `~/opt/amiga`. Host builds use clang. Tests run under `vamos`
(amitools) and compare 68k output byte for byte with the host build of the same core.

    make lib             # build/gme.library (needs only the checkout: src/gme and src/zlib are committed)
    make host-corpus     # host reference renders of corpus/ (needs the submodules)
    make check           # run-tests cases under vamos, results in build/tests/*.ok|fail (needs the submodules)
    make vendor          # regenerate src/gme and src/zlib from the submodules plus patches/

The third-party sources are git submodules under `third_party/` (Game_Music_Emu pinned at f68963b, zlib at
v1.3.1; versions in `third_party/VERSIONS`). Clone with `git clone --recurse-submodules`, or run
`git submodule update --init` in an existing checkout or a new worktree. `make lib` does not need them.

Only one vamos run at a time.

The test corpus (`corpus/`) and the emulator rig scripts are not in the public repository: the
modules are copyrighted. Put your own files in `corpus/` before running the corpus targets.

## Licence

gme.library is licensed under the GNU Lesser General Public License, version 2.1 or (at your option)
any later version, because Game_Music_Emu is. The full text is in `COPYING`. Third-party notices are in
`NOTICES`. The YM2612 core is `Ym2612_GENS.cpp` (LGPL-2.1+). `VGM_YM2612_MAME` (GPL) and
`VGM_YM2612_NUKED` are never defined and their sources are not vendored: the library contains no GPL code.

Files compiled into `build/gme.library` (from the Makefile object lists):

| Files | Licence |
|-------|---------|
| `src/gme/*.cpp` (47 files: Ay_*, Blip_Buffer, Classic_Emu, Data_Reader, Dual_Resampler, Effects_Buffer, Fir_Resampler, Gb_*, Gbs_Emu, Gme_File, gme, Gym_Emu, Hes_*, Kss_*, M3u_Playlist, Multi_Buffer, Music_Emu, Nes_*, Nsf_Emu, Nsfe_Emu, Sap_*, Sms_Apu, Snes_Spc, Spc_*, Vgm_Emu*, Ym2612_GENS) | LGPL-2.1-or-later (per-file header, Shay Green and contributors) |
| `src/gme/Nes_Vrc7_Apu.cpp` | no header in the file; part of GME, covered by GME's `license.txt` (LGPL-2.1) |
| `src/gme/Ym2413_Emu.cpp` | no header in the file; gmelib replacement of GME's stub, glue to emu2413; distributed under LGPL-2.1-or-later with the rest of the library |
| `src/gme/ext/emu2413.c` | MIT, Copyright (C) 2020 Mitsutaka Okazaki (see `NOTICES`) |
| `src/zlib/adler32.c crc32.c inffast.c inflate.c inftrees.c zutil.c` | zlib licence (see `NOTICES`) |
| `src/bzip2/*` (libbzip2 1.0.8 decompressor, magics patched for EZPK) | bzip2 licence, Julian Seward (see `NOTICES`, `src/bzip2/LICENSE`) |
| `src/lib_init.c`, `src/gmelib.c`, `src/ezpk.c`, `src/mem_glue.cpp` | gme.library, LGPL-2.1-or-later |

The headers under `src/gme` mostly carry no licence text; they are part of GME and fall under its LGPL
`license.txt`. `src/gme/Ym2612_GENS.cpp` is derived from Gens 2.10 (Stephane Dallongeville); GME's file
header states LGPL-2.1-or-later for it, and that statement is relied on here.
