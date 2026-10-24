bits 16
%ifndef dirs
%define dirs

%include "constants.inc"

%macro create_file_prolog 0
    push_registers
    allocate_frame 4   ; [parent_path, new_entry]

    call split_path
    ; here si contains path to the file's parent directory
    ; di contains file name
    mov [bp + 0], si ; preserve parent path

    mov si, di
    call create_directory_entry
    mov [bp + 2], di ; preserve new entry

    call heap_free  ; free memory taken by a file name
%endmacro

%macro create_file_epilog 1
    ; param 1: error handling label
    
    ; store the cluster number of the file's parent directory in bx
    mov si, [bp + 0]
    call load_directory
    jnz %1

    ; treat directory like file and append to it 32-bytes entry corresponding to a new file
    ; and persist it to disk
    mov di, [bp + 2]
    mov cx, DIRECTORY_ENTRY_SIZE
    mov [file_append_struct], di
    mov [file_append_struct+2], bx
    mov [file_append_struct+4], cx

    ; get file's parent directory length and store it in cx
    call get_directory_size
    jnz %1

    mov [file_append_struct+6], cx

    call append_file
%endmacro


%macro create_root_file_epilog 0
    ; compute ax, root record's logical sector on disk
    call first_rootdir_sector  ; -> ax

    push ax
    call get_root_entry_sector ; -> ax
    pop bx
    add ax, bx

    ; read sector, update it in memory, flush sector to disk
    call read_logical_sector

    mov di, sector_buffer
    add di, dx
    mov cx, DIRECTORY_ENTRY_SIZE
    mov si, [bp + 2]
    call copy_nbytes
    call write_logical_sector

    ; refresh root directory structure in memory
    call read_root_directory

    ; cleanup
    free_frame_ptr 0
    free_frame_ptr 1
    erase_frame 4
    sub cx, cx    ; set ZF
    pop_registers
%endmacro


create_file_under_root:
    ; si contains address of a path of file to be created
    create_file_prolog
    create_root_file_epilog
    ret


create_dir_under_root:
    ; si contains address of a path of directory to be created
    create_file_prolog

    mov di, [bp + 2]
    mov al, 0x10
    mov [di+11], al

    create_root_file_epilog
    ret



get_root_entry_sector:
    ; compute relative location of of next entry to be inserted in the root directory
    ; returns ax, record's sector offset relative to the root directory start
    ; returns dx, record's byte offset relative to the start of the sector
    mov si, root_path
    call get_directory_size ; cx = bytes occupied by existing records

    mov ax, cx
    mov cx, [BPB_BYTES_PER_LOGICAL_SECTOR]  ; bytes per logical sector
    xor dx, dx
    div cx
    ret


create_file:
    ; si contains address of a path of file to be created
    create_file_prolog
    create_file_epilog create_file__finally

    create_file__finally:
        call handle_finally

    create_file__finish:
        free_frame_ptr 0
        free_frame_ptr 1
        erase_frame 4
        or ax, ax
        pop_registers
        ret


create_directory:
    ; path buffer contains address of a path of directory to be created
    create_file_prolog
    
    mov di, [bp + 2]
    mov al, 0x10
    mov [di+11], al

    create_file_epilog create_directory__finally

    create_directory__finally:
        call handle_finally

    create_directory__finish:
        free_frame_ptr 0
        free_frame_ptr 1
        erase_frame 4
        or ax, ax
        pop_registers
        ret


handle_finally:
    jz handle_finally__ok

    handle_finally__error:
        mov ax, 1
        jmp handle_finally__finish

    handle_finally__ok:
        mov ax, 0

    handle_finally__finish:
        ret


get_directory_size:
    ; si is the address of the directory path
    ; cx will store the size of the directory in bytes

    push ax
    push bx
    push dx
    push si
    push di

    call load_directory
    jnz get_directory_size__error

    mov si, [directory_buffer_ptr]

    mov cx, 0
    get_directory_size__loop:
        mov al, [si]
        or al, al
        jz get_directory_size__finish

        add cx, DIRECTORY_ENTRY_SIZE
        add si, DIRECTORY_ENTRY_SIZE
        jmp get_directory_size__loop
    
    get_directory_size__error:


    get_directory_size__finish:
        pop di
        pop si
        pop dx
        pop bx
        pop ax
        ret


