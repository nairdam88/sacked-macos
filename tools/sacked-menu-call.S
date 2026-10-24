    .text
    .globl _call_set_state
    .def _call_set_state; .scl 2; .type 32; .endef
_call_set_state:
    pushl %ebp
    movl %esp, %ebp
    subl $12, %esp
    movl %ebx, -4(%ebp)
    movl %esi, -8(%ebp)
    movl %edi, -12(%ebp)
    movl 8(%ebp), %ecx
    pushl 16(%ebp)
    call *12(%ebp)
    movl -4(%ebp), %ebx
    movl -8(%ebp), %esi
    movl -12(%ebp), %edi
    leave
    ret
