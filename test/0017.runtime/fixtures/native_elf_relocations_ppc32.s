.text
.p2align 2
.globl call_external,call_external_again,call_addend,call_local,call_cross,local_target
call_external: bl external_target
call_external_again: bl external_target
call_addend: bl external_target+16
call_local: bl local_target
call_cross: bl cross_target
local_target: blr
.section .text.other,"ax",@progbits
.p2align 2
.globl cross_target
cross_target: blr
.data
.p2align 2
.globl address_data,relative_data
address_data: .long external_target+16
relative_data: .long local_target-.
