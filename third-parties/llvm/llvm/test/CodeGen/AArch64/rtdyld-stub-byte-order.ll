; RUN: llc -mtriple=aarch64-linux-gnu -filetype=obj %s -o %T/uwvm-aarch64-stub-le.o
; RUN: llvm-rtdyld -verify -triple=aarch64-linux-gnu -dummy-extern=external_target=0x11223344 -check=%S/rtdyld-stub-le.check %T/uwvm-aarch64-stub-le.o
; RUN: llc -mtriple=aarch64_be-linux-gnu -filetype=obj %s -o %T/uwvm-aarch64-stub-be.o
; RUN: llvm-rtdyld -verify -triple=aarch64_be-linux-gnu -dummy-extern=external_target=0x11223344 -check=%S/rtdyld-stub-be.check %T/uwvm-aarch64-stub-be.o
;
; Instructions remain LE for ELF BE. Data-endian stub writes produced invalid
; instructions even though object emission and RuntimeDyld loading succeeded.
; Check all five relocated words, not just the final BR or printed mnemonics.
declare void @external_target()
define void @entry() "disable-tail-calls"="true" {
  call void @external_target()
  ret void
}
