bits 16
%ifndef file_system
%define file_system

%include "constants.inc"

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
    call carriage_return
%endmacro


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


create_file:
    ; path buffer contains address of a path of file to be created
    mov si, path_buffer
    mov di, file_name_buffer
    call split_and_copy
    ; here path_buffer contains path to the file's parent directory

    ; get file's parent directory length and store it in dx
    call load_subdir_entry
    jnz create_file__error
    mov dx, [si+28] ; note: read first 2 bytes of 4-byte file length field
    push dx

    call create_directory_entry

    ; todo: handle corner case of creating files in the root directory, e.g. /filename
    ; store the cluster number of the file's parent directory in bx
    mov si, path_buffer
    call load_directory
    jnz create_file__error

    ; treat directory like file and append to it 32-bytes entry corresponding to a new file
    ; and persist it to disk
    mov si, new_dir_entry_buffer
    mov cx, 32
    pop dx
    ; bx should contain the cluster no
    call append_file

    create_file__error:


    create_file__finish:
        ret


load_subdir_entry:
    ; load parent directory of a given subdirectory and find directory entry describing it
    ; si: address of a subdirectory path
    ; on success, si will contain address of target entry
    push_abcd
    push di

    mov di, parent_path_buffer
    mov cx, 256
    call copy_nbytes

    mov si, parent_path_buffer
    mov di, subdir_name_buffer
    call get_parent

    mov si, parent_path_buffer
    call load_directory

    mov di, subdir_name_buffer
    call search_dir_entry

    pop di
    pop_dcba
    ret


append_file:
    ; add a few bytes to the end of a given a file
    ; expects si to store address of a buffer
    ; expects bx to store a starting cluster of the file
    ; expects cx to store number of bytes to write
    ; expects dx to store file size

    ; find last cluster of the file

    ; load last cluster

    push bx

    ; compute total cluster size in bytes 
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov bx, [0x7c00+0x00B] ; bytes per logical sector
    mul bx                 ; ax = bytes per cluster
    
    ; ax = file size / cluster size in bytes
    mov bx, ax
    mov ax, dx
    xor dx, dx
    div bx

    pop bx
    ; todo: modulo arithmetics for remainders > 255
    cmp dl, cl

    jl append_file__data_not_fits

    append_file__data_fits:
        ; append data to file buffer
        ; write file buffer
        jmp append_file__finish

    append_file__data_not_fits:
        ; append a few bytes to complete cluster
        ; write file buffer to disk
        ; allocate new cluster and add it to the file
        ; update file buffer to only contain remaining bytes
        ; write file buffer to disk

    append_file__finish:
        ret


create_directory_entry:
    push_abcd
    push si
    push di

    mov si, file_name_buffer
    call fix_file_name

    call allocate_cluster ; new cluster number -> bx

    ; copy 11 bytes of file name into the buffer
    mov di, new_dir_entry_buffer
    call clear_buffer
    mov cx, 11
    call copy_nbytes
    mov [si+26], bx

    pop di
    pop si
    pop_dcba
    ret


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
    mov bx, 0 ; contains cluster number

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

    chain_cluster__traverse_chain:
        push bx
        call get_next_cluster_num ; bx = next cluster number
        cmp bx, 0xfff
        je chain_cluster__update_fat
        cmp bx, 0xff0f
        je chain_cluster__update_fat

        pop bx
        jmp chain_cluster__traverse_chain

    chain_cluster__update_fat:
        pop bx ; bx here is the last cluster number of the chain

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


