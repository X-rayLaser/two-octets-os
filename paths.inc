bits 16
%ifndef paths
%define paths


split_path:
    ; split si path into parent (si) / child (di)
    ; returns
    ;   si: address of newly created parent path
    ;   di: address of newly created child name
    push_abcd
    call copy_string ; di -> copy

    mov si, di
    push si

    mov cx, MAX_FILENAME_LENGTH
    call heap_alloc  ; di -> new_buffer
    call get_last_path_segment_index
    add si, bx ; start of the file name
    mov cx, MAX_FILENAME_LENGTH
    call copy_nbytes
    ; putting null terminating character (0) at the start of file name portion
    mov al, 0
    mov [si], al
    pop si

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
    call ensure_trailing_slash

    mov di, buffer
    call clear_buffer

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
        pop si
        sub bx, si

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

    push si
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
        pop di
        pop si
        ret


%endif