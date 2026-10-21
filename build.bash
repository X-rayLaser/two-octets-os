#!/bin/bash
dd if=/dev/zero of=two-octets.img bs=1024 count=1440
nasm -f bin boot.asm -o boot.bin
nasm -f bin kernel.asm -o kernel.bin
mformat -f 1440 -c 1 -B boot.bin -i two-octets.img ::
mcopy -i two-octets.img kernel.bin ::
mcopy -i two-octets.img boot.asm ::
mcopy -i two-octets.img help.txt ::
mcopy -i two-octets.img greeting.txt ::
mcopy -i two-octets.img build.bash ::/build.bsh