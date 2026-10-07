bits 16
%ifndef diskio
%define diskio

%macro cluster_rw 1
    push_registers

    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov [daps+2], ax

    call set_cluster_address

    mov ax, cluster_buffer
    call set_transfer_buffer

    call %1

    pop_registers
%endmacro


%macro logical_sector_rw 1
    push_registers

    ; specify LBA disk address
    call set_lba_address

    ; specify # of physical sectors to process
    call get_logical_sector_size
    mov [daps+2], ax

    ; specify buffer address
    mov ax, sector_buffer
    call set_transfer_buffer

    call %1

    ; todo: catch read error

    pop_registers
%endmacro


get_cluster_remaining_space:
    ; compute unused space in bytes in the last file's cluster
    ; dx: total file size in bytes
    ; ax: will store remaining space in bytes
    push bx
    push cx
    push dx

    call get_cluster_size ; cx = bytes per cluster
    
    ; ax = file size / cluster size in bytes
    mov ax, dx
    xor dx, dx
    div cx

    ; bx now stores remaining unused space in a cluster
    sub cx, dx
    mov ax, bx

    pop dx
    pop cx
    pop bx
    ret


get_last_cluster:
    ; find the last cluster in the chain
    ; accepts initial cluster number bx
    ; updates bx with number of found cluster

    get_last_cluster__loop:
        push bx ; memorize current cluster number
        call get_next_cluster_num

        cmp bx, 0xfff
        je get_last_cluster__finish
        cmp bx, 0xff0f
        je get_last_cluster__finish

        pop cx ; forget it
        jmp get_last_cluster__loop

    get_last_cluster__finish:
        pop bx ; result
        ret


get_cluster_size:
    ; compute total cluster size in bytes
    ; store result in cx
    push ax
    push bx

    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov bx, [0x7c00+0x00B] ; bytes per logical sector
    mul bx                 ; ax = bytes per cluster
    mov cx, ax

    pop bx
    pop ax
    ret


update_cluster:
    ; read cluster from disk, modify it in-memory, write it back to disk
    ; pass arguments in cluster_args_struct:
    ;    address of a byte string to modify cluster with, 2 bytes
    ;    cluster number (2-based), 2 bytes
    ;    number of bytes to write, 2 bytes
    ;    byte offset within cluster, 2bytes


    ; set number of sectors to transfer to equal cluster size (in sectors)
    push_abcd
    push si
    push di

    mov si, [cluster_args_struct]
    mov bx, [si+2]
    call read_cluster

    mov di, cluster_buffer

    mov cx, [si+4]
    add di, [si+6]
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
    cluster_rw read_disk_lba
    ret


write_cluster:
    ; bx: cluster number (2-based)
    cluster_rw write_disk_lba
    ret


set_cluster_address:
    ; locate first physical sector of a cluster and write it to DAPS
    ; bx: cluster number (2-based)
    push_registers
    
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
    pop_registers
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


read_logical_sector:
    ; loads ax-th logical sector to a memory buffer
    logical_sector_rw read_disk_lba
    ret


write_logical_sector:
    ; writes content of buffer to ax-th logical sector on disk
    logical_sector_rw write_disk_lba
    ret


get_logical_sector_size:
    ; store in ax the number of physical sectors per logical sector
    push bx
    push cx
    push dx
    push si
    push di

    ; convert logical sector to physical

    push ax
    mov ah, 48h                   ; Get Drive Parameters
    mov dl, [boot_drive]          ; Drive index
    mov si, DISK_BUFFER_ADDRESS   ; response data structure address
    int 13h
    pop ax

    jc get_logical_sector_size__fallback
    mov cx, [DISK_BUFFER_ADDRESS + 18] ; bytes per physical sector
    jmp get_logical_sector_size__finish

    get_logical_sector_size__fallback:
        mov cx, 512

    get_logical_sector_size__finish:
        xor dx, dx
        mov ax, [0x7c00+0x00B] ; bytes per logical sector
        div cx

        pop di
        pop si
        pop dx
        pop cx
        pop bx
        ret


%endif