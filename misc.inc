bits 16
%ifndef misc
%define misc

%include "constants.inc"

itos:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov bx, 0


    test ax, ax
    js itos__signed
    jnz itos__getdigit

    mov dl, '0'
    mov [di], dl
    jmp itos__end

    itos__signed:
        neg ax
        mov dl, '-'
        mov [di], dl
        inc di

    itos__getdigit:
        cmp ax, 0
        je itos__flush
        xor dx, dx
        mov cx, 10
        div cx

        add dl, 48
        mov dh, 0
        push dx
        inc bx
        jmp itos__getdigit

    itos__flush:
        cmp bx, 0
        je itos__end

        pop ax
        stosb
        dec bx
        jmp itos__flush

    itos__end:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret


clear_buffer:
    push ax
    push bx
    push di

    mov cx, BUFFER_SIZE

    clear_buffer__loop:
        cmp cx, 0
        jle clear_buffer__finish

        mov al, 0
        mov [di], al
        inc di
        dec cx
        jmp clear_buffer__loop

    clear_buffer__finish:
        pop di
        pop bx
        pop ax
        ret


%endif