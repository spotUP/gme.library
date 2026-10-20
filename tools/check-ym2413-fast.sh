#!/bin/sh
# YM2413 (emu2413) fast core: fixed-point resampler timing in C plus the 68k asm
# OPLL_calc tick loop, putData, getData FIR, commit_slot_update and update_output
# (src/gme/ext/emu2413_68k.s, patches/0003). Checks:
#  1. host C render is byte-identical to the pre-ab86516 golden (md5 of 20 s);
#  2. the 68k build (asm path, under vamos) renders byte-identical to the host C path;
#  3. emu2413.o links the fused asm OPLL_calc (nm: opll_calc_68k), which runs the
#     tick loop, update_output (ampm, noise, update_slots, channel output), the
#     asm commit_slot_update, putData and the getData FIR;
#  4. speed sentinel: Musashi cycle count for 2 s at 22050 Hz stays under CYCLE_CAP.
#     Measured: soft-float original 1977M, C fixed-point 1271M, asm update_slots
#     1137M, asm update_output 969M, fused asm OPLL_calc/FIR 947M, asm
#     commit_slot_update 904M, cached phase increment + skipped idempotent commit
#     564M, frozen-slot flag (skip commit/envelope of muted or rate-0 SUSTAIN/RELEASE
#     slots) 467M, unrolled 18-slot loop with envelope subroutine 429M. The cap fails a revert to the previous stage and every earlier one.
# usage: check-ym2413-fast.sh "<vamos run command>"
set -e
VAMOSRUN=${1:-vamos -C 68020 -m 16384 -s 512 -H disable}
CYCLE_CAP=${CYCLE_CAP:-431000000}
F=corpus/vgm-ym2413.vgm
T=build/ym2413-fast; rm -rf $T; mkdir -p $T
fail=0
for p in 22050:034c2756caec17601b7a4cc7362b6f5f 11025:a78c6f35b59bcd9d6c5c656b8af2e452; do
  r=${p%%:*}; want=${p#*:}
  build/render-host $F 0 20000 $r $T/h.$r.raw > /dev/null 2>&1
  got=$(md5 -q $T/h.$r.raw 2>/dev/null || md5sum $T/h.$r.raw | cut -d' ' -f1)
  [ "$got" = "$want" ] && echo "ok ym2413 golden $r" || { echo "FAIL ym2413 golden $r: $got != $want"; fail=1; }
done
cp $F $T/in.vgm
out=$($VAMOSRUN -l main:info build/render $T/in.vgm 0 2000 22050 $T/a.raw 2>&1 | sed -n 's/.*total cycles: \([0-9]*\).*/\1/p')
head -c 176400 $T/h.22050.raw > $T/h2.raw
cmp -s $T/a.raw $T/h2.raw && echo "ok ym2413 asm == C (68k vs host, 2 s 22050)" || { echo "FAIL ym2413 asm output differs from host C"; fail=1; }
$NM68 build/68k/emu2413.o | grep -q ' U _opll_calc_68k' && echo "ok ym2413 asm linked" || { echo "FAIL ym2413 emu2413.o does not call _opll_calc_68k"; fail=1; }
if [ -n "$out" ] && [ "$out" -lt "$CYCLE_CAP" ]; then echo "ok ym2413 cycles $out < $CYCLE_CAP"; else echo "FAIL ym2413 cycles '$out' >= $CYCLE_CAP"; fail=1; fi
exit $fail
