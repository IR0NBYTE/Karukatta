global _start
_start:
    mov rax, 0
    push rax
    push QWORD [rsp + 0]
    pop rax
    test rax, rax
    jz label0
    mov rax, 3
    push rax
    mov rax, 60
    pop rdi
    syscall
    add rsp, 0
label0:
    mov rax, 5
    push rax
    mov rax, 60
    pop rdi
    syscall
    mov rax, 60
    mov rdi, 0
    syscall
