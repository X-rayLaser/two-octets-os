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
        add si, 4 ; print everything in the buffer past "echo"
        call lstrip
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

%endif