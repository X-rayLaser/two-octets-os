# TwoOctets OS

A tiny, toy 16-bit operating system running in x86 Real Mode. It boots from a custom VBR directly into a minimalist CLI shell with FAT-12 filesystem support.

---

### ⚠️ IMPORTANT DISCLAIMER & WARNING

> **RUN ONLY IN EMULATORS (QEMU / VIRTUALBOX).**  
> **NEVER ATTEMPT TO RUN THIS OPERATING SYSTEM ON ACTUAL HARDWARE.**  
>
> This project is a **Work in Progress (WIP)** and contains bugs. Because it operates in 16-bit Real Mode without memory protection or safety rails, buggy low-level disk writes (`fwrite`/`rename`) or corrupt segment registers could potentially corrupt or overwrite data on your modern physical drives if run on real hardware.  
> 
> The author takes **no responsibility** for any hardware damage, data loss, or bricked systems. Use at your own risk inside a strictly isolated virtual environment.

---

## Features
* **16-bit Pure Assembly:** Written 100% in x86 assembly language (`nasm`), focusing on bare-metal logic and direct BIOS interrupts.
* **16-bit Real Mode:** Operates strictly within 16-bit CPU registers (exactly *Two Octets* wide).
* **Volume Boot Record (VBR):** The bootloader fits into the first 512-byte sector of an unpartitioned disk image to initialize the system and load the kernel.
* **FAT-12 File Manager:** Full integration with classic 1.44MB disk structures using `mtools`.

## Shell Commands

```text
Supported commands and their usage:

  fsinfo                         Display information about the filesystem.
  help                           Display this help menu.
  echo    [arg1 arg2 ...]        Print arguments to the standard output.
  cat     <path/to/file>         Display the contents of a file.
  ls      <path/to/dir>          List files and subdirectories in a directory.
  mkfile  <path/to/newfile>      Create a new empty file.
  mkdir   <path/to/newdir>       Create a new directory.
  fwrite  <path/to/file> <str>   Write a string to a specified file.
  rename  <path/to/file> <name>  Rename a file or directory to a new name.

Note: Arguments in <angle brackets> are required. Arguments in [square brackets] are optional.
```

## How to Build

The OS utilizes `nasm` for assembly and `mtools` to format and inject files into a raw 1.44MB floppy image. Run the provided build script:

```bash
chmod +x build.bash
./build.bash
```

*The script automates creating a zeroed image, compiling `boot.bin` and `kernel.bin`, formatting it with FAT-12, and copying essential files (`boot.asm`, `help.txt`, `greeting.txt`) into the image.*

## How to Run

Execute the compiled image safely using QEMU:

```bash
qemu-system-i386 -drive format=raw,file=two-octets.img
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
