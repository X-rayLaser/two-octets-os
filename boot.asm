[org 0x7c00]

FAT_ADDRESS equ 0x500          ; 8 KB for storing File Allocation Table
ROOTDIR_ADDRESS equ 0x2500     ; 16 KB for storing Root Directory entries
DISK_BUFFER_ADDRESS equ 0x6500 ; remaining address space for storing temp data fetched from disk and for stack
KERNEL_ADDRESS equ 0x7e00      ; loaded kernel will reside in memory directly after bootloader

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
    call read_fat
    call get_kernel_first_cluster

    call load_kernel
    mov dl, [boot_drive]
    jmp KERNEL_ADDRESS


init_daps:
    ; set packet size to 16 bytes
    mov al, 16
    mov [daps], al

    mov ax, DISK_BUFFER_ADDRESS
    call set_transfer_buffer
    ret


get_kernel_first_cluster:

    call read_root_directory

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
    call first_rootdir_sector ; ax = starting sector of root directory
    call set_lba_address

    call sectors_per_rootdir ; ax = # sectors per root directory
    mov [daps+2], ax

    mov ax, ROOTDIR_ADDRESS
    call set_transfer_buffer
    call read_disk_lba

    mov si, ROOTDIR_ADDRESS

    ret


read_fat:
  

    ; set number of sectors to transfer
    mov bx, [0x7c00+0x016] ; # logical sectors per FAT
    mov [daps+2], bx

    ; set LBA address of sector to read
    mov ax, [0x7c00+0x00E] ; Reserved logical sectors
    call set_lba_address

    mov ax, FAT_ADDRESS
    call set_transfer_buffer

    call read_disk_lba

    mov si, FAT_ADDRESS

    ret


load_kernel:
    ; set number of sectors to transfer
    mov ax, 1
    mov [daps+2], ax

    load_kernel__loop:

    push bx
    
    ; ax = (bx - 2) * cluster size
    dec bx
    dec bx
    mov ax, [0x7c00+0x00D] ; number of logical sectors per cluster
    mul bx                 ; ax = offset of cluster's first sector

    push ax

    ; ax = first_rootdir_sector + sectors_per_rootdir
    call first_rootdir_sector ; -> ax
    mov cx, ax
    call sectors_per_rootdir ; -> ax
    add ax, cx

    pop cx ; restore ax (cluster's offset) to cx
    add ax, cx

    call set_lba_address

    pop bx ; restore original bx (with 2-based indexing)

    mov ax, KERNEL_ADDRESS
    add ax, [data_offset]
    call set_transfer_buffer

    call read_disk_lba
    jc some_error

    mov ax, [data_offset]
    add ax, 512
    mov [data_offset], ax

    call read_next_cluster ; bx = next cluster number

    cmp bx, 0xfff
    je load_kernel__finish
    cmp bx, 0xff0f

    je load_kernel__finish
    jmp load_kernel__loop

    some_error:
        mov si, error
        call print_loop

    load_kernel__finish:
        ret


read_next_cluster:
    ; FAT row number: ax = bx // 2; dl = bx % 2
    xor dx, dx
    mov ax, bx
    mov cx, 2
    div cx

    push dx
    ; FAT row address: ax = ax * 3 + FAT offset
    xor dx, dx
    mov cx, 3
    mul cx
    mov si, FAT_ADDRESS
    add si, ax

    pop dx

    xor ax, ax
    xor bx, bx

    cmp dl, 0
    jne read_next_cluster__odd

    read_next_cluster__even:
        xor dx, dx

        mov bl, [si]
        add si, 1
        mov al, [si]

        ; clear all most significant bits, multiply bu 256
        shl ax, 12
        shr ax, 4

        add bx, ax

        jmp read_next_cluster__finish

    read_next_cluster__odd:
        xor dx, dx
        add si, 1

        ; todo verify
        mov bl, [si]
        shr bl, 4

        add si, 1
        mov al, [si]
        mov cx, 256
        mul cx
        add bx, ax

    read_next_cluster__finish:
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
data_offset: dw 0

; Disk Address Packet Structure
align 4
daps times 16 db 0

file_name db "KERNEL", 0, 0
error db "HDE", 0, 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55
