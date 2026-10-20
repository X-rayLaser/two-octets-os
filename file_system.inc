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


%macro get_upper_case_path 0
    ; expects si to point to original path
    call copy_string
    mov si, di
    call to_upper_case
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

    get_upper_case_path
    mov [bp], si

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
    allocate_frame 8; [path_copy, parent_path, child, new_entry_buffer]

    get_upper_case_path
    mov [bp + 0], si

    call split_path ; si -> parent_path, di -> child
    mov [bp + 2], si
    mov [bp + 4], di
    mov si, di
    call fix_file_name

    mov si, [bp + 0]
    ; retrieve into new temp buffer an entry of the file to be renamed
    call find_file ; file entry -> si
    jnz fs_rename__error

    mov di, [argv_pointers+4]
    call fs_replace_entry_filename
    mov [bp + 6], di

    ; check if parent directory is root directory
    mov si, [bp + 2]
    mov cl, 0
    cmp [si+1], cl
    je fs_rename__root

    fs_rename__non_root:
        ; handle editing non root directory 
        mov si, [bp + 0]
        mov di, [bp + 6]
        call update_non_root_entry
        jnz fs_rename__error
        jmp fs_rename__finish

    fs_rename__root:
        ; handle editing root directory entry
        mov si, [bp + 6]
        mov di, [bp + 4]
        call update_root_entry

        jnz fs_rename__error
        jmp fs_rename__finish

    fs_rename__error:
        mov si, file_not_found
        call print_loop

    fs_rename__finish:
        free_frame_ptr 0
        free_frame_ptr 1
        free_frame_ptr 2
        free_frame_ptr 3
        erase_frame 8
        ret


fs_replace_entry_filename:
    ; si: edited entry address
    ; di: new file name
    ; returns di: updated entry buffer in memory
    push di

    ; copy 32 bytes from old entry (si) to buffer (di)s
    mov cx, DIRECTORY_ENTRY_SIZE
    call heap_alloc ; -> di
    call copy_nbytes

    ; prepare new file name and replace old file name with it (in-memory)

    pop si ; si = new file name
    call to_upper_case
    call fix_file_name
    mov cx, 11
    call copy_nbytes
    ret


fs_mkfile:
    ; si is address of path
    get_upper_case_path

    push si

    call is_parent_root
    jz fs_mkfile__root

    call create_file
    jz fs_mkfile__finish
    jmp fs_mkfile__error

    fs_mkfile__root:
        call create_file_under_root
        jz fs_mkfile__finish

    fs_mkfile__error:
        mov si, file_not_found
        call print_loop
    
    fs_mkfile__finish:
        pop si
        call heap_free
        ret


fs_mkdir:
    ; si is address of path
    get_upper_case_path

    push si
    
    call is_parent_root
    jz fs_mkdir__root

    call create_directory
    jz fs_mkdir__finish
    jmp fs_mkdir__error

    fs_mkdir__root:
        call create_dir_under_root
        jz fs_mkdir__finish

    fs_mkdir__error:
        mov si, file_not_found
        call print_loop
    
    fs_mkdir__finish:
        pop si
        call heap_free
        ret


fs_cat:
    ; si is address of path
    allocate_frame 4   ; [file path, file data buffer]

    get_upper_case_path
    mov [bp + 0], si

    call find_file

    jnz fs_cat__notfound

    mov bx, [si+26]
    mov cx, 4096
    call heap_alloc ; -> di
    mov [bp + 2], di

    mov [data_offset], di
    call load_file

    mov si, [bp + 2]  ; point to file buffer
    call file_viewer
    jmp fs_cat__finish

    fs_cat__notfound:
        mov si, file_not_found
        call print_loop

    fs_cat__finish:
        ; freeing path memory
        mov si, [bp + 0]
        call heap_free

        ; freeing file's data memory
        mov si, [bp + 2]
        call heap_free
        erase_frame 4
        ret


fs_file_append:
    ; si is address of file path
    ; di is address of a string to be appended
    allocate_frame 10 ; [file path, appended string, file entry, appended length, new entry]

    push di
    get_upper_case_path
    pop di

    mov [bp + 0], si
    mov [bp + 2], di

    ; set appending string
    mov [file_append_struct], di

    ; read file's entry, set starting cluster no
    call find_file
    jnz fs_file_append__not_found
    mov [bp + 4], si
    mov bx, [si+26]
    mov [file_append_struct+2], bx

    ; set appended string length
    mov si, [bp + 2]
    call get_string_length
    mov [bp + 6], cx
    mov [file_append_struct+4], cx

    ; set current file length
    mov si, [bp + 4]
    mov ax, [si+28]
    mov [file_append_struct+6], ax

    ; replace null terminator with newline
    mov di, [bp + 2]
    mov cx, [bp + 6]
    dec cx
    add di, cx
    mov al, 10 ; \n
    mov [di], al

    ; copy file's entry to temp buffer
    mov si, [bp + 4]
    mov cx, DIRECTORY_ENTRY_SIZE
    call heap_alloc
    call copy_nbytes
    mov [bp + 8], di

    ; modify buffer's length field
    mov ax, [di+28]
    mov cx, [bp + 6]
    add ax, cx
    mov [di+28], ax

    ; write a string to the end of file
    call append_file
    jnz fs_file_append__error

    mov si, [bp + 0]
    call is_parent_root
    jz fs_file_append__root

    fs_file_append__non_root:
        ; update file's entry and persist it on disk
        mov si, [bp + 0]
        mov di, [bp + 8]
        call update_non_root_entry
        jz fs_file_append__finish
        jmp fs_file_append__error

    fs_file_append__root:
        mov si, [bp + 0]
        call split_path ; si = /, di = file name
        call heap_free

        ; set file name of entry to update
        mov si, di
        call remove_trailing_slash
        call fix_file_name
        mov di, si

        ; set new entry buffer
        mov si, [bp + 8]

        call update_root_entry
        jz fs_file_append__root_ok

        mov si, di
        call heap_free
        jmp fs_file_append__error

        fs_file_append__root_ok:
            mov si, di
            call heap_free
            jmp fs_file_append__finish

    fs_file_append__error:
        mov si, fwrite_error
        call print_loop
        jmp fs_file_append__finish

    fs_file_append__not_found:
        mov si, file_not_found
        call print_loop

    fs_file_append__finish:
        free_frame_ptr 4
        erase_frame 10
        ret

%endif