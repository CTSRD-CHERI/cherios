#!/bin/sh
set -e
BUILDDIR=build

if [ ! -d "$BUILDDIR" ]; then
	mkdir -p $BUILDDIR
	ln -s $BUILDDIR/kernel/cherios.elf .
	cd $BUILDDIR
	cmake -GNinja ..
	cd ..
fi
cd $BUILDDIR
ninja
cd ..
