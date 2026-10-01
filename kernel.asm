[org 0x7e00]
bits 16

%include "constants.inc"
%include "macros.inc"

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
    detect_command cmd_rename, run_command__rename

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

    run_command__rename:
        call run_rename
        jmp run_cat__finish

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
    call file_viewer
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


run_rename:
    mov si, buffer
    add si, 3
    call lstrip

    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case

    ; retrieve into new temp buffer an entry of the file to be renamed
    call find_file ; file entry -> si
    mov cx, 32
    mov di, new_dir_entry_buffer
    call copy_nbytes

    ; prepare new file name and replace old file name with it (in-memory)
    mov si, new_file_name_buffer
    call to_upper_case
    call fix_file_name
    mov cx, 11
    mov di, new_dir_entry_buffer
    call copy_nbytes

    ; provide address of a new entry
    mov [update_entry_structure], di

    ; load parent directory and get it's starting cluster number
    mov si, path_buffer
    call load_directory

    ; set cluster number of the directory whose entry needs update
    mov [update_entry_structure+2], bx

    ; provide old file name to find the entry
    mov si, file_name_buffer
    mov [update_entry_structure+4], si

    ; update the entry and write changes to disk
    call update_directory_entry
    ret


file_viewer:

    file_viewer__loop:
        mov al, [si]
        or al, al
        jz file_viewer__finish
        call show_page

        push si
        mov si, viewer_help_scroll_down
        call print_loop
        call get_char
        call clear_help_line
        pop si
        jmp file_viewer__loop

    file_viewer__finish:
        ret


clear_help_line:
    push ax
    push cx
    
    clear_help_line__loop:
        mov al, 80
        cmp cl, al
        je clear_help_line__finish

        mov al, ' '
        call print_char
        inc cl
        jmp clear_help_line__loop

    clear_help_line__finish:
        mov al, 13
        call print_char
        pop cx
        pop ax
        ret


show_page:
    ; si = start of first byte that belongs to the page
    mov cx, 0

    show_page__loop:
        mov ax, 24
        cmp cx, ax
        je show_page__finish

        mov al, [si]
        or al, al
        jz show_page__finish

        call read_file_line
        inc cx

        push si
        mov si, buffer
        call print_loop
        call carriage_return
        pop si

        jmp show_page__loop

    show_page__finish:
        ret


read_file_line:
    push_abcd
    push di

    mov cx, 0

    push si

    read_file_line__loop:
        call is_newline
        jz read_file_line__line_end

        ; consider every w char sequence (without newline character) a line
        ; where w is display width
        mov ax, 80
        cmp cx, ax
        je read_file_line__line_end

        mov al, [si]
        or al, al
        jz read_file_line__line_end

        inc cx
        inc si

        jmp read_file_line__loop

    read_file_line__line_end:
        mov dx, si ; preserve next line address
        pop si


        mov di, buffer
        call clear_buffer
        call copy_nbytes
        mov si, dx ; restore next line address

    read_file_line__finish:
        pop di
        pop_dcba
        ret


is_newline:
    mov al, 10
    cmp [si], al
    jne is_newline__notfound

    is_newline__found:
        inc si
        and al, 0
        jmp is_newline__finish

    is_newline__notfound:
        or al, 1

    is_newline__finish:
        ret


%include "file_system.inc"
%include "paths.inc"
%include "strings.inc"
%include "diskio.inc"
%include "strio.inc"
%include "misc.inc"
%include "boot_util.inc"

%include "kernel_data.inc"