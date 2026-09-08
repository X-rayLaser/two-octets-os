[org 0x7c00]

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
    mov [boot_drive], dl

    call init_daps
    call print_root_directory

    jmp hang


init_daps:
    ; set packet size to 16 bytes
    mov al, 16
    mov [daps], al

    ; transfer buffer address segment:offset defined right after the end of bootloader code in memory
    mov bx, 0x7c00+512
    mov [daps+4], bx ; lower 16 bits for offset
    mov bx, 0
    mov [daps+6], bx ; higher 16 bits for data segment
    ret


print_root_directory:
    ; find address of Root region
    xor al, al
    mov al, [0x7c00+0x010] ; # of FATs

    mov bx, [0x7c00+0x016] ; # logical sectors per FAT

    xor dx, dx
    mul bx ; ax = # of sectors occupied by FAT region

    mov bx, [0x7c00+0x00E] ; Reserved logical sectors
    add ax, bx ; ax = first logical sector of root directory

    ; set LBA address of sector to read
    mov [daps+8], ax

    ; ax will store size of root directory in bytes
    xor dx, dx
    mov ax, [0x7c00+0x011] ; number of directory entries
    mov bx, 32
    mul bx

    ; ax will store number of logical sectors to read
    xor dx, dx
    mov bx, [0x7c00+0x00B] ; size of logical sector in bytes
    div bx


    mov di, buffer
    call clear_buffer
    call itos
    mov si, di
    call print_loop

    ; finalize daps structure
    ; set # of sectors to read
    mov [daps+2], ax

    ; issuing BIOS interrupt for reading disk sectors using LBA addressing
    mov ah, 0x42
    mov dl, [boot_drive] ; set drive number to floppy disk A
    int 0x13

    jc print_root_directory__error
    mov si, msg_success
    call print_loop

    jmp print_root_directory__finish

    print_root_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_root_directory__finish:
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

boot_drive: db 0
msg_disk_error db "Disk accessing error!", 0x0D, 0x0A, 0
msg_success db "Disk read succeeded!", 0x0D, 0x0A, 0
msg_hello db "Bootloader via mtools is running!", 0x0D, 0x0A, 0
buffer times 60 db 0

; Disk Address Packet Structure
align 4
daps times 16 db 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55