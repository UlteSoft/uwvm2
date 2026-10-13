// Independent assembler oracle for the fifteen A32 stale-r11 mutations.
// This object is inspected inside the test cgroup and never executed.
.syntax unified
.arm
.text
.global uwvm_arm_cfi_frame_write_encodings
.type uwvm_arm_cfi_frame_write_encodings,%function
uwvm_arm_cfi_frame_write_encodings:
    pop {r11, lr}
    ldr r11, [sp, #4]
    mov r11, r0
    add r11, r0, #8
    moveq r11, r0
    ldr r0, [r11, #4]!
    str r0, [r11, #4]!
    mul r11, r0, r1
    umull r0, r11, r1, r2
    stm r11!, {r0, r1}
    ldm r11!, {r0, r1}
    mov r11, sp
    ldrd r10, r11, [r0]
    ldrexd r10, r11, [r0]
    smulbb r11, r0, r1
.size uwvm_arm_cfi_frame_write_encodings, .-uwvm_arm_cfi_frame_write_encodings
.section .note.GNU-stack,"",%progbits
