#!/bin/sh
rm -fr obj
mkdir -p obj obj/fs obj/kernel obj/prga obj/uart obj/libuser obj/libs obj/sockets || exit 1
gmake -B $1 $2 $3 $4 || exit 1
ln -s obj/cherios.elf
