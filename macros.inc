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

%macro locate_fat_row 0
    ; find an address of a row (pair of 12 bit entries) in FAT corresponding to a given cluster number
    ; store address in si
    ; store even/odd bit in dl
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
%endmacro

%macro push_registers 0
    push_abcd
    push si
    push di
%endmacro

%macro pop_registers 0
    pop di
    pop si
    pop_dcba
%endmacro


; note: this is not conventional pattern, but it allows to access i-th variable of the frame
; using 0-based offsets with a nice expression like [bp+2*i]
%macro allocate_frame 1
    push bp
    sub sp, %1
    mov bp, sp
%endmacro

%macro erase_frame 1
    add sp, %1
    pop bp
%endmacro

%macro free_frame_ptr 1
    ; free a memory block of the heap pointed by i-th frame variable (pointer)
    push si
    mov si, [bp + 2*%1]
    call heap_free
    pop si
%endmacro