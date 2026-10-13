bits 16
%ifndef heap
%define heap

heap_init:
    push_registers
    mov si, TOTAL_HEAP_SIZE
    sub si, 4
    
    ; initial free block occupies the entire available heap memory

    mov ax, 0
    mov [heap_space], ax
    mov [heap_space + 2], si
    pop_registers
    ret


heap_alloc:
    ; allocate a block of cx bytes on the heap and fill it with zeros
    ; on success, stores address of the allocated block (past the 4-byte header) in di
    ; and sets ZF
    push_abcd
    push si
    mov si, heap_space

    ; include the overhead of 4 bytes into the size of carved out block
    add cx, 4

    heap_alloc__first_fit:
        ; check for a nullpointer
        mov ax, 0
        cmp si, ax
        je heap_alloc__not_found

        ; test if length of block in bytes is larger than required
        mov bx, [si+2]
        cmp bx, cx
        jge heap_alloc__found_block

        ; check the next block in the list
        mov si, [si]
        jmp heap_alloc__first_fit

    heap_alloc__found_block:
        ; reduce the size of the block by the size of the carved out from it block
        sub bx, cx
        mov [si+2], bx

        add si, 4  ; skip the header
        add si, bx ; point si to the start of an allocated block

        call heap_prepare_block

        sub cx, cx ; force set ZF

        jmp heap_alloc__finish

    heap_alloc__not_found:
        or ax, 1 ; clear ZF
    
    heap_alloc__finish:
        mov di, si
        pop si
        pop_dcba
        ret


heap_prepare_block:
    ; si is the address of a new block's header
    ; cx is size of the block including 4-byte header
    push ax
    ; makes sure that allocated block is the last in the list
    mov ax, 0
    mov [si], ax

    ; set the size of new block in bytes (excluding the header)
    sub cx, 4
    mov [si+2], cx

    ; erase leftover data if any
    call clear_block

    ; point si to the first data byte of a new block 
    add si, 4

    pop ax
    ret


clear_block:
    push di

    mov di, si

    clear_block__loop:
        cmp cx, 0
        je clear_block__finish

        mov al, 0
        stosb
        dec cx
        jmp clear_block__loop
    
    clear_block__finish:
        pop di
        ret


heap_free:
    ; si is the address of memory block to free (past the 4-byte header)
    push_registers

    ; get address of initial block in the list
    mov di, heap_space

    heap_free__loop:
        push di

        ; get address of the next block in the list
        mov di, [di]
        mov ax, 0
        cmp di, ax
        je heap_free__link

        pop ax ; dismiss di
        jmp heap_free__loop
    
    heap_free__link:
        pop di
        sub si, 4 ; set si to the start of the block's header

        ; attach freeing block to the end of free block list
        mov [di], si

    heap_free__finish:
        pop_registers
        ret

%endif
