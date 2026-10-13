bits 16
%ifndef shell
%define shell

%macro detect_command 2
    ; arg1: match string
    ; arg2: address of command to run
    mov si, %1
    mov ax, %2
    push_registers
    call starts_with
    pop_registers
    jz get_command_address__finish
%endmacro


shell_loop:
    ; show prompt
    mov si, prompt
    call print_loop

    ; get user command from keyboard
    call getline
    call run_command

    call carriage_return

    call heap_init ; reset the heap
    jmp shell_loop


run_command:
    mov si, buffer
    call split_args
    call get_command_address

    mov si, ax
    or ax, ax
    jz run_command__notfound

    call si
    jmp run_command__finish

    run_command__notfound:
        mov si, cmd_notfound
        call print_loop

    run_command__finish:
        ret


get_command_address:
    push si
    push di
    mov di, buffer
    
    detect_command cmd_fsinfo, run_fsinfo
    detect_command cmd_echo, run_echo
    detect_command cmd_cat, run_cat
    detect_command cmd_ls, run_ls
    detect_command cmd_rename, run_rename
    detect_command cmd_mkfile, run_mkfile
    detect_command cmd_mkdir, run_mkdir

    get_command_address__not_found:
        mov ax, 0

    get_command_address__finish:
        pop di
        pop si
        ret


run_fsinfo:
    call print_bpb
    ret


run_echo:
    mov si, argv_pointers+2

    run_echo__loop:
        cmp si, di
        jge run_echo__finish

        ; load next arg address
        lodsw
        push si
        mov si, ax
        call print_loop
        mov al, ' '
        call print_char
        pop si

        jmp run_echo__loop

    run_echo__finish:
        ret


run_cat:
    mov si, [argv_pointers+2]
    call fs_cat
    ret


run_ls:
    mov si, [argv_pointers+2]
    call fs_list
    ret


run_rename:
    mov si, [argv_pointers+2]
    call fs_rename
    ret


run_mkfile:
    mov si, [argv_pointers+2]
    call fs_mkfile
    ret


run_mkdir:
    mov si, [argv_pointers+2]
    call fs_mkdir
    ret


split_args:
    ; si should be the address of an argument string
    ; on success di points on the end of the array
    push_abcd
    push si

    mov di, argv_pointers

    split_args__loop:
        ; removing leading spaces
        call lstrip

        ; check end of string
        mov al, 0
        cmp [si], al
        je split_args__finish

        ; get next argument and append it's address to a argv array of pointers
        call get_next_arg
        stosw

        jmp split_args__loop
    
    split_args__finish:
        pop si
        pop_dcba
        ret


get_next_arg:
    ; extract the next argument (part of string wrapped by whitespaces) in the string 
    ; into a buffer and stores it's address in ax
    ; si points to the next character after the argument (whitespace or 0)
    
    push cx
    push di

    mov cx, 256
    call heap_alloc
    push di

    get_next_arg__loop:
        mov al, ' '
        cmp [si], al
        je get_next_arg__finish

        mov al, 0
        cmp [si], al
        je get_next_arg__finish

        lodsb
        stosb
        jmp get_next_arg__loop

    get_next_arg__finish:
        ; null terminator
        mov al, 0
        stosb

        pop ax ; restore di pushed earlier to ax

        pop di
        pop cx
        ret


free_argv:
    ; di is the address of end of argv

    mov si, argv_pointers

    free_args__loop:
        cmp si, di
        jge free_argv__finish

        lodsw
        push si
        mov si, ax
        call heap_free
        pop si

        jmp free_args__loop

    free_argv__finish:
        ret

%endif