bits 16
%ifndef utils
%define utils

%include "constants.inc"

get_next_cluster_num:
    ; FAT row number: ax = bx // 2; dl = bx % 2
    xor dx, dx
    mov ax, bx
    mov cx, 2
    div cx

    push dx
    ; FAT row address: ax = ax * 3 + FAT offset

    mov cx, 3
    mul cx
    mov si, FAT_ADDRESS
    add si, ax

    pop dx

    cmp dl, 0
    jne get_next_cluster_num__odd

    get_next_cluster_num__even:
        lodsw
        mov bx, 0x0fff
        and bx, ax
        jmp get_next_cluster_num__finish

    get_next_cluster_num__odd:
        add si, 1
        lodsw
        shr ax, 4
        mov bx, ax

    get_next_cluster_num__finish:
        ret


load_file:
    ; set number of sectors to transfer to equal cluster size (in sectors)
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov [daps+2], ax

    load_file__loop:

    push bx
    
    ; ax = (bx - 2) * cluster size
    dec bx
    dec bx
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
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

    mov ax, [data_offset]
    call set_transfer_buffer

    call read_disk_lba
    jc some_error

    ; increment data_offset (location of next cluster in RAM) by cluster_size * logical_sector_size
    mov ax, [0x7c00+0x00B] ; bytes per logical sector
    mov cx, [0x7c00+0x00D] ; logical sectors per cluster
    mul cx

    mov cx, [data_offset]
    add ax, cx
    mov [data_offset], ax

    call get_next_cluster_num ; bx = next cluster number

    cmp bx, 0xfff
    je load_file__finish
    cmp bx, 0xff0f

    je load_file__finish
    jmp load_file__loop

    some_error:
        mov si, error
        call print_loop

    load_file__finish:
        ret


init_daps:
    ; set packet size to 16 bytes
    mov al, 16
    mov [daps], al

    mov ax, DISK_BUFFER_ADDRESS
    call set_transfer_buffer
    ret


first_rootdir_sector:
    ; find address of Root region
    xor ax, ax
    mov al, [0x7c00+0x010] ; # of FATs

    mov bx, [0x7c00+0x016] ; # logical sectors per FAT

    mul bx ; ax = # of sectors occupied by FAT region

    mov bx, [0x7c00+0x00E] ; Reserved logical sectors
    add ax, bx ; ax = first logical sector of root directory
    ret


sectors_per_rootdir:
    ; ax will store size of root directory in bytes
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
    ; convert logical sector to physical LBA address and store it in the DAPS structure

    push ax
    ; convert logical sector to physical
    mov ah, 48h                   ; Get Drive Parameters
    mov dl, [boot_drive]          ; Drive index
    mov si, DISK_BUFFER_ADDRESS   ; response data structure address
    int 13h

    jc set_lba_address__fallback
    mov cx, [DISK_BUFFER_ADDRESS + 18] ; bytes per physical sector
    jmp set_lba_address__finish

    set_lba_address__fallback:
        mov cx, 512

    set_lba_address__finish:
        xor dx, dx
        mov ax, [0x7c00+0x00B] ; bytes per logical sector
        div cx

        ; ax = logical sector * logical sector size
        mov cx, ax
        pop ax  ; restore logical sector address
        mul cx

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


boot_drive: db 0

; Disk Address Packet Structure
align 4
daps times 16 db 0
error db "HDE", 0

data_offset: dw KERNEL_ADDRESS
%endif