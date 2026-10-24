bits 16
%ifndef strings
%define strings

%include "constants.inc"


lstrip:
    ; strips all white-spaces at the start of the string (si)
    ; si will contain address of the first non-space character
    push ax

    lstrip__loop:
        mov al, [si]
        or al, al
        jz lstrip__finish

        cmp al, ' '
        jne lstrip__finish
        
        inc si
        jmp lstrip__loop
    
    lstrip__finish:
        pop ax
        ret


to_upper_case:
    ; convert ascii string (with si starting address) letters to be in upper case
    ; in-place operation
    push_registers

    to_upper_case__loop:
        mov al, [si]
        or al, al
        jz to_upper_case__finish

        mov bl, 'a'
        cmp al, bl
        jge to_upper_case__ge

        jmp to_upper_case__next

        to_upper_case__ge:
            mov bl, 'z'
            cmp al, bl
            jg to_upper_case__next

        to_upper_case__lc:
            mov bl, LOWER_CASE_A - UPPER_CASE_A
            sub al, bl
            mov [si], al
        
        to_upper_case__next:
            inc si
            jmp to_upper_case__loop

    to_upper_case__finish:
        pop_registers
        ret


starts_with:
    ; sets ZF if the string b (pointed to by di) starts with string a (pointed to by si)
    push si
    push di

    mov al, [si]
    or al, al
    jnz starts_with__iter

    ; if first byte is zero -> clear ZF (doesn't start with other string)
    jmp starts_with__null

    starts_with__iter:
        lodsb

        or al, al            ; if it's the end of string, set ZF
        jz starts_with__finish

        scasb
        jne starts_with__finish ; if characters are not equal, clear ZF

        jmp starts_with__iter

    starts_with__differ:
        or al, 1

    starts_with__null:
        or al, 1 ; forces to set ZF flag to 0

    starts_with__finish:
        pop di
        pop si
        ret


copy_string:
    ; create a new buffer and copy si string to it
    ; di stores the address of a buffer
    push_abcd
    push si

    call get_string_length
    call heap_alloc        ; new buffer -> di
    call copy_nbytes       ; copy si -> di

    pop si
    pop_dcba
    ret


get_string_length:
    ; returns cx, length of the string counting null terminator
    push ax
    push si
    
    mov cx, 0

    get_string_length__loop:
        inc cx
        lodsb
        or al, al
        jz get_string_length__finish
        jmp get_string_length__loop

    get_string_length__finish:
        pop si
        pop ax
        ret


count_string_characters:
    ; similar to get_string_length, but does not include trailing 0 in the count
    call get_string_length
    dec cx
    ret


copy_nbytes:
    ; copy up to cx bytes from source string (si) to destination string (di)

    push_abcd
    push si
    push di

    copy_string__loop:
        cmp cx, 0
        jle copy_string__finish

        lodsb
        stosb

        dec cx
        jmp copy_string__loop

    copy_string__finish:
        pop di
        pop si
        pop_dcba
        ret


detect_dot:
    push si

    mov bx, 0

    detect_dot__loop:        
        lodsb

        or al, al
        jz detect_dot__finish

        cmp al, '.'
        je detect_dot__found

        inc bx
        jmp detect_dot__loop

    detect_dot__found:
        or al, 1 ; clear ZF

    detect_dot__finish:
        pop si
        ret


str_shl_1:
    ; shift all string characters 1 byte to the left
    push_abcd
    push si
    push di
    mov al, 0
    cmp al, [si]

    je str_shl_1__finish

    inc si

    str_shl_1__do_shift:
        mov al, [si]
        mov [si - 1], al

        or al, al
        jz str_shl_1__finish

        inc si
        jmp str_shl_1__do_shift

    str_shl_1__finish:
        pop di
        pop si
        pop_dcba
        ret


str_shr_n:
    ; shift all string characters (starting with address si) n bytes to the right
    ; fill left-most bx bytes with white spaces
    push_abcd
    push si
    push di

    mov al, [si]
    cmp al, 0
    je str_shr_n__finish

    push si

    str_shr_n__findend:
        lodsb
        cmp al, 0
        je str_shr_n__past_end

        jmp str_shr_n__findend

    str_shr_n__past_end:
        ; make si lie bx bytes past the string's null terminator
        dec si
        add si, bx

        ; restore initial string address si to di
        pop di

    str_shr_n__do_shift:
        cmp si, di
        jle str_shr_n__fill

        ; move value bx bytes to the left of the current position si
        sub si, bx
        mov al, [si]
        add si, bx
        mov [si], al

        dec si

        jmp str_shr_n__do_shift

    ; put bx white spaces at the start of the string

    str_shr_n__fill:
        cmp bx, 0
        je str_shr_n__finish
        mov al, ' '
        stosb

        dec bx
        jmp str_shr_n__fill

    str_shr_n__finish:
        pop di
        pop si
        pop_dcba
        ret


compare_strings:
    ; compare up to cx first characters
    push cx
    push si
    push di

    compare_strings__iter:
        or cx, cx
        jz compare_strings__finish
        lodsb
        scasb
        jne compare_strings__finish

        or al, al            ; detect the end of strings
        jz compare_strings__finish

        dec cx
        jmp compare_strings__iter

    compare_strings__finish:
        pop di
        pop si
        pop cx
        ret


get_string_prefix:
    ; get a copy of firxt cx bytes of string si + \0
    ; si is the string
    ; return di, copy
    push_abcd

    inc cx
    call heap_alloc
    dec cx
    call copy_nbytes

    pop_dcba
    ret


get_string_suffix:
    ; get tail of the string si starting with index bx
    ; return di, copy of the tail
    push_abcd
    push si

    add si, bx
    call copy_string

    pop si
    pop_dcba
    ret


create_appended_string:
    ; copy string si and append a character al to a it
    ; return di, address of a new string allocated on the heap
    push_abcd
    push si

    call get_string_length
    inc cx
    call heap_alloc
    dec cx
    call copy_nbytes

    dec cx
    add di, cx
    mov [di], al
    sub di, cx

    pop si
    pop_dcba
    ret

%endif