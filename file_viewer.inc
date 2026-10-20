bits 16
%ifndef file_viewer_module
%define file_viewer_module


file_viewer:
    push_registers

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
        pop_registers
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
        pop si

        ; if last character was \n, process it
        mov al, 10
        cmp [si-1], al
        jne show_page__continue
        call carriage_return

        show_page__continue:
        

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

%endif