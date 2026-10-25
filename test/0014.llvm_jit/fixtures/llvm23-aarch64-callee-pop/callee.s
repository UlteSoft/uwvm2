// @brief Model the ABI-mandated 16-byte callee pop and a caller-pop control.
// @details Neither leaf reads or writes the caller's spill slots. The ordinary
// leaf leaves SP unchanged, allowing the same caller-frame fixture to check both
// conventions independently of register allocation in another compiler.
.text
.globl callee
.type callee, %function
callee:
    add sp, sp, #16
    ret
.size callee, .-callee

.globl ordinary_callee
.type ordinary_callee, %function
ordinary_callee:
    ret
.size ordinary_callee, .-ordinary_callee
.section .note.GNU-stack,"",%progbits
