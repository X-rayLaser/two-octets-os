[org 0x7c00+0x2800]

mov [boot_drive], dl
call init_daps
mov si, msg_hello
call print_loop
call print_root_directory

jmp hang

init_daps:
    ; set packet size to 16 bytes
    mov al, 16
    mov [daps], al

    mov ax, 0x7c00+512
    call set_transfer_buffer
    ret


print_root_directory:
    call read_root_directory

    jc print_root_directory__error
    push si
    mov si, root_directory
    call print_loop
    pop si

    print_root_directory__print_entry:
        mov al, [si]
        cmp al, 0
        je print_root_directory__finish

        call print_directory_entry
        add si, 32  ; go to the next entry

        jmp print_root_directory__print_entry


    jmp print_root_directory__finish

    print_root_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_root_directory__finish:
        ret



print_directory_entry:
    push ax
    push bx
    push cx
    push dx
    push si
    
    mov di, buffer
    call clear_buffer

    mov cx, 11

    print_directory_entry__getchar:
        cmp cx, 3
        jle print_directory_entry__gotname

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__getchar

    print_directory_entry__gotname:
        mov al, '.'
        mov [di], al
        inc di

    print_directory_entry__get_extension:
        cmp cx, 0
        jle print_directory_entry__finish

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__get_extension

    print_directory_entry__finish:
        mov al, 0x0D
        mov [di], al
        inc di
        mov al, 0x0A
        mov [di], al
        
        mov si, buffer
        call print_loop

        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret



read_root_directory:
    call first_rootdir_sector ; ax = starting sector of root directory
    call set_lba_address

    call sectors_per_rootdir ; ax = # sectors per root directory
    mov [daps+2], ax

    mov ax, 0x7c00+512
    call set_transfer_buffer
    call read_disk_lba

    mov si, 0x7c00+512

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



clear_buffer:
    push ax
    push bx
    push di

    mov cx, 30

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



first_rootdir_sector:
    ; find address of Root region
    xor ax, ax
    mov al, [0x7c00+0x010] ; # of FATs

    mov bx, [0x7c00+0x016] ; # logical sectors per FAT

    xor dx, dx
    mul bx ; ax = # of sectors occupied by FAT region

    mov bx, [0x7c00+0x00E] ; Reserved logical sectors
    add ax, bx ; ax = first logical sector of root directory
    ret


sectors_per_rootdir:
    ; ax will store size of root directory in bytes
    xor dx, dx
    mov ax, [0x7c00+0x011] ; number of directory entries
    mov bx, 32
    mul bx

    ; ax will store number of logical sectors in root directory
    xor dx, dx
    mov bx, [0x7c00+0x00B] ; size of logical sector in bytes
    div bx
    ret


set_transfer_buffer:
    ; transfer buffer address segment:offset defined right after the end of bootloader code in memory
    ; assumes that segment:offset is stored in ax

    mov [daps+4], ax ; lower 16 bits for offset
    mov ax, 0
    mov [daps+6], ax ; higher 16 bits for data segment
    ret


set_lba_address:
    ; set LBA address of sector to read
    mov dword [daps+12], 0 ; set field to zero
    mov dword [daps+8], 0  ; set field to zero
    mov [daps+8], ax
    ret


read_disk_lba:
    ; issuing BIOS interrupt for reading disk sectors using LBA addressing
    mov ah, 0x42
    mov dl, [boot_drive] ; set drive number to boot drive
    mov si, daps         ; pass an address of data structure before BIOS call
    int 0x13
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

hang:
    jmp hang

buffer times 30 db 0
msg_hello db "Hello from kernel!", 0x0D, 0x0A, 0
msg_disk_error db "Disk accessing error!", 0x0D, 0x0A, 0
root_directory db "Root directory:", 0x0D, 0x0A, 0


boot_drive: db 0
data_offset: dw 0

; Disk Address Packet Structure
align 4
daps times 16 db 0
