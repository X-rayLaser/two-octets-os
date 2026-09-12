set architecture i8086
target remote localhost:1234
break *0x7c00
continue

layout asm
layout regs