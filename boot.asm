[org 0x7c00]

jmp short start
nop

; those 59 bytes containing BPB will be ignored by mformat
times 59 db 0

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov si, msg_hello
print_loop:
    lodsb
    or al, al
    jz hang
    mov ah, 0x0E
    int 0x10
    jmp print_loop

hang:
    jmp hang

msg_hello db "Bootloader via mtools is running!", 0x0D, 0x0A, 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55