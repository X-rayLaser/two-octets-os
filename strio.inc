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

    getline__handle_char:
        cmp cx, BUFFER_SIZE
        jge getline__finish

        ; get character
        mov ah, 0x00
        int 0x16

        cmp al, 13  ; check if user pressed 'Enter'
        je getline__finish

        stosb

        ; show character
        call print_char

        inc cx
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
    xor bx, bx
    mov ah, 0x0E
    int 0x10
    ret

%endif