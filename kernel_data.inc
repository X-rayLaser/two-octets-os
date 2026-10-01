
%ifndef kernel_data
%define kernel_data

%include "constants.inc"

buffer times BUFFER_SIZE + 1 db 0
second_buffer times BUFFER_SIZE + 1 db 0
directory_buffer_ptr dw 0

; todo: add 2 byte alignment for transfer buffers
directory_buffer times 2048 db 0 ; buffer for storing up to 64 entries
file_buffer times 4096 db 0      ; buffer for storing up to 4 KB of files data
null_buffer times 4096 db 0      ; buffer of zeros to prepare allocated clusters with size up to 4 KB
path_buffer times BUFFER_SIZE + 1 db 0 ; buffer for storing file path
file_name_buffer times BUFFER_SIZE + 1 db 0

parent_path_buffer times BUFFER_SIZE + 1 db 0 ; another buffer for storing path to the parent directory
subdir_name_buffer times BUFFER_SIZE + 1 db 0 ; another buffer for storing subdirectory name
new_dir_entry_buffer times BUFFER_SIZE + 1 db 0 ; buffer for storing a new directory entry

test_directory_name db "FILES      ", 0
prompt db "$ ", 0
msg_hello db "Hello from kernel, indeed!", 0x0D, 0x0A, 0
msg_disk_error db "Disk accessing error!", 0x0D, 0x0A, 0
root_directory db "Root directory:", 0x0D, 0x0A, 0
file_not_found db "File or directory not found", 0x0D, 0x0A, 0

; DOS 2.0 BPB Labels (FAT-12)
lbl_bytes_per_sector:         db "Bytes per logical sector:    ", 0
lbl_sectors_per_cluster:      db "Logical sectors per cluster: ", 0
lbl_reserved_sectors:         db "Reserved logical sectors:    ", 0
lbl_fats:                     db "Number of FATs:              ", 0
lbl_root_entries:             db "Root directory entries:      ", 0
lbl_total_sectors:            db "Total logical sectors:       ", 0
lbl_media:                    db "Media descriptor (Hex):      ", 0
lbl_fat_size:                 db "Logical sectors per FAT:     ", 0


cmd_notfound db "Command not found", 0x0D, 0x0A, 0

; supported commands
cmd_fsinfo db "fsinfo", 0
cmd_echo db "echo ", 0
cmd_cat  db "cat ", 0
cmd_ls   db "ls ", 0

; file viewer help
viewer_help_scroll_down db "Press any key to scroll down", 13, 0

file_append_struct times 8 db 0
cluster_args_struct times 8 db 0 ; data structure storing arguments to update_cluster
update_entry_structure times 8 db 0 
; data buffer address | cluster number | count | byte offset within cluster

cluster_buffer times 4096 db 0 ; buffer for storing cluster data in memory

%endif