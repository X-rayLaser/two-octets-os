bits 16
%ifndef paths
%define paths


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


%endif