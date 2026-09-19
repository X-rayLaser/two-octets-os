[org 0x7e00]
bits 16

%macro print_bpb_field 2
    push si
    mov si, %1
    call print_loop
    pop si

    xor ax, ax
    %2
    
    push si

    push ax
    mov di, buffer
    call clear_buffer

    pop ax
    call itos
    mov si, buffer
    call print_loop

    pop si
    call cariage_return
%endmacro

mov [boot_drive], dl
call init_daps

call print_bpb
call print_root_directory

jmp hang


print_bpb:
    mov si, 0x7c00+0x00B

    print_bpb_field lbl_bytes_per_sector, lodsw
    print_bpb_field lbl_sectors_per_cluster, lodsb
    print_bpb_field lbl_reserved_sectors, lodsw
    print_bpb_field lbl_fats, lodsb
    print_bpb_field lbl_root_entries, lodsw
    print_bpb_field lbl_total_sectors, lodsw
    print_bpb_field lbl_media, lodsb
    print_bpb_field lbl_fat_size, lodsw
    
    ret


print_root_directory:
    mov si, ROOTDIR_ADDRESS

    jc print_root_directory__error
    push si
    mov si, root_directory
    call print_loop
    pop si

    print_root_directory__print_entry:
        mov al, [si]
        cmp al, 0
        je print_root_directory__finish

        call print_directory_entry
        add si, 32  ; go to the next entry

        jmp print_root_directory__print_entry


    jmp print_root_directory__finish

    print_root_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_root_directory__finish:
        ret


print_directory_entry:
    push ax
    push bx
    push cx
    push dx
    push si
    
    mov di, buffer
    call clear_buffer

    mov cx, 11

    print_directory_entry__getchar:
        cmp cx, 3
        jle print_directory_entry__gotname

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__getchar

    print_directory_entry__gotname:
        mov al, '.'
        mov [di], al
        inc di

    print_directory_entry__get_extension:
        cmp cx, 0
        jle print_directory_entry__finish

        lodsb
        mov [di], al
        inc di
        dec cx
        jmp print_directory_entry__get_extension

    print_directory_entry__finish:
        mov al, 0x0D
        mov [di], al
        inc di
        mov al, 0x0A
        mov [di], al
        
        mov si, buffer
        call print_loop

        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret


itos:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov bx, 0


    test ax, ax
    js itos__signed
    jnz itos__getdigit

    mov dl, '0'
    mov [di], dl
    jmp itos__end

    itos__signed:
        neg ax
        mov dl, '-'
        mov [di], dl
        inc di

    itos__getdigit:
        cmp ax, 0
        je itos__flush
        xor dx, dx
        mov cx, 10
        div cx

        add dl, 48
        mov dh, 0
        push dx
        inc bx
        jmp itos__getdigit

    itos__flush:
        cmp bx, 0
        je itos__end

        pop ax
        stosb
        dec bx
        jmp itos__flush

    itos__end:
        pop di
        pop si
        pop dx
        pop cx
        pop bx
        pop ax
        ret



clear_buffer:
    push ax
    push bx
    push di

    mov cx, 30

    clear_buffer__loop:
        cmp cx, 0
        jle clear_buffer__finish

        mov al, 0
        mov [di], al
        inc di
        dec cx
        jmp clear_buffer__loop

    clear_buffer__finish:
        pop di
        pop bx
        pop ax
        ret


cariage_return:
    mov al, 0x0D
    call print_char
    mov al, 0x0A
    call print_char
    ret


print_char:
    mov ah, 0x0E
    int 0x10
    ret


%include "util.inc"


hang:
    jmp hang

buffer times 30 db 0
msg_hello db "Hello from kernel, indeed!", 0x0D, 0x0A, 0
msg_disk_error db "Disk accessing error!", 0x0D, 0x0A, 0
root_directory db "Root directory:", 0x0D, 0x0A, 0

; DOS 2.0 BPB Labels (FAT-12)
lbl_bytes_per_sector:         db "Bytes per logical sector:    ", 0
lbl_sectors_per_cluster:      db "Logical sectors per cluster: ", 0
lbl_reserved_sectors:         db "Reserved logical sectors:    ", 0
lbl_fats:                     db "Number of FATs:              ", 0
lbl_root_entries:             db "Root directory entries:      ", 0
lbl_total_sectors:            db "Total logical sectors:       ", 0
lbl_media:                    db "Media descriptor (Hex):      ", 0
lbl_fat_size:                 db "Logical sectors per FAT:     ", 0

data_offset: dw 0

