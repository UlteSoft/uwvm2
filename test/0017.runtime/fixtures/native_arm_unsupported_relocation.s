// An unsupported loader relocation must report an error before execution.
.section .rodata,"a",%progbits
.p2align 2
.global unsupported_place
unsupported_place: .word 0
.reloc unsupported_place, R_ARM_SBREL32, external_typeinfo
