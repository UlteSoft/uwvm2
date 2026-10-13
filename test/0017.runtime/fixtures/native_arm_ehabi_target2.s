// DATA-only GNU ARM TARGET2/GOT_PREL relocation oracle.
.section .rodata,"a",%progbits
.p2align 2
.global target2_zero, target2_plus, target2_minus, target2_local, got_prel
target2_zero: .word 0
.reloc target2_zero, R_ARM_TARGET2, external_typeinfo
target2_plus: .word 4
.reloc target2_plus, R_ARM_TARGET2, external_typeinfo
target2_minus: .word -4
.reloc target2_minus, R_ARM_TARGET2, external_typeinfo
target2_local: .word 0
.reloc target2_local, R_ARM_TARGET2, local_typeinfo
got_prel: .word 0
.reloc got_prel, R_ARM_GOT_PREL, external_typeinfo
.local local_typeinfo
local_typeinfo: .word 0x12345678
