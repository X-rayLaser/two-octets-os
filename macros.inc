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