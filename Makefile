# gme.library build (GNU make, macOS)
GME   = third_party/gme/gme
ZLIB  = third_party/zlib-1.3.1
CXX   = clang++
CC    = clang
# D4: only the GENS YM2612 core (LGPL). MAME (GPL) and Nuked are never defined, nor vendored.
DEFS  = -DBLARGG_LITTLE_ENDIAN=1 -DVGM_YM2612_GENS -DHAVE_ZLIB_H
OPT   = -O2 -ffp-contract=off
CORPUS_FILES = $(filter-out corpus/SOURCES,$(wildcard corpus/*))
CORPUS_FILES := $(filter-out %.txt %.md,$(CORPUS_FILES))
NEG_FILES =
# Files IsOurFile must refuse and Open must fail on: no chip GME emulates.
NOTOURS_FILES = corpus/vgm-ay-only.vgm
NEG_FILES += $(NOTOURS_FILES)
CORPUS_FILES := $(filter-out $(NEG_FILES),$(CORPUS_FILES))
SONGMS = 30000
RATE   = 44100

.PHONY: check-ezpk vendor host host-corpus clean amiga check-core audit-syms audit-ctors includes tests check check-v171 check-vendor check-ym2413
.DELETE_ON_ERROR:

vendor:
	rm -rf src/gme src/zlib
	mkdir -p src/gme/ext src/zlib
	for f in $(GME)/*.cpp $(GME)/*.h; do \
	  case $$f in *Ym2612_MAME.*|*Ym2612_Nuked.*) ;; *) cp $$f src/gme/;; esac; done
	cp $(GME)/ext/emu2413.c $(GME)/ext/emu2413.h src/gme/ext/
	cp $(addprefix $(ZLIB)/,inflate.c inftrees.c inffast.c zutil.c crc32.c adler32.c \
	  zlib.h zconf.h zutil.h inflate.h inftrees.h inffast.h inffixed.h crc32.h gzguts.h) src/zlib/
	for p in $(sort $(wildcard patches/*.patch)); do patch -p1 < $$p || exit 1; done

# host reference. gz* file API (gzopen etc.) is referenced by GME's file reader; the host
# build takes it from the pristine zlib tree, the 68k build patches it out.
host: build/render-host
HOST_CXX = $(wildcard src/gme/*.cpp)
# EZPK (packed GYMX): src/ezpk.c plus the vendored libbzip2 decompressor (src/bzip2, not touched by make vendor).
BZ_C     = $(wildcard src/bzip2/*.c)
BZDEFS   = -DBZ_NO_STDIO -DBZ_DECOMPRESS_ONLY
HOST_C   = src/ezpk.c $(BZ_C) src/gme/ext/emu2413.c $(wildcard src/zlib/*.c) $(ZLIB)/gzlib.c $(ZLIB)/gzread.c $(ZLIB)/gzclose.c $(ZLIB)/gzwrite.c $(ZLIB)/deflate.c $(ZLIB)/trees.c
build/render-host: tools/render.cpp $(HOST_CXX) $(HOST_C) $(wildcard src/gme/*.h)
	mkdir -p build/obj
	rm -f build/obj/*.o
	for f in $(HOST_CXX); do $(CXX) $(OPT) -std=c++17 $(DEFS) -Isrc/gme -Isrc/zlib -c $$f -o build/obj/$$(basename $$f).o || exit 1; done
	for f in $(HOST_C); do $(CC) $(OPT) $(DEFS) $(BZDEFS) -DZ_HAVE_UNISTD_H -Isrc/zlib -I$(ZLIB) -c $$f -o build/obj/$$(basename $$f).o || exit 1; done
	$(CXX) $(OPT) -std=c++17 $(DEFS) -Isrc/gme -c tools/render.cpp -o build/obj/render.o
	$(CXX) -o $@ build/obj/*.o

host-corpus: build/render-host
	mkdir -p build/host
	for f in $(CORPUS_FILES); do n=$$(basename $$f); \
	  if [ ! -f build/host/$$n.raw ] || [ build/render-host -nt build/host/$$n.raw ]; then \
	    build/render-host $$f 0 $(SONGMS) $(RATE) build/host/$$n.raw && \
	    build/render-host --info $$f > build/host/$$n.info || { echo "FAIL $$f"; rm -f build/host/$$n.raw; exit 1; }; \
	  fi; done

# ---- 68k (bebbo gcc, read-only toolchain, D3). Interim: gcc 6.5; amiga16's shipped C libraries
# predate its call-saved-register fix (thoughts/shared/research/2026-10-19-amiga16-defect.md). Test tool links libnix startup; the core objects do not use stdio.
A68    = $(HOME)/opt/amiga/bin
CXX68  = $(A68)/m68k-amigaos-g++
CC68   = $(A68)/m68k-amigaos-gcc
NM68   = $(A68)/m68k-amigaos-nm
OBJDUMP68 = $(A68)/m68k-amigaos-objdump
VAMOS  ?= vamos
FLAGS68 = -m68020 -O2 -ffunction-sections -fdata-sections -noixemul -DBLARGG_BIG_ENDIAN=1 -DVGM_YM2612_GENS -DHAVE_ZLIB_H -DGME_NO_STDIO
CXXFLAGS68 = $(FLAGS68) -fno-exceptions -fno-rtti -fno-threadsafe-statics -Isrc/gme -Isrc/zlib
CFLAGS68   = $(FLAGS68) -Isrc/zlib
OBJ68 = $(patsubst src/gme/%.cpp,build/68k/%.o,$(wildcard src/gme/*.cpp)) build/68k/emu2413.o build/68k/emu2413_68k.o \
        $(patsubst src/zlib/%.c,build/68k/%.o,$(wildcard src/zlib/*.c)) \
        $(patsubst src/bzip2/%.c,build/68k/%.o,$(BZ_C)) build/68k/ezpk.o
# own ram: per checkout: vamos' default temp ram volume clashes when two checkouts run at once.
VAMOSRUN = $(VAMOS) -C 68020 -m 16384 -s 512 -H disable -V ram:$(CURDIR)/build/ramv

amiga: $(OBJ68) build/render
build/68k/%.o: src/gme/%.cpp $(wildcard src/gme/*.h)
	@mkdir -p build/68k; $(CXX68) $(CXXFLAGS68) -c $< -o $@
# emu2413 hot paths (update_slots, resampler FIR) are hand-written 68020+ asm; the C stays the host reference.
build/68k/emu2413.o: src/gme/ext/emu2413.c src/gme/ext/emu2413.h
	@mkdir -p build/68k; $(CC68) $(CFLAGS68) -DOPLL_ASM -c $< -o $@
build/68k/emu2413_68k.o: src/gme/ext/emu2413_68k.s
	@mkdir -p build/68k; $(CC68) -m68020 -c $< -o $@
build/68k/%.o: src/zlib/%.c
	@mkdir -p build/68k; $(CC68) $(CFLAGS68) -c $< -o $@
build/68k/%.o: src/bzip2/%.c $(wildcard src/bzip2/*.h)
	@mkdir -p build/68k; $(CC68) $(CFLAGS68) $(BZDEFS) -c $< -o $@
build/68k/ezpk.o: src/ezpk.c src/ezpk.h
	@mkdir -p build/68k; $(CC68) $(CFLAGS68) -c $< -o $@
build/68k/render.o: tools/render.cpp
	@mkdir -p build/68k; $(CXX68) $(CXXFLAGS68) -c $< -o $@
build/68k/amiga_shim.o: tools/amiga_shim.cpp
	@mkdir -p build/68k; $(CXX68) $(CXXFLAGS68) -c $< -o $@
build/render: build/68k/render.o build/68k/amiga_shim.o $(OBJ68)
	$(CXX68) -noixemul -o $@ build/68k/render.o build/68k/amiga_shim.o $(OBJ68) -lm

# The library. lib_init.o links first so LibRunGuard is the hunk's first code;
# -fno-toplevel-reorder keeps it first inside lib_init.o and keeps the ctor list
# header in front of GME's entries. lib_init.o is built without -ffunction-sections:
# its code must land in plain .text, which the ld script places before every .text.*. No startup code: lib_init.c walks the ctors.
LIBOBJ = build/68k/lib_init.o build/68k/gmelib.o build/68k/mem_glue.o
LIBCFLAGS = $(FLAGS68) -Iinclude -Isrc -Isrc/gme -Wall
build/68k/lib_init.o: src/lib_init.c src/gmelib_int.h src/gmelib_vec.h include/libraries/gme.h
	@mkdir -p build/68k; $(CC68) $(filter-out -ffunction-sections -fdata-sections,$(LIBCFLAGS)) -fno-toplevel-reorder -c $< -o $@
build/68k/gmelib.o: src/gmelib.c src/gmelib_int.h src/gmelib_vec.h include/libraries/gme.h
	@mkdir -p build/68k; $(CC68) $(LIBCFLAGS) -c $< -o $@
build/68k/mem_glue.o: src/mem_glue.cpp
	@mkdir -p build/68k; $(CXX68) $(CXXFLAGS68) -c $< -o $@
build/gme.library: $(LIBOBJ) $(OBJ68)
	$(CC68) -noixemul -nostartfiles -o $@ $(LIBOBJ) $(OBJ68) -lm -Wl,-Map,build/gme.map

# The linked library must not pull startup, stdio or libnix's heap, and must
# start with LibRunGuard (moveq #-1,d0; rts). Silent on pass.
LIBFORBID = __main|_exit|___exitlibraries|_fopen|_printf|___stdio|___initlibraries|___nocommandline|_DOSBase
audit-lib: build/gme.library
	@$(NM68) build/gme.library > build/68k/lib-syms.txt; \
	if grep -E ' ($(LIBFORBID))$$' build/68k/lib-syms.txt; then echo "audit-lib: forbidden symbols"; exit 1; fi; \
	$(OBJDUMP68) -d build/gme.library | awk '/>:$$/{f=1;next} f&&NF>1{print;exit}' | grep -q '70ff' || { echo "audit-lib: first code is not LibRunGuard"; exit 1; }

# 68k vs host, byte-identical. Resumable: *.ok skipped.
check-core: build/render host-corpus
	@mkdir -p build/check build/ramv; rc=0; \
	for f in $(CORPUS_FILES); do n=$$(basename $$f); \
	  [ -f build/check/$$n.ok ] && continue; rm -f build/check/$$n.fail build/check/$$n.diff; \
	  $(VAMOSRUN) build/render $$f 0 $(SONGMS) $(RATE) build/check/$$n.raw >build/check/$$n.log 2>&1; \
	  if [ -s build/check/$$n.raw ] && cmp -s build/check/$$n.raw build/host/$$n.raw; then touch build/check/$$n.ok; echo "ok   $$n"; \
	  else touch build/check/$$n.fail; python3 -I tools/rawdiff.py build/host/$$n.raw build/check/$$n.raw > build/check/$$n.diff; echo "FAIL $$n: $$(cat build/check/$$n.diff)"; rc=1; fi; \
	done; exit $$rc

# Undefined externals of the core objects (all objects minus what they define). Silent on pass.
FORBID = fopen|fread|fseek|ftell|fclose|gzopen|gzread|gzclose|gzseek|gztell|gzeof|__stdio|_main|printf
audit-syms: $(OBJ68)
	@$(NM68) -u $(OBJ68) | awk 'NF==2&&$$1=="U"{print $$2}' | sort -u > build/68k/undef.txt; \
	$(NM68) --defined-only $(OBJ68) | awk 'NF==3{print $$3}' | sort -u > build/68k/def.txt; \
	comm -23 build/68k/undef.txt build/68k/def.txt > build/68k/ext.txt; \
	if grep -E '^_?($(FORBID))$$' build/68k/ext.txt; then echo "audit-syms: forbidden symbols"; exit 1; fi

audit-ctors: $(OBJ68)
	@$(NM68) $(OBJ68) | grep _GLOBAL__sub_I || true

clean:
	rm -rf build

# Public includes, generated from sfd/gme_lib.sfd with the toolchain's sfdc.
SFDC = $(A68)/sfdc
SFD  = sfd/gme_lib.sfd
includes:
	mkdir -p fd include/clib include/inline include/proto
	$(SFDC) --quiet --mode=fd       --output=fd/gme_lib.fd           $(SFD)
	$(SFDC) --quiet --mode=clib     --output=include/clib/gme_protos.h $(SFD)
	$(SFDC) --quiet --mode=macros   --output=include/inline/gme.h    $(SFD)
	$(SFDC) --quiet --mode=proto    --output=include/proto/gme.h     $(SFD)
	$(SFDC) --quiet --mode=lvo      --output=include/gme_lib.i       $(SFD)

# ---- tests (D1t, C5). vgm2raw/stackprobe: 68k clients of the library, link no GME code. render-ref:
# host reference with the same driver (tools/driver.h), calls GME directly. render-ref links the host
# GME objects of build/render-host (build/obj minus render.o).
VGM2RAW_DEPS = tools/driver.h tools/driver_types.h include/proto/gme.h include/libraries/gme.h
build/vgm2raw: tools/vgm2raw.c $(VGM2RAW_DEPS)
	$(CC68) -m68020 -O2 -noixemul -Iinclude -o $@ tools/vgm2raw.c
# Rig bench (phase E): 68k client, timer.device EClock. Run on the FS-UAE rig via rig/run-bench.sh.
build/bench: tools/bench.c include/proto/gme.h include/libraries/gme.h
	$(CC68) -m68020 -O2 -noixemul -Iinclude -o $@ tools/bench.c
build/getstats: tools/getstats.c include/proto/gme.h include/libraries/gme.h
	$(CC68) -m68020 -O2 -noixemul -Iinclude -o $@ tools/getstats.c
build/stackprobe: tools/stackprobe.c include/proto/gme.h include/libraries/gme.h
	$(CC68) -m68020 -O2 -noixemul -Iinclude -o $@ tools/stackprobe.c
build/render-ref: tools/render-ref.cpp tools/driver.h tools/driver_types.h build/render-host
	$(CXX) $(OPT) -std=c++17 $(DEFS) -Isrc/gme -Itools -o $@ tools/render-ref.cpp $(filter-out build/obj/render.o,$(wildcard build/obj/*.o))

# Per-case build/tests/<name>.<case>.ok|fail, resumable (delete the file to re-run one case).
# Runs under vamos with LIBS: assigned to build/ (real 68k library loaded). The own ram: volume keeps
# concurrent vamos runs (check-core) from colliding on the default temp volume. STACK_LIMIT is the
# guaranteed caller stack (C5): measured max 4415 bytes (SPC SetSong), rounded up to 1024 plus 1024. See include/libraries/gme.h.
STACK_LIMIT = 6144
VAMOSLIB = $(VAMOS) -C 68020 -m 16384 -H disable -V ram:$(CURDIR)/build/ramv -V pl:$(CURDIR) -a LIBS:pl:build
tests: build/gme.library build/vgm2raw build/stackprobe build/render-ref
	@mkdir -p build/ramv; STACK_LIMIT=$(STACK_LIMIT) NEG_FILES="$(NEG_FILES)" NOTOURS_FILES="$(NOTOURS_FILES)" sh tools/run-tests.sh "$(VAMOSLIB)" $(CORPUS_FILES)

# EZPK: the C unpack of both shinobi samples must equal tools/unezpk.py's output (hashes committed in
# tools/ezpk-expected.sha256, taken from unezpk.py), byte for byte, with the right sizes.
build/ezpk-unpack: tools/ezpk-unpack.c src/ezpk.c src/ezpk.h $(BZ_C) $(wildcard src/bzip2/*.h)
	@mkdir -p build; $(CC) $(OPT) $(BZDEFS) -DZ_HAVE_UNISTD_H -Isrc/zlib -o $@ tools/ezpk-unpack.c src/ezpk.c $(BZ_C) $(filter-out %/gzlib.c,$(wildcard src/zlib/*.c))
check-ezpk: build/ezpk-unpack
	@mkdir -p build/ezpk; rc=0; \
	for n in gym-shinobi2-01.gym gym-shinobi2-04.gym; do \
	  build/ezpk-unpack corpus/$$n build/ezpk/$$n > build/ezpk/$$n.out 2>&1 || { echo "FAIL ezpk $$n: unpack error: $$(cat build/ezpk/$$n.out)"; rc=1; continue; }; \
	  want=$$(grep " $$n$$" tools/ezpk-expected.sha256 | cut -d' ' -f1); got=$$(shasum -a 256 build/ezpk/$$n | cut -d' ' -f1); \
	  if [ -n "$$want" ] && [ "$$want" = "$$got" ]; then echo "ok   ezpk $$n ($$(cut -d' ' -f1 build/ezpk/$$n.out) bytes, peak $$(cut -d' ' -f3 build/ezpk/$$n.out))"; \
	  else echo "FAIL ezpk $$n: sha256 $$got, expected $$want"; rc=1; fi; \
	done; exit $$rc

check: check-core tests check-v171 check-vendor check-ym2413 check-ezpk

# YM2413 fast core: golden md5, 68k asm == host C, asm linked, cycle sentinel (tools/check-ym2413-fast.sh).
check-ym2413: build/render-host build/render
	@mkdir -p build/ramv; NM68=$(NM68) sh tools/check-ym2413-fast.sh "$(VAMOSRUN)"

# A3: derived VGM 1.71 corpus file reproduces, renders like its 1.50 source, is audible.
check-v171: build/render-ref
	@sh tools/check-v171.sh

# make vendor writes the local src/zlib even when third_party is a symlink (worktrees).
check-vendor:
	@sh tools/check-vendor.sh
