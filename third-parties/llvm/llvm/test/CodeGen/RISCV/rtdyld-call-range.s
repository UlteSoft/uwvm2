# RUN: llvm-mc -triple=riscv64-linux-gnu -filetype=obj %s -o %T/riscv-call-range.o
# RUN: llvm-rtdyld -verify -triple=riscv64-linux-gnu -map-section=riscv-call-range.o,.text=0x4000000000 -map-section=riscv-call-range.o,.text.other=0x6000000000 -dummy-extern=external_target=0x1122334455667780 -check=%S/rtdyld-call-range.check %T/riscv-call-range.o
# The standalone Inputs/rtdyld-call-range.cpp additionally checks both rounded
# range boundaries, near-call bypass, addends, tail calls, no-stub failure, RV32
# XLEN wrapping, and two distinct target section mappings without executing code.

.option norvc
.option norelax
.text
.globl call_target
call_target:
.reloc ., R_RISCV_CALL, external_target
  auipc ra, 0
  jalr ra, 0(ra)
  ret
.globl tail_target
tail_target:
.reloc ., R_RISCV_CALL_PLT, external_target
  auipc t1, 0
  jalr zero, 0(t1)
  ret
.globl call_addend
call_addend:
.reloc ., R_RISCV_CALL_PLT, external_target+16
  auipc ra, 0
  jalr ra, 0(ra)
  ret
.globl call_local
call_local:
.reloc ., R_RISCV_CALL, local_target
  auipc ra, 0
  jalr ra, 0(ra)
  ret
.globl call_cross
call_cross:
.reloc ., R_RISCV_CALL_PLT, cross_target
  auipc ra, 0
  jalr ra, 0(ra)
  ret
.globl local_target
local_target:
  ret
  .byte 0, 0 # Require padding before the eight-byte-aligned stub/literal area.
.section .text.other,"ax",@progbits
.globl cross_target
cross_target:
  ret
