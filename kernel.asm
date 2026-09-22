[org 0x7e00]
bits 16

BUFFER_SIZE equ 120

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

%macro detect_command 2
    ; arg1: match string
    ; arg2: address of command to run
    mov si, %1
    call starts_with
    jz %2
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

    detect_command cmd_echo, run_command__echo
    detect_command cmd_cat, run_command__cat
    detect_command cmd_ls, run_command__ls

    jnz run_command__notfound

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
        push si
        add si, bx ; start of the file name
        mov di, file_name_buffer
        mov cx, 256
        call copy_nbytes

        ; putting null terminating character (0) at the start of file name portion
        mov al, 0
        mov [si], al

        pop si
        call load_directory ; atempt to load parent directory of whatever is under it

        jnz run_ls__notfound ; if that fails, the path must be wrong

        ; if it succeeds, find the file entry and show file's data
        push si
        mov di, file_name_buffer
        mov si, di
        call fix_file_name

        pop si
        call print_file_entry
        jnz run_ls__notfound ; we have case here that parent directory exists, but there is no target file in it

        jmp run_ls__finish

    run_ls__notfound:
        mov si, file_not_found
        call print_loop

    run_ls__finish:
        ret


print_file_entry:
    push_abcd
    push si
    push di

    mov si, [directory_buffer_ptr]

    print_file_entry__loop:
        mov al, [si]
        or al, al
        jz print_file_entry__notfound

        call check_directory_entry
        jz print_file_entry__found; check if entry found

        add si, 32 ; go to the next entry
        jmp print_file_entry__loop

    print_file_entry__notfound:
        or al, 1 ; clear ZF
        jmp print_file_entry__finish

    print_file_entry__found:
        call print_directory_entry

    print_file_entry__finish:
        pop di
        pop si
        pop_dcba
        ret


load_directory:
    ; copies directory data into directory_buffer and sets ZF on success
    ; si points to the directory path with leading /

    mov di, ROOTDIR_ADDRESS
    mov [directory_buffer_ptr], di

    call ensure_trailing_slash

    ; special case: illegal path without leading /
    mov al, [si]
    cmp al, '/'
    jne load_directory__notfound

    ; special case: directory path is just /
    mov al, [si+1]
    or al, al
    jz load_directory__finish

    inc si ; skip leading '/'

    load_directory__outer_loop:
        ; end of path: 0 after trailing /
        mov al, [si]
        or al, al
        jz load_directory__finish

        mov di, buffer
        call clear_buffer
    
        call parse_path_segment ; next subdirectory name -> buffer
        
        push si
        mov si, buffer
        call fix_file_name
        pop si
        mov di, buffer ; target sub directory name to load

        ; sanity check: buffer must not start with 0
        mov al, [di]
        or al, al
        jz load_directory__notfound

        call load_next_dir
        jnz load_directory__notfound

        jmp load_directory__outer_loop

    load_directory__notfound:
        or al, 1 ; clear ZF
        jmp load_directory__finish

    load_directory__finish:
        ret


load_next_dir:
    ; given a current directory pointed by directory_buffer_ptr, find and load a target subdirectory pointed by di
    ; set ZF on success
    push_abcd
    push si
    push di

    mov si, [directory_buffer_ptr]

    load_next_dir__loop:
        mov al, [si]
        or al, al
        jz load_next_dir__notfound

        call check_directory_entry
        jz load_next_dir__found; check if entry found

        load_next_dir__next_entry:
            add si, 32 ; go to the next entry
            jmp load_next_dir__loop
    
    load_next_dir__notfound:
        or al, 1 ; clear ZF
        jmp load_next_dir__finish

    load_next_dir__found:
        ; check wheither it's a directory rather than file
        mov al, [si+11]
        cmp al, 0x10
        jne load_next_dir__next_entry ; if it's not directory, go back to loop

        mov bx, [si+26] ; get first sector of subdirectory

        ; load directory to be at directory_buffer address
        mov di, directory_buffer
        mov [data_offset], di
        mov [directory_buffer_ptr], di
        call load_file

        xor al, al ; force setting ZF

    load_next_dir__finish:
        pop di
        pop si
        pop_dcba
        ret


split_path:
    ; split path into parent directory path (si) and remaining segment (filename/directory)
    ; stores start position of remaining segment in bx

    push ax
    push cx
    push dx
    push di

    push si
    call ensure_trailing_slash

    mov di, buffer
    call clear_buffer

    inc si

    mov bx, si

    split_path__loop:
        mov al, [si]
        or al, al
        jz split_path__finish

        mov bx, si ; save address
        call parse_path_segment
        jmp split_path__loop

    split_path__finish:
        pop si
        sub bx, si

        pop di
        pop dx
        pop cx
        pop ax
        ret


check_directory_entry:
    ; sets ZF if current directory entry pointed by si contains subdirectory pointed by di
    push_abcd
    push si

    mov cx, 11
    call compare_strings

    pop si
    pop_dcba
    ret


parse_path_segment:
    ; copy the next path segment from source string (si) to destination string (di)
    ; si points at the next segment after this call finishes

    push di

    parse_path_segment__loop:
        lodsb
        cmp al, '/'
        je parse_path_segment__finish

        or al, al
        jz parse_path_segment__finish

        stosb
        jmp parse_path_segment__loop

    parse_path_segment__finish:
        ; always add a trailing 0
        xor al, al
        stosb

        pop di
        ret


