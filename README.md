# TwoOctets OS

A tiny, toy 16-bit operating system running in x86 Real Mode. It boots from a custom VBR directly into a minimalist CLI shell with FAT-12 filesystem support.

---

### ⚠️ EDUCATIONAL EXPERIMENT - NOT FOR PRODUCTION USE

> **THIS PROJECT IS TEEMING WITH BUGS BY DESIGN.**  
> **IT WAS BUILT SOLELY FOR THE SAKE OF LEARNING AND HOBBY EXPERIMENTATION.**  
>
> * **RUN ONLY IN EMULATORS (QEMU / VIRTUALBOX).**  
> * **NEVER ATTEMPT TO RUN THIS OPERATING SYSTEM ON ACTUAL HARDWARE.**  
>
> Because this OS operates in 16-bit Real Mode without memory protection, virtual memory, or safety rails, it is highly unstable. Buggy low-level disk writes (`fwrite`/`rename`) or corrupt segment registers could potentially corrupt or overwrite data on your modern physical drives if run on real hardware. 
>
> This is a fragile educational playground, not a stable system. The author takes **no responsibility** for any hardware damage, data loss, or bricked systems. Use at your own risk inside a strictly isolated virtual environment.

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

## Known Quirks & Experimental "Features"
Because this is a non-production toy operating system, it handles edge cases in simple or unconventional ways. Expect the following behaviors and bugs when testing the shell:

### Filesystem & Design Constraints
* **Unix-Style Paths on FAT-12:** The shell uses Unix forward slashes (`/`) for paths and treats the unpartitioned volume as a single unified root directory. Drive letters (like `A:\`) are completely omitted.
* **Spaced-Out Filenames (8.3 Padding):** The `ls` command displays entries in their raw FAT-12 directory structure. Filenames are padded to fit fixed 11-byte boundaries (e.g., printing `FILE    .TXT` instead of trimming it down to `FILE.TXT`).
* **No Duplicate Checking:** The OS does not cross-reference existing entries when adding files, meaning duplicate files or folders can be created within the same directory.
* **Strict 4 KB File Limit:** The `cat` command utilizes a hardcoded buffer length. Trying to load or read any file longer than 4 KB will result in truncation.

### Input & Shell Validation Bugs
* **The Eager Pager (`cat` bug):** The file viewer forces a `"Press any key to scroll down"` prompt at the end of a file, even if the entire content already fits perfectly on the screen.
* **Missing Newline Mess:** If a text file lacks a trailing newline character, the pager prompt will blend directly into the last line of text instead of breaking to a fresh line.
* **No Argument Verification:** Commands do not validate the number of arguments supplied. Running commands with missing or extra inputs can cause undefined behavior.
* **Loose Path Validation:** Path parsing logic lacks robust verification. The kernel may blindly accept malformed strings or invalid filenames without throwing an error.

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

This project is free and unencumbered software released into the public domain under **The Unlicense** - see the [LICENSE](LICENSE) file for details.
