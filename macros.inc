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