clear_cluster:
    push_abcd
    push si
    push di
    ; fill the cluster bx with zeros

    ; set number of sectors to transfer to equal cluster size (in sectors)
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mov [daps+2], ax

    ; ax = (bx - 2) * cluster size
    dec bx
    dec bx
    xor ax, ax
    mov al, [0x7c00+0x00D] ; number of logical sectors per cluster
    mul bx                 ; ax = offset of cluster's first sector

    push ax

    ; ax = first_rootdir_sector + sectors_per_rootdir = first cluster sector number
    call first_rootdir_sector ; -> ax
    mov cx, ax
    call sectors_per_rootdir ; -> ax
    add ax, cx

    pop cx ; restore ax (cluster's offset) to cx
    add ax, cx

    call set_lba_address

    mov ax, null_buffer
    call set_transfer_buffer

    ; issuing BIOS interrupt for writing disk sectors using LBA addressing
    mov ah, 0x43
    mov dl, [boot_drive] ; set drive number to boot drive
    mov si, daps         ; pass an address of data structure before BIOS call
    int 0x13

    pop di
    pop si
    pop_dcba
    ret


search_dir_entry:
    ; on success, sets ZF and sets si to the address of the target entry containing filename di
    push_abcd
    push di

    mov si, [directory_buffer_ptr]

    search_dir_entry__loop:
        mov al, [si]
        or al, al
        jz search_dir_entry__notfound

        call check_directory_entry
        jz search_dir_entry__finish; check if entry found

        add si, 32 ; go to the next entry
        jmp search_dir_entry__loop

    search_dir_entry__notfound:
        or al, 1 ; clear ZF

    search_dir_entry__finish:
        pop di
        pop_dcba
        ret


find_file:
    ; attempts to find a parent directory of the file and locate it's entry
    ; si is the address of the byte string containing complete file path
    ; sets ZF and sets si to the address of the file's entry on success
    push_abcd
    push di

    call split_path

    add si, bx ; start of the file name
    mov di, file_name_buffer
    mov cx, 32
    call copy_nbytes

    ; putting null terminating character (0) at the start of file name portion
    mov al, 0
    mov [si], al

    mov si, path_buffer
    call load_directory ; atempt to load parent directory of whatever is under it

    jnz find_file__finish ; if that fails, the path must be wrong

    ; if it succeeds, find the file entry and show file's data

    mov si, file_name_buffer
    call fix_file_name
    mov di, file_name_buffer
    call search_dir_entry

    find_file__finish:
        pop di
        pop_dcba
        ret


load_directory:
    ; copies directory data into directory_buffer, sets ZF and saves first directory cluster in bx on success
    ; si points to the directory path with leading /

    mov di, ROOTDIR_ADDRESS
    mov [directory_buffer_ptr], di

    call ensure_trailing_slash

    ; special case: illegal path without leading /
    mov al, [si]
    cmp al, '/'
    jne load_directory__notfound

    ; special case: directory path is just /
    mov al, [si+1]
    or al, al
    jz load_directory__finish

    inc si ; skip leading '/'

    load_directory__outer_loop:
        ; end of path: 0 after trailing /
        mov al, [si]
        or al, al
        jz load_directory__finish

        mov di, buffer
        call clear_buffer
    
        call parse_path_segment ; next subdirectory name -> buffer
        
        push si
        mov si, buffer
        call fix_file_name
        pop si
        mov di, buffer ; target sub directory name to load

        ; sanity check: buffer must not start with 0
        mov al, [di]
        or al, al
        jz load_directory__notfound

        call load_next_dir
        jnz load_directory__notfound

        jmp load_directory__outer_loop

    load_directory__notfound:
        or al, 1 ; clear ZF
        jmp load_directory__finish

    load_directory__finish:
        ret


load_next_dir:
    ; given a current directory pointed by directory_buffer_ptr, find and load a target subdirectory pointed by di
    ; save the first cluster of subdirectory in bx and set ZF on success
    push ax
    push cx
    push dx
    push si
    push di

    mov si, [directory_buffer_ptr]

    load_next_dir__loop:
        mov al, [si]
        or al, al
        jz load_next_dir__notfound

        call check_directory_entry
        jz load_next_dir__found; check if entry found

        load_next_dir__next_entry:
            add si, 32 ; go to the next entry
            jmp load_next_dir__loop
    
    load_next_dir__notfound:
        or al, 1 ; clear ZF
        jmp load_next_dir__finish

    load_next_dir__found:
        ; check wheither it's a directory rather than file
        mov al, [si+11]
        cmp al, 0x10
        jne load_next_dir__next_entry ; if it's not directory, go back to loop

        mov bx, [si+26] ; get first sector of subdirectory
        push bx

        ; load directory to be at directory_buffer address
        mov di, directory_buffer
        mov [data_offset], di
        mov [directory_buffer_ptr], di
        call load_file

        pop bx
        xor al, al ; force setting ZF

    load_next_dir__finish:
        pop di
        pop si
        pop dx
        pop cx
        pop ax
        ret


check_directory_entry:
    ; sets ZF if current directory entry pointed by si contains subdirectory pointed by di
    push_abcd
    push si

    mov cx, 11
    call compare_strings

    pop si
    pop_dcba
    ret


print_root_directory:
    mov si, ROOTDIR_ADDRESS
    call print_directory
    ret


print_directory:
    jc print_directory__error
    push si

    print_directory__print_entry:
        mov al, [si]
        cmp al, 0
        je print_directory__finish

        call print_directory_entry
        add si, 32  ; go to the next entry

        jmp print_directory__print_entry


    jmp print_directory__finish

    print_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_directory__finish:
        pop si
        ret


print_directory_entry:
    push_abcd
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
        pop_dcba
        ret

%endif