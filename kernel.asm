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
        call lstrip
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
    mov si, buffer
    add si, 4
    call lstrip

    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case
    mov si, path_buffer

    call find_file

    jnz run_cat__notfound

    mov bx, [si+26]
    mov di, file_buffer
    mov [data_offset], di
    call load_file
    mov si, file_buffer
    call print_loop
    jmp run_cat__finish

    run_cat__notfound:
        mov si, file_not_found
        call print_loop


    run_cat__finish:
        ret


run_ls:
    mov si, buffer
    add si, 3
    call lstrip

    mov di, path_buffer
    mov cx, BUFFER_SIZE
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

        call find_file

        jnz run_ls__notfound ; we have case here that parent directory exists, but there is no target file in it

        ; file entry found, display it
        call print_directory_entry

        jmp run_ls__finish

    run_ls__notfound:
        mov si, file_not_found
        call print_loop

    run_ls__finish:
        ret


find_file:
    ; attempts to find a parent directory of the file and locate it's entry
    ; si is the address of the byte string containing complete file path
    ; sets ZF and sets si to the address of the file's entry on success
    push_abcd
    push di

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
    call search_dir_entry

    find_file__finish:
        pop di
        pop_dcba
        ret


%include "file_system.inc"
%include "paths.inc"
%include "strings.inc"
%include "strio.inc"
%include "misc.inc"
%include "boot_util.inc"

%include "kernel_data.inc"