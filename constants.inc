bits 16
%ifndef constants
%define constants

FAT_ADDRESS equ 0x500          ; 8 KB for storing File Allocation Table
ROOTDIR_ADDRESS equ 0x2500     ; 16 KB for storing Root Directory entries
DISK_BUFFER_ADDRESS equ 0x6500 ; remaining address space for storing temp data fetched from disk and for stack
KERNEL_ADDRESS equ 0x7e00      ; loaded kernel will reside in memory directly after bootloader
BUFFER_SIZE equ 256
TOTAL_HEAP_SIZE equ 2048

LOWER_CASE_A equ 97
UPPER_CASE_A equ 65

KEY_ENTER equ 13
KEY_BACKSPACE equ 8

KEY_UP_ARROW equ 72
KEY_DOWN_ARROW equ 80
KEY_LEFT_ARROW equ 75
KEY_RIGHT_ARROW equ 77
%endif