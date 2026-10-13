bits 16
%ifndef shell
%define shell

%macro detect_command 2
    ; arg1: match string
    ; arg2: address of command to run
    mov si, %1
    call starts_with
    jz %2
%endmacro


shell_loop:
    ; show prompt
    mov si, prompt
    call print_loop

    ; get user command from keyboard
    call getline
    call run_command

    call carriage_return

    jmp shell_loop


run_command:
    mov di, buffer

    detect_command cmd_fsinfo, run_command__fsinfo
    detect_command cmd_echo, run_command__echo
    detect_command cmd_cat, run_command__cat
    detect_command cmd_ls, run_command__ls
    detect_command cmd_rename, run_command__rename
    detect_command cmd_mkfile, run_command__mkfile
    detect_command cmd_mkdir, run_command__mkdir

    jnz run_command__notfound

    run_command__fsinfo:
        call print_bpb
        jmp run_command__finish

    run_command__echo:
        mov si, buffer
        call split_args
        mov si, [argv_pointers+0]
        call print_loop
        mov si, [argv_pointers+2]
        call print_loop
        mov si, [argv_pointers+4]
        call print_loop

        jmp run_command__finish

    run_command__cat:
        call run_cat
        jmp run_command__finish

    run_command__ls:
        call run_ls
        jmp run_command__finish

    run_command__rename:
        call run_rename
        jmp run_command__finish

    run_command__mkfile:
        call run_mkfile
        jmp run_command__finish

    run_command__mkdir:
        call run_mkdir
        jmp run_command__finish

    run_command__notfound:
        mov si, cmd_notfound
        call print_loop

    run_command__finish:
        ret


run_cat:
    mov si, buffer
    add si, 4
    call lstrip
    call fs_cat
    ret


run_ls:
    mov si, buffer
    add si, 3
    call lstrip
    call fs_list
    ret


run_rename:
    mov si, buffer
    add si, 7
    call lstrip
    call fs_rename
    ret


run_mkfile:
    mov si, buffer
    add si, 7
    call lstrip
    call fs_mkfile
    ret


run_mkdir:
    mov si, buffer
    add si, 6
    call lstrip
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
        pop ax ; restore di pushed earlier to ax

        pop di
        pop cx
        ret

%endif