bits 16
%ifndef file_system
%define file_system


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


fs_list:
    ; si is address of path
    allocate_frame 2

    call copy_string   ; di -> path copy
    mov si, di
    mov [bp], di

    call to_upper_case

    call load_directory
    jnz fs_list__maybe_file ; if it fails, the path may refer to a file rather than a directory

    mov si, [directory_buffer_ptr]
    call print_directory
    jmp fs_list__finish

    fs_list__maybe_file:
        mov si, [bp]             ; restore path copy address to si

        call find_file

        jnz fs_list__notfound ; we have case here that parent directory exists, but there is no target file in it

        ; file entry found, display it
        call print_directory_entry

        jmp fs_list__finish

    fs_list__notfound:
        mov si, file_not_found
        call print_loop

    fs_list__finish:
        mov si, [bp]
        call heap_free
        erase_frame 2
        ret


fs_rename:
    ; si is address of arg string
    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case
    mov si, path_buffer

    ; retrieve into new temp buffer an entry of the file to be renamed
    call find_file ; file entry -> si
    jnz fs_rename__error

    mov cx, 32
    mov di, new_dir_entry_buffer
    call copy_nbytes

    ; prepare new file name and replace old file name with it (in-memory)

    mov si, [argv_pointers+4]
    call to_upper_case
    call fix_file_name
    mov cx, 11
    mov di, new_dir_entry_buffer
    call copy_nbytes

    ; check if parent directory is root directory
    mov si, path_buffer
    mov cl, 0
    cmp [si+1], cl
    je fs_rename__root

    fs_rename__non_root:
        ; handle editing non root directory entry

        ; provide address of a new entry
        mov [update_entry_structure], di

        ; load parent directory and get it's starting cluster number
        mov si, path_buffer
        call load_directory
        jnz fs_rename__error

        ; set cluster number of the directory whose entry needs update
        mov [update_entry_structure+2], bx

        ; provide old file name to find the entry
        mov si, file_name_buffer
        mov [update_entry_structure+4], si

        ; update the entry and write changes to disk
        call update_directory_entry

        jmp fs_rename__finish

    fs_rename__root:
        ; handle editing root directory entry
        mov si, new_dir_entry_buffer
        mov di, file_name_buffer
        call update_root_entry

        jnz fs_rename__error
        jmp fs_rename__finish

    fs_rename__error:
        mov si, file_not_found
        call print_loop

    fs_rename__finish:
        ret


fs_mkfile:
    ; si is address of path
    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case
    mov si, path_buffer

    call create_file
    jz fs_mkfile__finish

    fs_mkfile__error:
        mov si, file_not_found
        call print_loop
    
    fs_mkfile__finish:
        ret


fs_mkdir:
    ; si is address of path
    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case
    mov si, path_buffer

    call create_directory
    jz fs_mkdir__finish

    fs_mkdir__error:
        mov si, file_not_found
        call print_loop
    
    fs_mkdir__finish:
        ret


fs_cat:
    ; si is address of path
    mov di, path_buffer
    mov cx, BUFFER_SIZE
    call copy_nbytes

    mov si, di

    call to_upper_case
    mov si, path_buffer

    call find_file

    jnz fs_cat__notfound

    mov bx, [si+26]
    mov di, file_buffer
    mov [data_offset], di
    call load_file
    mov si, file_buffer
    call file_viewer
    jmp fs_cat__finish

    fs_cat__notfound:
        mov si, file_not_found
        call print_loop

    fs_cat__finish:
        ret

%endif