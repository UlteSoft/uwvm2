// Freestanding MIPS N64 R6/MSA probe: no libc, runtime, or I/O.
// Genuine generated NaN2008 ELF; no retagging a legacy-NaN executable.
.set noreorder
.set nomips16
.set msa
.text
.globl __start
.ent __start
__start:
    li $4, 5
    dla $5, action
    move $6, $0
    li $7, 16
    li $2, 5013
    syscall
    bne $7, $0, failed
    nop
    dla $4, lanes
    ld.b $w0, 0($4)
    copy_s.d $8, $w0[0]
    copy_s.d $9, $w0[1]
    dli $10, 0x0706050403020100
    bne $8, $10, failed
    nop
    dli $10, 0x0f0e0d0c0b0a0908
    bne $9, $10, failed
    nop
    break 0
failed:
    li $4, 99
exit_probe:
    li $2, 5058
    syscall
.end __start
.ent trap_handler
trap_handler:
    // N64 used_math: uc_mcontext40 + offset588. QEMU actually writes
    // USED_FP only, even though the two MSA halves were proved before trap.
    // This observes the missing provider data; C++ ABI fixtures separately
    // verify that this flag cannot grant the product's W-register slots.
    li $10, 5
    bne $4, $10, failed
    nop
    lw $8, 628($6)
    li $10, 1
    bne $8, $10, failed
    nop
    ld $8, 296($6)
    dli $10, 0x0706050403020100
    bne $8, $10, failed
    nop
    li $4, 0
    li $2, 5058
    syscall
.end trap_handler
.data
.balign 8
action:
    .word 8
    .word 0
    .dword trap_handler
    .dword 0, 0
.balign 16
lanes:
    .byte 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
.section .note.GNU-stack,"",@progbits