load_subdir_entry:
    ; load parent directory of a given subdirectory and find directory entry describing it
    ; si: address of a subdirectory path
    ; on success, si will contain address of target entry
    push_abcd
    push di

    ; split path into parent (si) / child (di)
    call split_path

    ; fix child
    push si
    mov si, di
    call remove_trailing_slash
    call fix_file_name
    pop si

    push si
    push di
    call load_directory
    pop di
    pop si
    jnz load_subdir_entry__error

    call search_dir_entry
    jnz load_subdir_entry__error

    ; todo: free allocated memory

    ; ZF already set
    jmp load_subdir_entry__finish

    load_subdir_entry__error:
        ; ZF is already cleared

    load_subdir_entry__finish:
        pop di
        pop_dcba
        ret


update_root_entry:
    ; scan root directory to find target entry and replace it with a given buffer
    ; si: buffer address
    ; di: target file name address

    push_registers

    push si
    push di

    call sectors_per_rootdir
    mov cx, ax

    ; ax = first logical root sector
    push cx
    call first_rootdir_sector
    pop cx

    pop di

    update_root_entry__loop:
        ; check if we moved past the last root directory sector
        cmp cx, 0
        jle update_root_entry__notfound

        ; todo: clear sector_buffer
        ; load ax-th logical sector
        call read_logical_sector

        mov si, sector_buffer
        mov [directory_buffer_ptr], si

        call search_dir_entry
        jz update_root_entry__do_update

        inc ax ; check next sector in the next iteration
        dec cx
        jmp update_root_entry__loop

    update_root_entry__do_update:
        ; replace old 32-byte entry with new entry in the buffer
        mov di, si ; di = start of old entry in sector_buffer
        pop si     ; si = buffer with new entry data
        mov cx, DIRECTORY_ENTRY_SIZE
        call copy_nbytes

        ; write in-memory updated sector (in sector_buffer) to ax-th logical disk sector
        call write_logical_sector

        ; reload the root directory
        call read_root_directory

        ; force set ZF
        sub cx, cx

        jmp update_root_entry__finish

    update_root_entry__notfound:
        or cx, 1 ; clear ZF

    update_root_entry__finish:
        pop_registers
        ret


update_non_root_entry:
    ; handle editing non root directory entry
    ; si, address of a full path of file whose entry needs updating
    ; di, address of a new entry for the file
    ; sets ZF on success
    push_registers

    ; provide address of a new entry
    mov [update_entry_structure], di

    call split_path

    ; set cluster number of the directory whose entry needs update
    call load_directory ; bx : = first cluster number of a directory
    jnz update_non_root_entry__error
    mov [update_entry_structure+2], bx

    ; provide old file name to find the entry
    push si
    mov si, di
    call remove_trailing_slash
    call fix_file_name
    mov [update_entry_structure+4], si
    pop si
    
    ; update the entry and write changes to disk
    call update_directory_entry
    jnz update_non_root_entry__error

    mov ax, 0
    jmp update_non_root_entry__cleanup

    update_non_root_entry__error:
        mov ax, 1

    update_non_root_entry__cleanup:
        ; free parent path
        call heap_free

        ; free file name
        mov si, di
        call heap_free
    
    update_non_root_entry__finish:
        or ax, ax ; set ZF as needed
        pop_registers
        ret


read_root_directory:
    call first_rootdir_sector ; ax = starting sector of root directory
    call set_lba_address

    call sectors_per_rootdir ; ax = # sectors per root directory
    mov [daps+2], ax

    mov ax, ROOTDIR_ADDRESS
    call set_transfer_buffer
    call read_disk_lba

    mov si, ROOTDIR_ADDRESS

    ret


update_directory_entry:
    ; scan directory to find target entry and replace it with a given buffer
    ; update_entry_structure:
    ;   buffer address, 2 bytes
    ;   directory start cluster, 2 bytes
    ;   target file name address, 2 bytes
    push_registers

    mov bx, [update_entry_structure+2]
    mov di, [update_entry_structure+4]
    call locate_directory_entry
    jnz update_directory_entry__not_found

    update_directory_entry__do_update:
        ; replace old 32-byte entry with new entry in the buffer
        mov di, si                       ; di = start of old entry in cluster_buffer
        mov si, [update_entry_structure] ; si = buffer address of a new entry
        mov cx, DIRECTORY_ENTRY_SIZE
        call copy_nbytes
        call write_cluster
        sub cx, cx ; force set ZF
        jmp update_directory_entry__finish

    update_directory_entry__not_found:
        mov ax, 1
        or ax, bx ; clear ZF

    update_directory_entry__finish:
        pop_registers
        ret


