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

    call load_file
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
        mov bx, [si+26-6]  ; cluster no is located at byte 26 from beginning, but we already skipped 6 bytes

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



%include "boot_util.inc"

file_name db "KERNEL", 0, 0

; pad remaining part of sector with 0 and add the signature at the end
times 510-($-$$) db 0
dw 0xAA55