to_upper_case:
    ; convert ascii string (with si starting address) letters to be in upper case
    ; in-place operation
    push si

    to_upper_case__loop:
        mov al, [si]
        or al, al
        jz to_upper_case__finish

        mov bl, 97
        cmp al, bl
        jge to_upper_case__ge

        jmp to_upper_case__next

        to_upper_case__ge:
            mov bl, 122
            cmp al, bl
            jg to_upper_case__next

        to_upper_case__lc:
            mov bl, 32
            sub al, bl
            mov [si], al
        
        to_upper_case__next:
            inc si
            jmp to_upper_case__loop

    to_upper_case__finish:
        pop si
        ret


ensure_trailing_slash:
    push si

    mov al, [si]
    or al, al
    jz ensure_trailing_slash__add_slash

    ensure_trailing_slash__loop:
        mov al, [si]

        or al, al
        jz ensure_trailing_slash__string_end

        inc si
        jmp ensure_trailing_slash__loop

    ensure_trailing_slash__string_end:
        mov al, [si-1]
        cmp al, '/'
        je ensure_trailing_slash__finish

    ensure_trailing_slash__add_slash:
        mov al, '/'
        mov [si], al
        inc si
        mov al, 0
        mov [si], al

    ensure_trailing_slash__finish:
        pop si
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


copy_nbytes:
    ; copy up to cx bytes from source string (si) to destination string (di)

    push_abcd
    push si
    push di

    copy_string__loop:
        cmp cx, 0
        jle copy_string__finish

        lodsb
        stosb

        dec cx
        jmp copy_string__loop

    copy_string__finish:
        pop di
        pop si
        pop_dcba
        ret


fix_file_name:
    ; pad the file name with white-spaces to be 8 bytes long
    ; pad the extension with white-spaces to be 3 bytes long
    ; remove the dot if present

    push si
    push di

    call detect_dot; bx will store the index of the dot

    jnz fix_file_name__found_dot

    mov cx, 0

    fix_file_name__nodot:
        cmp cx, 11
        jge fix_file_name__finish
        
        mov al, 0
        cmp [si], al
        jne fix_file_name__nopad
        
        mov al, ' '
        mov [si], al

        fix_file_name__nopad:
            inc cx
            inc si
            jmp fix_file_name__nodot

    fix_file_name__found_dot:
        mov cx, 8
        sub cx, bx

        push si

        add si, bx
        mov bx, cx ; number of white-spaces to insert
        call str_shr_n

        ; point si at the dot character and shift the remaining bytes one to the left (also removes the dot)
        pop si
        add si, 8
        call str_shl_1

        ; ensure null-terminator placed at the end after 11 bytes
        mov al, 0
        mov [si+3], al

    fix_file_name__finish:
        pop di
        pop si
        ret


detect_dot:
    push si

    mov bx, 0

    detect_dot__loop:        
        lodsb

        or al, al
        jz detect_dot__finish

        cmp al, '.'
        je detect_dot__found

        inc bx
        jmp detect_dot__loop

    detect_dot__found:
        or al, 1 ; clear ZF

    detect_dot__finish:
        pop si
        ret


str_shl_1:
    ; shift all string characters (starting with address si) 1 byte to the left
    push si
    mov al, 0
    cmp al, [si]

    je str_shl_1__finish

    inc si

    str_shl_1__do_shift:
        mov al, [si]
        mov [si - 1], al

        or al, al
        jz str_shl_1__finish

        inc si
        jmp str_shl_1__do_shift

    str_shl_1__finish:
        pop si
        ret

str_shr_n:
    ; shift all string characters (starting with address si) n bytes to the right
    ; fill left-most bx bytes with white spaces

    push si
    push di

    push si

    str_shr_n__findend:
        lodsb
        cmp al, 0
        je str_shr_n__past_end

        jmp str_shr_n__findend

    str_shr_n__past_end:
        ; make si lie bx bytes past the string's null terminator
        dec si
        add si, bx

        ; restore initial string address si to di
        pop di

    str_shr_n__do_shift:
        cmp si, di
        jle str_shr_n__fill

        ; move value bx bytes to the left of the current position si
        sub si, bx
        mov al, [si]
        add si, bx
        mov [si], al

        dec si

        jmp str_shr_n__do_shift

    ; put bx white spaces at the start of the string

    str_shr_n__fill:
        cmp bx, 0
        je str_shr_n__finish
        mov al, ' '
        stosb

        dec bx
        jmp str_shr_n__fill

    str_shr_n__finish:
        pop di
        pop si
        ret


compare_strings:
    ; compare up to cx first characters
    push cx
    push si
    push di

    compare_strings__iter:
        or cx, cx
        jz compare_strings__finish
        lodsb
        scasb
        jne compare_strings__finish

        or al, al            ; detect the end of strings
        jz compare_strings__finish

        dec cx
        jmp compare_strings__iter

    compare_strings__finish:
        pop di
        pop si
        pop cx
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
        pop si
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
cmd_echo db "echo ", 0
cmd_cat  db "cat ", 0
cmd_ls   db "ls ", 0