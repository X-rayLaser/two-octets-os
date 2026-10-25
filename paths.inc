bits 16
%ifndef paths
%define paths


split_path:
    ; split si path into parent (si) / child (di)
    ; returns
    ;   si: address of newly created parent path
    ;   di: address of newly created child name
    push_abcd
    allocate_frame 4 ; [path copy, parent path]

    call copy_string ; di -> copy
    mov si, di
    mov [bp + 0], si

    call get_last_path_segment_index

    mov cx, bx
    call get_string_prefix
    mov [bp + 2], di
    call get_string_suffix ; -> di (child)

    mov si, [bp + 2]       ; parent path

    free_frame_ptr 0
    erase_frame 4
    pop_dcba
    ret


get_last_path_segment_index:
    ; split path into parent directory path and remaining segment (filename/directory)
    ; stores start position of remaining segment in bx
    push ax
    push cx
    push dx
    push di
    push si

    allocate_frame 4 ; [path copy ending with /, segment buffer]

    call ensure_trailing_slash
    mov si, di
    mov [bp + 0], si

    mov cx, MAX_FILENAME_LENGTH
    call heap_alloc
    mov [bp + 2], di

    inc si
    mov bx, si

    get_last_path_segment_index__loop:
        mov al, [si]
        or al, al
        jz get_last_path_segment_index__finish

        mov bx, si ; save address
        call parse_path_segment
        jmp get_last_path_segment_index__loop

    get_last_path_segment_index__finish:
        mov si, [bp + 0]
        sub bx, si

        free_frame_ptr 0
        free_frame_ptr 1
        erase_frame 4
        
        pop si
        pop di
        pop dx
        pop cx
        pop ax
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


ensure_trailing_slash:
    ; makes a copy of a si string and appends it with slash if one's missing
    ; returns di, string with trailing slash
    push_abcd

    mov al, [si]
    or al, al
    jz ensure_trailing_slash__add_slash
    push si

    ensure_trailing_slash__loop:
        mov al, [si]

        or al, al
        jz ensure_trailing_slash__string_end

        inc si
        jmp ensure_trailing_slash__loop

    ensure_trailing_slash__string_end:
        mov al, [si-1]
        pop si
        cmp al, '/'
        je ensure_trailing_slash__just_copy

    ensure_trailing_slash__add_slash:
        mov al, '/'
        call create_appended_string
        jmp ensure_trailing_slash__finish

    ensure_trailing_slash__just_copy:
        call copy_string

    ensure_trailing_slash__finish:
        pop_dcba
        ret


remove_trailing_slash:
    ; si stores address of a string
    push_registers

    ; corner case: empty string
    mov al, [si]
    or al, al
    jz remove_trailing_slash__finish

    remove_trailing_slash__loop:
        mov al, [si]
        or al, al
        jz remove_trailing_slash__string_end

        inc si
        jmp remove_trailing_slash__loop

    remove_trailing_slash__string_end:
        ; if last non-zero byte was /, replace it with 0
        dec si
        mov al, [si]
        cmp al, '/'
        jne remove_trailing_slash__finish

        mov al, 0
        mov [si], al

    remove_trailing_slash__finish:
        pop_registers
        ret


fix_file_name:
    ; pad the file name with white-spaces to be 8 bytes long
    ; pad the extension with white-spaces to be 3 bytes long
    ; remove the dot if present

    push_registers

    ; preserve original string address
    push si
    ; allocate temporary string big enough to account for right byte shifts
    mov cx, MAX_FILENAME_LENGTH
    call heap_alloc
    call get_string_length
    call copy_nbytes
    mov si, di

    ; preserve new string address
    push di

    call detect_dot; bx will store the index of the dot

    jnz fix_file_name__found_dot

    mov di, si
    ; final address
    add si, 11

    fix_file_name__nodot:
        mov al, [di]
        or al, al
        jz fix_file_name__pad

        inc di
        jmp fix_file_name__nodot

    fix_file_name__pad:
        cmp di, si
        jge fix_file_name__add_null

        mov al, ' '
        stosb
        jmp fix_file_name__pad

    fix_file_name__add_null:
        mov al, 0
        mov [si], al
        jmp fix_file_name__finish

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
        ; copy fixed string into old string (assumes old string is at least 8+3+1 bytes long)
        pop si ; new fixed string address
        pop di ; original string address
        mov cx, 12       ; 8+3 bytes for name+extention followed by \0
        call copy_nbytes

        ; we just copied modified string bytes, so it can be deleted
        call heap_free

        pop_registers
        ret