fix_directory_length:
    ; for a given directory path si, 
    ; updates it's corresponding entry in a parent directory by increasing the length field

    ; load entry's cluster, finds entry's address and cluster number
    ; edit it's length field
    ; write back fixed cluster content to disk
    push_registers

    ; split path into parent_path (si) / child (di)
    call split_path

    push si
    mov si, di
    call remove_trailing_slash
    call fix_file_name
    pop si

    ; store starting cluster into bx
    call load_directory

    call locate_directory_entry
    jnz fix_directory_length__finish

    ; todo: free allocated memory

    ; update length in-memory (within cluster_buffer)
    mov ax, [si+28]
    mov cx, DIRECTORY_ENTRY_SIZE
    add ax, cx
    mov [si+28], ax

    ; update disk cluster
    call write_cluster

    sub cx, cx ; set ZF (successs flag)

    fix_directory_length__finish:
        pop_registers
        ret


locate_directory_entry:
    ; similar to search_dir_entry, but searches among all directory clusters
    ; parameters:
    ;   bx: starting cluster number
    ;   di: target file name address
    ; on success, cluster_buffer contains the target cluster,
    ; also stores found entry's cluster in bx, target entry address in si
    ; sets ZF

    push ax
    push cx
    push dx
    push di
    
    locate_directory_entry__loop:
        push bx

        call read_cluster
        mov si, cluster_buffer
        mov [directory_buffer_ptr], si
        ; todo: search may freeze due to going outside the cluster buffer (and missing 0 byte)
        call search_dir_entry
        jz locate_directory_entry__found

        call get_next_cluster_num

        cmp bx, 0xfff
        je locate_directory_entry__notfound
        cmp bx, 0xff0f
        je locate_directory_entry__notfound

        pop cx ; dismiss bx
        jmp locate_directory_entry__loop

    locate_directory_entry__found:
        pop bx
        sub cx, cx
        jmp locate_directory_entry__finish

    locate_directory_entry__notfound:
        or cx, 1 ; clear ZF

    locate_directory_entry__finish:
        pop di
        pop dx
        pop cx
        pop ax
        ret


append_file:
    ; add a few bytes to the end of a given a file

    push_registers

    call configure_initial_file_update_structure

    append_file__loop:
        mov dx, [cluster_args_struct+6]

        call get_cluster_remaining_space  ; ax now stores remaining unused space in a cluster

        ; todo: verify modulo arithmetics for remainders > 255
        cmp ax, [file_append_struct+4]

        jl append_file__data_not_fits


        append_file__data_fits:
            call update_cluster
            jnz append_file__update_error
            jmp append_file__finish

        append_file__data_not_fits:
            call complete_cluster_and_advance
            call create_and_goto_cluster  
            jmp append_file__loop

    append_file__update_error:
        ;

    append_file__finish:
        pop_registers
        ret


configure_initial_file_update_structure:
    push_registers

    ; copy one structure content to another
    mov si, file_append_struct
    mov di, cluster_args_struct
    mov cx, 8
    call copy_nbytes

    ; find last cluster of the file
    mov bx, [file_append_struct+2]
    call get_last_cluster
    mov [cluster_args_struct+2], bx

    ; find byte offset of the first affected cluster
    call get_cluster_size

    ; dx = file size % cluster size
    mov ax, [file_append_struct+6]
    xor dx, dx
    div cx
    mov [cluster_args_struct+6], dx

    pop_registers
    ret


complete_cluster_and_advance:
    ; append a few bytes to complete the last cluster
    ; configure next write operation

    push_registers
    call get_cluster_remaining_space
    mov [cluster_args_struct+4], ax
    call update_cluster

    ; advance a buffer address
    mov si, [cluster_args_struct]
    add si, ax ; skip bytes written to previous cluster
    mov [cluster_args_struct], si

    ; decrease number of remaining bytes to write
    mov cx, [file_append_struct+4]
    sub cx, ax
    mov [file_append_struct+4], cx

    ; set write position right at the beginning of next cluster
    mov dx, 0
    mov [cluster_args_struct+6], dx

    pop_registers
    ret


