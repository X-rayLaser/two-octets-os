bits 16
%ifndef file_system
%define file_system

%include "constants.inc"

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


search_dir_entry:
    ; on success, sets ZF and sets si to the address of the target entry
    push_abcd
    push di

    mov si, [directory_buffer_ptr]

    search_dir_entry__loop:
        mov al, [si]
        or al, al
        jz search_dir_entry__notfound

        call check_directory_entry
        jz search_dir_entry__finish; check if entry found

        add si, 32 ; go to the next entry
        jmp search_dir_entry__loop

    search_dir_entry__notfound:
        or al, 1 ; clear ZF

    search_dir_entry__finish:
        pop di
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


check_directory_entry:
    ; sets ZF if current directory entry pointed by si contains subdirectory pointed by di
    push_abcd
    push si

    mov cx, 11
    call compare_strings

    pop si
    pop_dcba
    ret


print_root_directory:
    mov si, ROOTDIR_ADDRESS
    call print_directory
    ret


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

%endif