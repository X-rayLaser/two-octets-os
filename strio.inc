bits 16
%ifndef strio
%define strio

%macro save_cursor 0
    mov ah, 0x03
    int 0x10 ; save curent cursor row and col into dh:dl
    push dx
%endmacro

%macro restore_cursor 0
    pop dx
    mov ah, 0x02
    int 0x10
%endmacro

%include "constants.inc"

getline:
    push_abcd
    push si
    push di

    mov di, buffer
    call clear_buffer
    mov di, buffer

    mov cx, 0
    xor bx, bx

    getline__handle_char:
        cmp cx, BUFFER_SIZE
        jge getline__finish

        push cx
        ; get character
        mov ah, 0x00
        int 0x16
        pop cx

        cmp al, 0
        jne getline__handle_standard_key

        cmp ah, KEY_LEFT_ARROW
        je getline__handle_left_key

        cmp ah, KEY_RIGHT_ARROW
        je getline__handle_right_key

        jmp getline__handle_char

        getline__handle_standard_key:
            cmp al, KEY_ENTER       ; check if user pressed 'Enter'
            je getline__finish

            cmp al, KEY_BACKSPACE   ; check if user pressed 'Backspace'
            je getline__handle_backspace
            
            mov bx, 1
            mov si, di
            call str_shr_n
            xor bx, bx

            stosb

            ; show character
            call print_char

            push cx
            save_cursor
            mov si, di
            call print_string
            restore_cursor

            pop cx

            inc cx
            jmp getline__handle_char

        getline__handle_backspace:
            ; if buffer is empty, ignore            
            cmp di, buffer
            je getline__handle_char

            ; eraze character
            dec di
            mov si, di
            call str_shl_1

            mov al, 8
            call print_char

            push cx
            save_cursor
            call print_string

            mov al, ' '   ; replace character to be erazed by whitespace
            call print_char

            ; restore cursor position
            restore_cursor

            pop cx

            dec cx

            jmp getline__handle_char

        getline__handle_left_key:
            cmp di, buffer
            je getline__handle_char

            dec di

            call step_cursor_left
            jmp getline__handle_char

        getline__handle_right_key:
            ; if position index >= size of string, ignore
            mov ax, di
            sub ax, buffer ; ax = byte's index 
            cmp ax, cx
            jge getline__handle_char

            inc di

            call step_cursor_right
            jmp getline__handle_char
            

    getline__finish:
        call carriage_return
        pop di
        pop si
        pop_dcba
        ret


carriage_return:
    mov al, 0x0D
    call print_char
    mov al, 0x0A
    call print_char
    ret


eraze_last_char:
    push_abcd
    push si
    push di

    mov al, 8
    call print_char

    ; save cursor position onto stack
    save_cursor

    mov al, ' '   ; replace character to be erazed by whitespace
    call print_char

    ; restore cursor position
    restore_cursor

    pop di
    pop si
    pop_dcba
    ret


step_cursor_left:
    push_abcd
    push si
    push di
    
    xor bx, bx
    mov ah, 0x03
    int 0x10 ; get curent cursor row and col into dh:dl
    
    cmp dl, 0
    je step_cursor_left__finish

    dec dl
    mov ah, 0x02
    int 0x10

    step_cursor_left__finish:
        pop di
        pop si
        pop_dcba
        ret


step_cursor_right:
    push_abcd
    push si
    push di
    
    xor bx, bx
    mov ah, 0x03
    int 0x10 ; get curent cursor row and col into dh:dl

    inc dl
    mov ah, 0x02
    int 0x10

    step_cursor_right__finish:
        pop di
        pop si
        pop_dcba
        ret




print_string:
    push_abcd
    push si
    push di

    print_string__loop:
        lodsb
        or al, al
        jz print_string__finish
        mov ah, 0x0E
        int 0x10
        jmp print_string__loop

    print_string__finish:
        pop di
        pop si
        pop_dcba
        ret


print_char:
    push_abcd
    push si
    push di

    xor bx, bx
    mov ah, 0x0E
    int 0x10

    pop di
    pop si
    pop_dcba
    ret

%endif