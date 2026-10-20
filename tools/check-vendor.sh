#!/bin/sh
# make vendor must write src/zlib of the tree it runs in, also when third_party is a
# symlink to another checkout (worktrees share third_party). The old rule did
# `cd $(ZLIB) && cp ... ../../src/zlib/`, which resolves through the symlink and wrote
# (or failed on) the other checkout's src/zlib.
# usage: check-vendor.sh
set -e
R=$(pwd)
T=${VENDOR_TMP:-$R/build/vendor-check}; rm -rf $T; mkdir -p $T/other/third_party $T/tree
cp -R third_party/zlib-1.3.1 $T/other/third_party/
ln -s $R/third_party/gme $T/other/third_party/gme
ln -s $T/other/third_party $T/tree/third_party
cp Makefile $T/tree/; cp -R patches $T/tree/
make -C $T/tree vendor > $T/vendor.log 2>&1 || { echo "FAIL vendor: make vendor failed in a tree with symlinked third_party (see $T/vendor.log)"; exit 1; }
[ -f $T/tree/src/zlib/inflate.c ] || { echo "FAIL vendor: src/zlib not written in the tree that ran make vendor"; exit 1; }
[ ! -e $T/other/src ] || { echo "FAIL vendor: make vendor wrote into the symlink target checkout"; exit 1; }
rm -rf $T
echo "ok vendor: symlinked third_party writes local src/zlib"
