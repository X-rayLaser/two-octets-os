[org 0x7e00]
bits 16

BUFFER_SIZE equ 120

%macro print_bpb_field 2
    push si
    mov si, %1
    call print_loop
    pop si

    xor ax, ax
    %2
    
    push si

    push ax
    mov di, buffer
    call clear_buffer

    pop ax
    call itos
    mov si, buffer
    call print_loop

    pop si
    call carriage_return
%endmacro

mov [boot_drive], dl
call init_daps

call print_bpb
mov si, root_directory
call print_loop
call print_root_directory

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


jmp hang


run_command:
    mov di, buffer

    mov si, cmd_echo
    call starts_with
    jz run_command__echo

    mov si, cmd_cat
    call starts_with
    jz run_command__cat

    jnz run_command__notfound

    run_command__echo:
        mov si, buffer
        add si, 4 ; print everything in the buffer past "echo"
        jmp run_command__finish

    run_command__cat:
        call run_cat
        jmp run_command__finish

    run_command__notfound:
        mov si, cmd_notfound

    run_command__finish:
        call print_loop
        ret


run_cat:
    add si, 4
    ret


load_directory:
    ; copies directory data into directory_buffer on success and sets ZF

    mov di, buffer
    call parse_path_segment
    
    mov si, ROOTDIR_ADDRESS

    load_directory__loop:
        mov al, [si]
        or al, al
        jz load_directory__notfound

        call check_directory_entry
        jz load_directory__found; check if entry found

        add si, 32 ; go to the next entry
        jmp load_directory__loop
    
    load_directory__notfound:
        or al, 1 ; clear ZF
        jmp load_directory__finish

    load_directory__found:
        ; check wheither it's a directory rather than file
        mov al, [si+11]
        cmp al, 0x10
        jne load_directory__finish ; if it's not directory, set ZF to 0 and quit

        mov bx, [si+26] ; get first sector of subdirectory

        mov cx, [directory_buffer]
        mov [data_offset], cx
        call load_file

    load_directory__finish:
        ret


check_directory_entry:



parse_path_segment:
    ; copy the next path segment from source string to destination string
    ; source string starts with /
    ; si points at the next segment after this call finishes

    push di
    inc si ; trim / at the beginning

    parse_path_segment__loop:
        lodsb
        cmp al, '/'
        je parse_path_segment__finish

        stosb
        jmp parse_path_segment__loop

    parse_path_segment__finish:
        pop di
        ret


starts_with:
    ; sets ZF if the string b (pointed to by di) starts with string a (pointed to by si)
    push si
    push di

    mov al, [si]
    or al, al
    jnz starts_with__iter

    ; if first byte is zero -> clear ZF (doesn't start with other string)
    jmp starts_with__null

    starts_with__iter:
        lodsb

        or al, al            ; if it's the end of string, set ZF
        jz starts_with__finish

        scasb
        jne starts_with__finish ; if characters are not equal, clear ZF

        jmp starts_with__iter

    starts_with__differ:
        or al, 1

    starts_with__null:
        or al, 1 ; forces to set ZF flag to 0

    starts_with__finish:
        pop di
        pop si
        ret


compare_strings:
    push si
    push di

    compare_strings__iter:
        lodsb
        scasb
        jne compare_strings__finish

        or al, al            ; detect the end of strings
        jz compare_strings__finish

        jmp compare_strings__iter

    compare_strings__finish:
        pop di
        pop si
        ret


print_bpb:
    mov si, 0x7c00+0x00B

    print_bpb_field lbl_bytes_per_sector, lodsw
    print_bpb_field lbl_sectors_per_cluster, lodsb
    print_bpb_field lbl_reserved_sectors, lodsw
    print_bpb_field lbl_fats, lodsb
    print_bpb_field lbl_root_entries, lodsw
    print_bpb_field lbl_total_sectors, lodsw
    print_bpb_field lbl_media, lodsb
    print_bpb_field lbl_fat_size, lodsw
    
    ret


print_root_directory:
    mov si, ROOTDIR_ADDRESS
    call print_directory


print_directory:
    jc print_directory__error
    push si

    print_directory__print_entry:
        mov al, [si]
        cmp al, 0
        je print_directory__finish

        call print_directory_entry
        add si, 32  ; go to the next entry

        jmp print_directory__print_entry


    jmp print_directory__finish

    print_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_directory__finish:
        ret


print_directory_entry:
    push ax
    push bx
    push cx
    push dx
    push si
    
    mov di, buffer
    call clear_buffer

    mov cx, 11

    print_directory_entry__getchar:
        cmp cx, 3
        jle print_directory_entry__gotname

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__getchar

    print_directory_entry__gotname:
        mov al, '.'
        mov [di], al
        inc di

    print_directory_entry__get_extension:
        cmp cx, 0
        jle print_directory_entry__finish

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__get_extension

    print_directory_entry__finish:
        mov al, 0x0D
        mov [di], al
        inc di
        mov al, 0x0A
        mov [di], al
        
        mov si, buffer
        call print_loop

        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret


getline:
    push ax
    push bx
    push cx
    push dx
    push si
    push di

    mov di, buffer
    call clear_buffer
    mov di, buffer

    mov cx, 0

    getline__handle_char:
        cmp cx, BUFFER_SIZE
        jge getline__finish

        ; get character
        mov ah, 0x00
        int 0x16

        cmp al, 13  ; check if user pressed 'Enter'
        je getline__finish

        stosb

        ; show character
        xor bx, bx
        mov ah, 0x0E
        int 0x10

        inc cx
        jmp getline__handle_char

    getline__finish:
        mov al, 0
        stosb
        call carriage_return
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret


new_line:
    ; get and save curent cursor row and col into dh:dl
    mov bx, 0
    mov ah, 0x03
    int 0x10

    ; go to next row

    inc dh
    mov dl, 0

    ; set updated cursor position
    mov bx, 0
    mov ah, 0x02
    int 0x10

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


carriage_return:
    mov al, 0x0D
    call print_char
    mov al, 0x0A
    call print_char
    ret


print_char:
    xor bx, bx
    mov ah, 0x0E
    int 0x10
    ret


%include "util.inc"


hang:
    jmp hang

buffer times BUFFER_SIZE+1 db 0
directory_buffer times 2048 db 0 ; buffer for storing up to 64 entries

prompt db "$ ", 0
msg_hello db "Hello from kernel, indeed!", 0x0D, 0x0A, 0
msg_disk_error db "Disk accessing error!", 0x0D, 0x0A, 0
root_directory db "Root directory:", 0x0D, 0x0A, 0

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
cmd_echo db "echo ", 0
cmd_cat  db "cat ", 0