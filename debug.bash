#!/bin/bash
dd if=/dev/zero of=microos.img bs=1024 count=1440
nasm -f bin boot.asm -o boot.bin -l boot.lst
nasm -f bin kernel.asm -o kernel.bin -l kernel.lst
mformat -f 1440 -B boot.bin -i microos.img ::
mcopy -i microos.img kernel.bin ::
mcopy -i microos.img boot.asm ::
mcopy -i microos.img build.bash ::/build.bsh