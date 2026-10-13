    .text
    .globl toc_pool_ha
toc_pool_ha:
    addis 3,2,pool@toc@ha
    .globl toc_pool_lo
toc_pool_lo:
    addi 3,3,pool@toc@l
    .globl toc_pool_lo_ds
toc_pool_lo_ds:
    ld 3,pool@toc@l(3)
    .globl toc_near16
toc_near16:
    addi 3,2,near@toc
    .globl toc_near_ds
toc_near_ds:
    ld 3,near@toc(2)
    .section .toc,"aw",@progbits
    .p2align 3
near:
    .quad 0
    .section .rodata,"a",@progbits
    .p2align 3
pool:
    .double 2.25
