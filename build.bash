#!/bin/bash
dd if=/dev/zero of=microos.img bs=1024 count=1440
nasm -f bin boot.asm -o boot.bin
mformat -f 1440 -B boot.bin -i microos.img ::
