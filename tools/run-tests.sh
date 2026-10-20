#!/bin/sh
# usage: [STACK_LIMIT=n] [NEG_FILES="..."] run-tests.sh "<vamos cmd incl. LIBS assign>" corpus-files...
# Cases per file (resumable: build/tests/<name>.<case>.ok|fail; delete a file to re-run it):
#   a  OpenLibrary v1 + Open + GetInfo run (the driver opens the library at version 1)
#   b  -i info (songs, durations, loop, text) == host
#   c  10 s 44100 S16BE render == host
#   d  song index 1 render == host (file with one song: ok, nothing to compare)
#   e  Seek to 30000 ms, GetPosition within one Render block (47 ms) of 30000
#   f  GetStats renders > 0 and bytes > 0 after case c (library code ran)
#   g  tools/stackprobe: caller stack used per LVO, all < STACK_LIMIT; numbers in n.g.out
#   h  IsOurFile accepts the file, also from only its first $PROBE bytes (HippoPlayer's
#      detection probe); pseudo-files neg-sap and neg-mod are rejected
#   i  S8 at 28150 Hz == host
#   j  mute mask 0x0E == host
#   k  case c again with an 8 KB vamos stack (-s 8: the 6144-byte STACK guarantee
#      plus the driver's own frames; HippoPlayer's sample process has 8192) == host
#   h  also: NOTOURS_FILES (AY-only VGM) give isour=0
#   l  files in NEG_FILES: Open fails cleanly (exit 20, "open failed")
VAMOS="$1"; shift
STACK_LIMIT=${STACK_LIMIT:-6144}
PROBE=1084	# bytes HippoPlayer reads for format detection (puu016.s loadfile)
T=build/tests; mkdir -p $T
# vamos treats leading-dash args as its own unless they follow --
run_lib() { $VAMOS -- build/vgm2raw "$@"; }
rec() { # name case rc
  rm -f $T/$1.$2.ok $T/$1.$2.fail
  if [ "$3" = 0 ]; then touch $T/$1.$2.ok; else touch $T/$1.$2.fail; fi
}
# first differing byte offset, for the failure evidence
cmpcase() { # name case ; expects $T/n.c.lib.raw and $T/n.c.ref.raw
  if [ -s $T/$1.$2.lib.raw ] && cmp -s $T/$1.$2.lib.raw $T/$1.$2.ref.raw; then rec $1 $2 0
  else rec $1 $2 1; cmp $T/$1.$2.lib.raw $T/$1.$2.ref.raw > $T/$1.$2.diff 2>&1; ls -l $T/$1.$2.lib.raw $T/$1.$2.ref.raw >> $T/$1.$2.diff 2>&1; fi
  rm -f $T/$1.$2.lib.raw $T/$1.$2.ref.raw
}
for f in "$@"; do
  n=$(basename $f)
  if [ ! -f $T/$n.c.ok ] || [ ! -f $T/$n.f.ok ] || [ ! -f $T/$n.a.ok ]; then
    rm -f $T/$n.c.lib.raw; run_lib $f $T/$n.c.lib.raw -1 10000 44100 s16 > $T/$n.c.out 2>&1; rc=$?
    build/render-ref $f $T/$n.c.ref.raw -1 10000 44100 s16 > /dev/null 2>&1
    cmpcase $n c
    if [ $rc = 0 ]; then rec $n a 0; else rec $n a 1; fi
    r=$(sed -n 's/.*stats opens=\([0-9]*\) renders=\([0-9]*\) bytes=\([0-9]*\).*/\2 \3/p' $T/$n.c.out)
    rn=${r% *}; by=${r#* }
    if [ -n "$rn" ] && [ "$rn" -gt 0 ] && [ "$by" -gt 0 ]; then rec $n f 0; else rec $n f 1; fi
  fi
  if [ ! -f $T/$n.b.ok ]; then
    run_lib -i $f > $T/$n.b.lib.txt 2>&1
    build/render-ref -i $f > $T/$n.b.ref.txt 2>&1
    if [ -s $T/$n.b.ref.txt ] && cmp -s $T/$n.b.lib.txt $T/$n.b.ref.txt; then rec $n b 0
    else rec $n b 1; diff $T/$n.b.lib.txt $T/$n.b.ref.txt | head -5 > $T/$n.b.diff; fi
  fi
  if [ ! -f $T/$n.d.ok ]; then
    songs=$(build/render-ref -i $f | sed -n 's/^songs=\([0-9]*\).*/\1/p')
    if [ -n "$songs" ] && [ "$songs" -gt 1 ]; then
      rm -f $T/$n.d.lib.raw; run_lib $f $T/$n.d.lib.raw 1 10000 44100 s16 > $T/$n.d.out 2>&1
      build/render-ref $f $T/$n.d.ref.raw 1 10000 44100 s16 > /dev/null 2>&1
      cmpcase $n d
    elif [ -n "$songs" ]; then rec $n d 0; else rec $n d 1; fi
  fi
  if [ ! -f $T/$n.e.ok ]; then
    run_lib -p 30000 $f - -1 120000 44100 s16 > $T/$n.e.out 2>&1
    p=$(sed -n 's/^pos=\(-*[0-9]*\)$/\1/p' $T/$n.e.out)
    if [ -n "$p" ] && [ "$p" -ge 29953 ] && [ "$p" -le 30047 ]; then rec $n e 0; else rec $n e 1; fi
  fi
  if [ ! -f $T/$n.g.ok ]; then
    $VAMOS -- build/stackprobe $f > $T/$n.g.out 2>&1
    if awk -v lim=$STACK_LIMIT '/^stack /{ok=1; for(i=2;i<=NF;i++){split($i,kv,"="); if(kv[2]+0>=lim) ok=0}; found=1} END{exit !(found&&ok)}' $T/$n.g.out; then rec $n g 0; else rec $n g 1; fi
  fi
  if [ ! -f $T/$n.h.ok ]; then
    run_lib -o $f > $T/$n.h.out 2>&1
    run_lib -O $PROBE $f >> $T/$n.h.out 2>&1
    if [ "$(grep -c '^isour=1$' $T/$n.h.out)" = 2 ]; then rec $n h 0; else rec $n h 1; fi
  fi
  if [ ! -f $T/$n.i.ok ]; then
    rm -f $T/$n.i.lib.raw; run_lib $f $T/$n.i.lib.raw -1 10000 28150 s8 > /dev/null 2>&1
    build/render-ref $f $T/$n.i.ref.raw -1 10000 28150 s8 > /dev/null 2>&1
    cmpcase $n i
  fi
  if [ ! -f $T/$n.j.ok ]; then
    rm -f $T/$n.j.lib.raw; run_lib -m 14 $f $T/$n.j.lib.raw -1 10000 44100 s16 > /dev/null 2>&1
    build/render-ref -m 14 $f $T/$n.j.ref.raw -1 10000 44100 s16 > /dev/null 2>&1
    cmpcase $n j
  fi
  if [ ! -f $T/$n.k.ok ]; then
    rm -f $T/$n.k.lib.raw; $VAMOS -s 8 -- build/vgm2raw $f $T/$n.k.lib.raw -1 10000 44100 s16 > $T/$n.k.out 2>&1
    build/render-ref $f $T/$n.k.ref.raw -1 10000 44100 s16 > /dev/null 2>&1
    cmpcase $n k
  fi
done
# IsOurFile must reject a SAP and a MOD (case h, pseudo-files)
# vamos only reaches files under the pl: volume, so the SAP is copied next to the MOD
SAP=$(ls ${SAP_CORPUS:-../playsap.library/corpus}/*.sap 2>/dev/null | head -1)
[ -n "$SAP" ] && cp "$SAP" $T/neg.sap && SAP=$T/neg.sap
dd if=/dev/zero of=$T/neg.mod bs=1084 count=1 2>/dev/null; printf 'M.K.' | dd of=$T/neg.mod bs=1 seek=1080 conv=notrunc 2>/dev/null
for pair in "neg-sap:$SAP" "neg-mod:$T/neg.mod"; do
  n=${pair%%:*}; p=${pair#*:}
  if [ ! -f $T/$n.h.ok ]; then
    run_lib -o $p > $T/$n.h.out 2>&1
    if [ -n "$p" ] && grep -q '^isour=0$' $T/$n.h.out; then rec $n h 0; else rec $n h 1; fi
  fi
done
for p in $NEG_FILES; do
  n=$(basename $p)
  if [ ! -f $T/$n.l.ok ]; then
    run_lib $p $T/$n.l.raw -1 10000 44100 s16 > $T/$n.l.out 2>&1; rc=$?
    if [ $rc = 20 ] && grep -q 'open failed' $T/$n.l.out; then rec $n l 0; else rec $n l 1; fi
    rm -f $T/$n.l.raw
  fi
done
# files with no GME chip: IsOurFile must answer 0 (case h), also from a probe
for p in $NOTOURS_FILES; do
  n=$(basename $p)
  if [ ! -f $T/$n.h.ok ]; then
    run_lib -o $p > $T/$n.h.out 2>&1
    run_lib -O $PROBE $p >> $T/$n.h.out 2>&1
    if [ "$(grep -c '^isour=0$' $T/$n.h.out)" = 2 ]; then rec $n h 0; else rec $n h 1; fi
  fi
done
# stack numbers: maximum over every probed file and LVO
cat $T/*.g.out 2>/dev/null | awk '/^stack /{for(i=2;i<=NF;i++){split($i,kv,"="); if(kv[2]+0>m)m=kv[2]+0}} END{print "stack max=" m+0}' > $T/stackmax.txt
cat $T/stackmax.txt
fail=0
for c in a b c d e f g h i j k l; do
  ok=$(ls $T/*.$c.ok 2>/dev/null | wc -l); bad=$(ls $T/*.$c.fail 2>/dev/null | wc -l)
  echo "tests case $c: ok=$ok fail=$bad"
  [ $bad = 0 ] || { fail=1; ls $T/*.$c.fail | xargs -n1 basename; }
done
exit $fail
