#!/bin/bash
dd if=/dev/zero of=two-octets.img bs=1024 count=160
nasm -f bin boot.asm -o boot.bin -l boot.lst
nasm -f bin kernel.asm -o kernel.bin -l kernel.lst
mformat -f 160 -c 2 -B boot.bin -i two-octets.img ::
mcopy -i two-octets.img kernel.bin ::
mcopy -i two-octets.img boot.asm ::
mcopy -i two-octets.img help.txt ::
mcopy -i two-octets.img greeting.txt ::
mcopy -i two-octets.img build.bash ::/build.bsh
mmd -i two-octets.img ::/files ::/files/scripts
mcopy -i two-octets.img build.bash ::/files/scripts/build.bsh
mcopy -i two-octets.img debug.bash ::/files/scripts/debug.bsh

