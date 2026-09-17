; RUN: llc -mtriple=ve -O3 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=ve -O3 -relocation-model=pic -verify-machineinstrs < %s | FileCheck %s
; Stack growth splits after physical argument allocation. Both arguments must
; remain live through the syscall block and into the continuation.
declare void @callee(ptr, ptr)
define void @caller(ptr %a, ptr %b) "disable-tail-calls"="true" {
; CHECK-LABEL: caller:
; CHECK: monc
; CHECK: bsic
  call void @callee(ptr %a, ptr %b)
  ret void
}
