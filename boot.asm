[org 0x7c00]

%macro macro_print_byte_field 2
    mov si, %1
    call print_loop
    call print_separator
    
    xor ax, ax
    mov al, [%2]
    call print_field_value

    call carriage_return
%endmacro

%macro macro_print_word_field 2
    mov si, %1
    call print_loop
    call print_separator
    
    mov ax, [%2]
    call print_field_value

    call carriage_return
%endmacro

jmp short main
nop

; those 59 bytes will be filled by mformat to contain BPB 
times 59 db 0

main:
    ; initialize data and stack segments to 0 and setup stack
    cli ; disable interrupts
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti ; enable interrupts

    call show_BPB
    jmp hang


show_BPB:
    macro_print_word_field logical_sector, 0x7C0B
    macro_print_byte_field cluster_size, 0x7C0D
    ret


print_separator:
    mov al, ':'
    call print_char
    mov al, ' '
    call print_char
    ret

print_field_value:
    ; value is looked up in ax
    mov di, buffer
    call clear_buffer
    call itos
    mov si, di
    call print_loop
    ret


carriage_return:
    mov al, 0x0D
    call print_char

    mov al, 0x0A
    call print_char
    ret

clear_buffer:
    push ax
    push bx
    push di

    mov cx, 60

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

print_char:
    mov ah, 0x0E
    int 0x10
    ret


print_loop:
    lodsb
    or al, al
    jz print__finish
    mov ah, 0x0E
    int 0x10
    jmp print_loop

    print__finish:
        ret


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

hang:
    jmp hang

msg_hello db "Bootloader via mtools is running!", 0x0D, 0x0A, 0
buffer times 60 db 0
buffer2 times 60 db 0

logical_sector db "Logical sector size", 0
cluster_size db "Cluster size", 0


; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55