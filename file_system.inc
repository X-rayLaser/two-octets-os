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

    call split_and_copy_new ; si -> parent_path, di -> child
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
        ; handle editing non root directory entry

        ; provide address of a new entry
        mov di, [bp + 6]
        mov [update_entry_structure], di

        ; load parent directory and get it's starting cluster number
        mov si, [bp + 2]
        call load_directory
        jnz fs_rename__error

        ; set cluster number of the directory whose entry needs update
        mov [update_entry_structure+2], bx

        ; provide old file name to find the entry
        mov si, [bp + 4]
        mov [update_entry_structure+4], si

        ; update the entry and write changes to disk
        call update_directory_entry

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
    mov cx, 32
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
    call create_file
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
    call create_directory
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

%endif