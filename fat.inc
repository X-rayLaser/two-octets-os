bits 16
%ifndef fat
%define fat


allocate_cluster:
    ; find a FAT entry containing the next free unused cluster,
    ; mark the cluster as end of cluster (EOC) in the entry,
    ; store number of allocated cluster in bx
    push ax
    push cx
    push dx
    push si
    push di

    mov si, FAT_ADDRESS
    add si, 3 ; skip first 2 entries (first 3 bytes)
    mov bx, 2 ; contains cluster number

    allocate_cluster__search:
        ; check even entry
        mov ax, [si]
        mov cx, 0x0fff
        and ax, cx

        or ax, ax
        jz allocate_cluster__fix_even

        ; check odd entry
        inc bx
        inc si
        mov ax, [si]
        shr ax, 4
        or ax, ax
        jz allocate_cluster__fix_odd

        inc bx
        mov ax, 2
        add si, ax

        jmp allocate_cluster__search

    allocate_cluster__fix_even:
        ; write 0xfff to even entry
        mov ax, [si]

        ; apply mask
        mov cx, 0x0fff
        or ax, cx
        mov [si], ax
        jmp allocate_cluster__finish

    allocate_cluster__fix_odd:
        ; write 0xfff to odd entry
        mov ax, [si]
        mov cx, 0xfff0
        or ax, cx
        mov [si], ax

    allocate_cluster__finish:
        pop di
        pop si
        pop dx
        pop cx
        pop ax
        ret


chain_cluster:
    ; link a given cluster number ax to the end of specified chain starting at number bx
    ; ax is a cluster number to append
    ; bx is a cluster number of a chain to append to
    push_abcd
    push si
    push di

    push ax

    call get_last_cluster ; bx here is the last cluster number of the chain

    chain_cluster__update_fat:
        locate_fat_row
        cmp dl, 0
        jne chain_cluster__update_odd

    chain_cluster__update_even:
        mov bx, [si]
        mov cx, 0xf000
        and bx, cx     ; bx = 0x?000

        pop ax         ; ax = 0x0hhh
        add ax, bx     ; ax = 0x?hhh
        mov [si], ax
        jmp chain_cluster__finish

    chain_cluster__update_odd:
        inc si
        mov bx, [si]
        mov cx, 0x000f
        and bx, cx     ; bx = 0x000?

        pop ax
        shl ax, 4      ; ax = 0xhhh0
        add ax, bx     ; ax = 0xhhh?
        mov [si], ax

    chain_cluster__finish:
        ; todo: persist FAT changes by modifying FAT on disk as well
        pop di
        pop si
        pop_dcba
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


persist_fat_record:
    ; saves fat record bx to disk
    push_registers

    ; locate modified in-memory fat record (new)
    mov ax, bx
    push bx
    call locate_fat_record
    mov si, FAT_ADDRESS
    add si, ax

    pop bx
    mov ax, bx
    call get_fat_record_sector ; (ax:dx) = (absolute sector : byte offset within sector)

    push ax
    ; read sector and locate record within sector (old)
    call read_logical_sector
    mov di, sector_buffer
    add di, dx

    ; copy modified record to sector_buffer
    mov cx, 2
    call copy_nbytes

    ; flush sector_buffer
    pop ax
    call write_logical_sector
    pop_registers
    ret


get_fat_record_sector:
    ; given a record number ax,
    ; return ax, absolute address of logical sector containing it
    ; return dx, record's byte offset within sector
    push bx
    push cx

    mov cx, [0x7c00+0x00B] ; size of logical sector in bytes

    call locate_fat_record
    xor dx, dx
    div cx
    ; ax = logical sector relative to FAT start,
    ; dx = record's byte offset within sector

    ; ax = record's relative sector number + first FAT sector 
    mov bx, [0x7c00+0x00E] ; Reserved logical sectors
    add ax, bx

    pop cx
    pop bx
    ret


locate_fat_record:
    ; find starting byte of a FAT record by it's index ax
    ; return ax, record offset relative to the start of FAT
    push cx
    push dx

    ; ax = (ax * 3) // 2
    mov cx, 3
    mul cx
    mov cx, 2
    xor dx, dx
    div cx

    pop dx
    pop cx
    ret


load_file_debug:
    ; set number of sectors to transfer to equal cluster size (in sectors)
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov [daps+2], ax

    load_file_debug__loop:
            push_registers
        mov di, buffer
        call clear_buffer
        mov ax, bx
        call itos
        mov si, di
        call print_loop
        call carriage_return
        pop_registers

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
    jc load_file_debug__some_error

    ; increment data_offset (location of next cluster in RAM) by cluster_size * logical_sector_size
    mov ax, [0x7c00+0x00B] ; bytes per logical sector
    mov cx, [0x7c00+0x00D] ; logical sectors per cluster
    mul cx

    mov cx, [data_offset]
    add ax, cx
    mov [data_offset], ax

    call get_next_cluster_num ; bx = next cluster number

    cmp bx, 0xfff
    je load_file_debug__finish
    cmp bx, 0xff0f

    je load_file_debug__finish
    jmp load_file_debug__loop

    load_file_debug__some_error:
        mov si, error
        call print_loop

    load_file_debug__finish:
        ret


%endif