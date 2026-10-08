.text
.p2align 2
.globl call_external,call_external_again,call_addend,call_local,call_cross,local_target
call_external: call external_target
nop
call_external_again: call external_target
nop
call_addend: call external_target+16
nop
call_local: call local_target
nop
call_cross: call cross_target
nop
local_target: retl
nop
.section .text.other,"ax",@progbits
.p2align 2
.globl cross_target
cross_target: retl
nop
.data
.p2align 3
.globl address_data,relative_data
address_data: .word external_target+16
relative_data: .xword local_target-.
