#!/bin/bash
dd if=/dev/zero of=microos.img bs=1024 count=160
nasm -f bin boot.asm -o boot.bin -l boot.lst
nasm -f bin kernel.asm -o kernel.bin -l kernel.lst
mformat -f 160 -c 2 -B boot.bin -i microos.img ::
mcopy -i microos.img kernel.bin ::
mcopy -i microos.img boot.asm ::
mcopy -i microos.img help.txt ::
mcopy -i microos.img build.bash ::/build.bsh
mmd -i microos.img ::/files ::/files/scripts
mcopy -i microos.img build.bash ::/files/scripts/build.bsh
mcopy -i microos.img debug.bash ::/files/scripts/debug.bsh

