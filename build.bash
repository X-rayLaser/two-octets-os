#!/bin/bash
dd if=/dev/zero of=microos.img bs=1024 count=1440
nasm -f bin boot.asm -o boot.bin
nasm -f bin kernel.asm -o kernel.bin
mformat -f 1440 -c 1 -B boot.bin -i microos.img ::
mcopy -i microos.img kernel.bin ::
mcopy -i microos.img boot.asm ::
mcopy -i microos.img help.txt ::
mcopy -i microos.img greeting.txt ::
mcopy -i microos.img build.bash ::/build.bsh