is_parent_root:
    ; set ZF if parent path of si is just /
    push_registers
    call split_path
    mov al, [si+1]

    ; free parent
    call heap_free

    ; free file name
    mov si, di
    call heap_free

    or al, al
    pop_registers
    ret


is_valid_path:
    ; set ZF if path, si, ends with valid file name
    push_registers
    allocate_frame 2 ; [file name]

    call split_path

    mov [bp + 0], di

    ; ignoring the file's directory path
    call heap_free
    
    mov si, di
    call is_valid_file_name
    jnz is_valid_path__invalid

    mov ax, 0
    jmp is_valid_path__finish

    is_valid_path__invalid:
        mov ax, 1

    is_valid_path__finish:
        free_frame_ptr 0
        erase_frame 2

        or ax, ax
        pop_registers
        ret


is_valid_file_name:
    ; set ZF if file name, si, is valid (8+3)
    push_registers
    allocate_frame 6 ; [full name copy, name, extension]

    call copy_string
    mov si, di
    mov [bp + 0], di

    call remove_trailing_slash

    ; test if string is empty
    mov al, [si]
    or al, al
    jz is_valid_file_name__empty

    ; split into name (si) and extension (di)
    call split_file_name
    mov [bp + 2], si
    mov [bp + 4], di

    ; invalid if name > 8 characters
    call count_string_characters
    cmp cx, 8
    jg is_valid_file_name__invalid

    ; invalid if extension > 3 characters
    mov si, di
    call count_string_characters
    cmp cx, 3
    jg is_valid_file_name__invalid

    mov ax, 0
    jmp is_valid_file_name__cleanup

    is_valid_file_name__invalid:
        mov ax, 1 ; clear ZF

    is_valid_file_name__cleanup:
        free_frame_ptr 0
        free_frame_ptr 1
        free_frame_ptr 2
        jmp is_valid_file_name__finish

    is_valid_file_name__empty:
        free_frame_ptr 0
        mov ax, 1

    is_valid_file_name__finish:
        erase_frame 6
        or ax, ax ; set ZF as needed
        pop_registers
        ret


split_file_name:
    ; split file name si into name part (si) and extension part (di)
    push_abcd
    allocate_frame 6 ; [original file name, name part, extension len]

    mov [bp + 0], si

    call detect_dot
    jnz split_file_name__found_dot

    call get_string_length
    call heap_alloc
    call copy_nbytes
    mov si, di

    ; return empty string in di (just null terminator)
    mov cx, 1
    call heap_alloc
    jmp split_file_name__finish

    split_file_name__found_dot:
        ; allocate bx bytes for name part, plus 1 for null terminator
        mov cx, bx
        inc cx
        call heap_alloc ; -> di

        ; copy bx bytes of name to name part
        dec cx
        mov si, [bp + 0]
        call copy_nbytes
        mov [bp + 2], di

        ; create another empty string for the extention
        ; extension part len = total len - name len - 1 (the dot)
        mov si, [bp + 0]
        call get_string_length
        sub cx, bx
        dec cx
        mov [bp + 4], cx
        call heap_alloc
        
        ; go to the start of the extention and copy extension to di
        mov si, [bp + 0]
        add si, bx
        inc si           ; skip the dot
        mov cx, [bp + 4]
        call copy_nbytes

        mov si, [bp + 2]

    split_file_name__finish:
        erase_frame 6
        pop_dcba
        ret


%endif