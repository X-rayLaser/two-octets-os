[org 0x7c00]

%macro push_abcd 0
    push ax
    push bx
    push cx
    push dx
%endmacro

%macro pop_dcba 0
    pop dx
    pop cx
    pop bx
    pop ax
%endmacro

%macro set_lba_address 1
    ; set LBA address of sector to read
    mov dword [daps+12], 0 ; set field to zero
    mov dword [daps+8], 0  ; set field to zero
    mov [daps+8], %1
%endmacro

%macro read_disk_lba 0
    ; issuing BIOS interrupt for reading disk sectors using LBA addressing
    mov ah, 0x42
    mov dl, [boot_drive] ; set drive number to boot drive
    mov si, daps         ; pass an address of data structure before BIOS call
    int 0x13
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
    mov [boot_drive], dl

    call init_daps
    call get_kernel_first_cluster

    call load_kernel

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


get_kernel_first_cluster:
    push ax
    push cx
    push dx
    push si
    push di

    call read_root_directory

    mov bx, -1  ; if not found, bx = -1

    get_kernel_first_cluster__check_entry:
        mov al, [si]
        cmp al, 0
        je get_kernel_first_cluster__finish

        call is_kernel
        cmp bx, -1
        jne get_kernel_first_cluster__finish

        add si, 32
        jmp get_kernel_first_cluster__check_entry

    get_kernel_first_cluster__finish:
        pop di
        pop si
        pop dx
        pop cx
        pop ax
        ret


is_kernel:
    push ax
    push cx
    push dx
    push di

    push si

    mov di, file_name
    mov cx, 6

    mov bx, -1  ; if not found, bx = -1

    is_kernel__compare_char:
        cmp cx, 0
        je is_kernel__found

        lodsb
        mov dl, [di]

        cmp al, dl
        jne is_kernel__finish

        inc di
        dec cx

        jmp is_kernel__compare_char
    
    is_kernel__found:
        pop si
        mov bx, [si+26]
        push si

    is_kernel__finish:
        pop si

        pop di
        pop dx
        pop cx
        pop ax
        ret


read_root_directory:
    push_abcd
    push di

    ; find address of Root region
    xor ax, ax
    mov al, [0x7c00+0x010] ; # of FATs

    mov bx, [0x7c00+0x016] ; # logical sectors per FAT

    xor dx, dx
    mul bx ; ax = # of sectors occupied by FAT region

    mov bx, [0x7c00+0x00E] ; Reserved logical sectors
    add ax, bx ; ax = first logical sector of root directory

    set_lba_address ax

    ; ax will store size of root directory in bytes
    xor dx, dx
    mov ax, [0x7c00+0x011] ; number of directory entries
    mov bx, 32
    mul bx

    ; ax will store number of logical sectors to read
    xor dx, dx
    mov bx, [0x7c00+0x00B] ; size of logical sector in bytes
    div bx

    ; finalize daps structure
    ; set # of sectors to read
    mov [daps+2], ax

    read_disk_lba

    mov si, 0x7c00+512

    pop di
    pop_dcba
    ret


read_fat:
    push_abcd
    push di

    ; set number of sectors to transfer
    mov bx, [0x7c00+0x016] ; # logical sectors per FAT
    mov [daps+2], bx

    ; set LBA address of sector to read
    mov ax, [0x7c00+0x00E] ; Reserved logical sectors
    set_lba_address ax

    read_disk_lba

    mov si, 0x7c00+512

    pop di
    pop_dcba
    ret


load_kernel:
    ; set number of sectors to transfer
    mov ax, 1
    mov [daps+2], ax

    add bx, 33
    set_lba_address bx

    read_disk_lba

    jc some_error

    mov si, 0x7c00+512
    call print_loop
    jmp load_kernel__finish

    some_error:
        mov si, error
        call print_loop

    load_kernel__finish:
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

boot_drive: db 0
buffer times 30 db 0

; Disk Address Packet Structure
align 4
daps times 16 db 0

file_name db "KERNEL", 0, 0
error db "disk error", 0, 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55