bits 16
%ifndef constants
%define constants

FAT_ADDRESS equ 0x500          ; 8 KB for storing File Allocation Table
ROOTDIR_ADDRESS equ 0x2500     ; 16 KB for storing Root Directory entries
DISK_BUFFER_ADDRESS equ 0x6500 ; remaining address space for storing temp data fetched from disk and for stack
KERNEL_ADDRESS equ 0x7e00      ; loaded kernel will reside in memory directly after bootloader
BUFFER_SIZE equ 120

%endif