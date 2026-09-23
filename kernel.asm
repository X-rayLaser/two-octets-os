[org 0x7e00]
bits 16

%include "constants.inc"

%macro push_abcd 0
    push ax
    push bx
    push cx
    push dx
%endmacro

%macro pop_dcba 0
    pop dx
    pop cx
    pop bx
    pop ax
%endmacro

%macro detect_command 2
    ; arg1: match string
    ; arg2: address of command to run
    mov si, %1
    call starts_with
    jz %2
%endmacro

mov [boot_drive], dl
call init_daps

; main loop
main:
    ; show prompt
    mov si, prompt
    call print_loop

    ; get user command from keyboard
    call getline
    call run_command

    call carriage_return

    jmp main


run_command:
    mov di, buffer

    detect_command cmd_fsinfo, run_command__fsinfo
    detect_command cmd_echo, run_command__echo
    detect_command cmd_cat, run_command__cat
    detect_command cmd_ls, run_command__ls

    jnz run_command__notfound

    run_command__fsinfo:
        call print_bpb
        jmp run_command__finish

    run_command__echo:
        mov si, buffer
        add si, 4 ; print everything in the buffer past "echo"
        call print_loop
        jmp run_command__finish

    run_command__cat:
        call run_cat
        jmp run_command__finish

    run_command__ls:
        call run_ls
        jmp run_command__finish

    run_command__notfound:
        mov si, cmd_notfound
        call print_loop

    run_command__finish:
        ret


run_cat:
    add si, 4
    ret


run_ls:
    mov si, buffer
    add si, 3

    mov di, path_buffer
    mov cx, 256
    call copy_nbytes

    mov si, di

    call to_upper_case

    call load_directory
    jnz run_ls__maybe_file ; if it fails, the path may refer to a file rather than a directory

    mov si, [directory_buffer_ptr]
    call print_directory
    jmp run_ls__finish

    run_ls__maybe_file:
        mov si, path_buffer
        call split_path

        add si, bx ; start of the file name
        mov di, file_name_buffer
        mov cx, 32
        call copy_nbytes

        ; putting null terminating character (0) at the start of file name portion
        mov al, 0
        mov [si], al

        mov si, path_buffer
        call load_directory ; atempt to load parent directory of whatever is under it

        jnz run_ls__notfound ; if that fails, the path must be wrong

        ; if it succeeds, find the file entry and show file's data

        mov si, file_name_buffer
        call fix_file_name
        mov di, file_name_buffer
        call print_file_entry
        jnz run_ls__notfound ; we have case here that parent directory exists, but there is no target file in it

        jmp run_ls__finish

    run_ls__notfound:
        mov si, file_not_found
        call print_loop

    run_ls__finish:
        ret


itos:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov bx, 0


    test ax, ax
    js itos__signed
    jnz itos__getdigit

    mov dl, '0'
    mov [di], dl
    jmp itos__end

    itos__signed:
        neg ax
        mov dl, '-'
        mov [di], dl
        inc di

    itos__getdigit:
        cmp ax, 0
        je itos__flush
        xor dx, dx
        mov cx, 10
        div cx

        add dl, 48
        mov dh, 0
        push dx
        inc bx
        jmp itos__getdigit

    itos__flush:
        cmp bx, 0
        je itos__end

        pop ax
        stosb
        dec bx
        jmp itos__flush

    itos__end:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret


clear_buffer:
    push ax
    push bx
    push di

    mov cx, BUFFER_SIZE

    clear_buffer__loop:
        cmp cx, 0
        jle clear_buffer__finish

        mov al, 0
        mov [di], al
        inc di
        dec cx
        jmp clear_buffer__loop

    clear_buffer__finish:
        pop di
        pop bx
        pop ax
        ret


%include "file_system.inc"
%include "paths.inc"
%include "strings.inc"
%include "strio.inc"
%include "boot_util.inc"


buffer times BUFFER_SIZE+1 db 0
directory_buffer_ptr dw 0
directory_buffer times 2048 db 0 ; buffer for storing up to 64 entries
path_buffer times 256 db 0 ; buffer for storing file path
file_name_buffer times 256 db 0 

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