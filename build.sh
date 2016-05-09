#!/bin/sh
set -e

BUILDDIR=build
mkdir -p $BUILDDIR
cd $BUILDDIR
cmake -GNinja ..
ninja
cd ..
ln -s $BUILDDIR/kernel/cherios.elf .