create_and_goto_cluster:
    ; cx is number of bytes to write
    push_registers

    ; prepare new cluster
    mov bx, [cluster_args_struct+2]
    call allocate_cluster ; new cluster num -> bx
    call clear_cluster

    ; link it to the file's chain
    mov ax, bx
    mov bx, [cluster_args_struct+2]                ; old cluster num
    call chain_cluster

    ; update structure with new cluster number
    mov [cluster_args_struct+2], ax

    pop_registers
    ret


create_directory_entry:
    ; si: new file name address
    ; returns new directory entry in di
    push_abcd
    push si

    call remove_trailing_slash
    call fix_file_name

    call allocate_cluster ; new cluster number -> bx
    call sync_all_fats

    ; copy 11 bytes of file name into the buffer
    mov cx, DIRECTORY_ENTRY_SIZE
    call heap_alloc ; -> di
    mov cx, 11
    call copy_nbytes
    mov [di+26], bx

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

        add si, DIRECTORY_ENTRY_SIZE ; go to the next entry
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
    allocate_frame 6

    call split_path
    ; si -> parent path, di -> file name
    mov [bp + 0], si
    mov [bp + 2], di

    call load_directory ; atempt to load parent directory of whatever is under it
    jnz find_file_new__error ; if that fails, the path must be wrong

    ; if it succeeds, find the file entry and show file's data
    mov si, di
    call remove_trailing_slash
    call fix_file_name

    call search_dir_entry
    jnz find_file_new__error
    mov [bp + 4], si

    mov ax, 0     ; success flag
    jmp find_file_new__cleanup

    find_file_new__error:
        mov ax, 1 ; error flag

    find_file_new__cleanup:
        ; cleanup heap and stack
        free_frame_ptr 0
        free_frame_ptr 1
        mov si, [bp + 4]
        erase_frame 6

    ; set ZF on success, clear it on error
    or ax, ax

    find_file_new__finish:
        pop di
        pop_dcba
        ret


load_directory:
    ; copies directory data into directory_buffer, sets ZF and saves first directory cluster in bx on success
    ; si points to the directory path with leading /

    push ax
    push cx
    push dx
    push si
    push di

    allocate_frame 4 ; [path copy with trailing /, segment buffer]
    
    mov di, ROOTDIR_ADDRESS
    mov [directory_buffer_ptr], di

    call ensure_trailing_slash
    mov si, di
    mov [bp + 0], di

    ; allocate a buffer for segments
    mov cx, MAX_FILENAME_LENGTH
    call heap_alloc
    mov [bp + 2], di

    ; special case: illegal path without leading /
    mov al, [si]
    cmp al, '/'
    jne load_directory__notfound

    ; special case: directory path is just /
    mov al, [si+1]
    or al, al
    jz load_directory__found

    inc si ; skip leading '/'

    load_directory__outer_loop:
        ; end of path: 0 after trailing /
        mov al, [si]
        or al, al
        jz load_directory__found

        mov di, [bp + 2]
        call parse_path_segment ; next subdirectory name -> buffer
        
        push si
        mov si, di
        call fix_file_name
        pop si

        ; sanity check: buffer must not start with 0
        mov al, [di]
        or al, al
        jz load_directory__notfound

        call load_next_dir
        jnz load_directory__notfound
        jmp load_directory__outer_loop

    load_directory__notfound:
        mov ax, 1
        jmp load_directory__finish

    load_directory__found:
        mov ax, 0

    load_directory__finish:
        free_frame_ptr 0
        free_frame_ptr 1
        erase_frame 4
        or ax, ax ; set ZF as needed
        pop di
        pop si
        pop dx
        pop cx
        pop ax
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
            add si, DIRECTORY_ENTRY_SIZE ; go to the next entry
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

    push_registers
    jc print_directory__error

    print_directory__print_entry:
        mov al, [si]
        cmp al, 0
        je print_directory__finish

        call print_directory_entry
        add si, DIRECTORY_ENTRY_SIZE  ; go to the next entry

        jmp print_directory__print_entry


    jmp print_directory__finish

    print_directory__error:
        ; hadle error
        mov si, msg_disk_error
        call print_loop

    print_directory__finish:
        pop_registers
        ret


print_directory_entry:
    push_registers
    
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

        pop_registers
        ret

%endif