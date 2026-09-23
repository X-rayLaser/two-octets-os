bits 16
%ifndef strio
%define strio

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

        cmp al, 13  ; check if user pressed 'Enter'
        je getline__finish

        cmp al, 8   ; check if user pressed 'Backspace'
        je getline__handle_backspace

        stosb
        ; show character
        call print_char

        inc cx
        jmp getline__handle_char

        getline__handle_backspace:
            ; if buffer is empty, ignore            
            cmp di, buffer
            je getline__handle_char

            ; eraze last stored character
            dec di
            mov al, 0
            mov [di], al

            mov al, 8
            call print_char

            push cx

            ; save cursor position onto stack
            mov ah, 0x03
            int 0x10 ; save curent cursor row and col into dh:dl
            push dx

            mov al, ' '   ; replace character to be erazed by whitespace
            call print_char

            ; restore cursor position
            pop dx
            mov ah, 0x02
            int 0x10

            pop cx

            jmp getline__handle_char

    getline__finish:
        mov al, 0
        stosb
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