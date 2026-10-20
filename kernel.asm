[org 0x7e00]
bits 16

%include "constants.inc"
%include "macros.inc"

mov [boot_drive], dl
call init_daps
call heap_init
jmp shell_entry_point


%include "shell.inc"
%include "file_system.inc"
%include "file_viewer.inc"
%include "dirs.inc"
%include "fat.inc"
%include "paths.inc"
%include "strings.inc"
%include "diskio.inc"
%include "strio.inc"
%include "misc.inc"
%include "heap.inc"
%include "boot_util.inc"

%include "kernel_data.inc"