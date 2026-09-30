bits 16
%ifndef diskio
%define diskio


get_cluster_remaining_space:
    ; compute unused space in bytes in the last file's cluster
    ; dx: total file size in bytes
    ; ax: will store remaining space in bytes
    push bx
    push cx
    push dx

    call get_cluster_size ; ax = bytes per cluster
    
    ; ax = file size / cluster size in bytes
    mov bx, ax
    mov ax, dx
    xor dx, dx
    div bx

    ; bx now stores remaining unused space in a cluster
    sub bx, dx
    mov ax, bx

    pop dx
    pop cx
    pop bx
    ret


get_cluster_size:
    ; compute total cluster size in bytes
    ; store result in ax
    push bx

    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov bx, [0x7c00+0x00B] ; bytes per logical sector
    mul bx                 ; ax = bytes per cluster
    
    pop bx
    ret


update_cluster:
    ; read cluster from disk, modify it in-memory, write it back to disk
    ; si: address of a byte string to modify cluster with
    ; bx: cluster number (2-based)
    ; cx: number of bytes to write
    ; dx: cluster's byte offset
    ; set number of sectors to transfer to equal cluster size (in sectors)
    push_abcd
    push si
    push di

    call read_cluster

    mov di, cluster_buffer
    add di, dx
    call copy_nbytes

    call write_disk_lba
    jc update_cluster__disk_error

    jmp update_cluster__finish

    update_cluster__disk_error:

    update_cluster__finish:
        pop di
        pop si
        pop_dcba
        ret


read_cluster:
    ; bx: cluster number (2-based)
    push_abcd
    push si
    push di

    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov [daps+2], ax

    call set_cluster_address

    mov ax, cluster_buffer
    call set_transfer_buffer

    call read_disk_lba

    pop di
    pop si
    pop_dcba
    ret


set_cluster_address:
    ; locate first physical sector of a cluster and 
    ; bx: cluster number (2-based)
    push_abcd
    push si
    push di

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
    pop di
    pop si
    pop_dcba
    ret


write_disk_lba:
    ; issuing BIOS interrupt for writing disk sectors using LBA addressing
    push_abcd
    push si
    push di

    mov ah, 0x43
    mov dl, [boot_drive] ; set drive number to boot drive
    mov si, daps         ; pass an address of data structure before BIOS call
    int 0x13

    pop di
    pop si
    pop_dcba
    ret

%endif