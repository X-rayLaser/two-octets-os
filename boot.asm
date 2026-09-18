[org 0x7c00]
bits 16

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

    mov ax, KERNEL_ADDRESS
    add ax, [data_offset]
    call set_transfer_buffer

    call read_disk_lba
    jc some_error

    mov ax, [data_offset]
    add ax, 512
    mov [data_offset], ax

    call get_next_cluster_num ; bx = next cluster number

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


%include "util.inc"


file_name db "KERNEL", 0, 0
error db "HDE", 0

data_offset: dw 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55
