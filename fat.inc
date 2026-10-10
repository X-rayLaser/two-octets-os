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

%endif