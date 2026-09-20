; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -mattr=+micromips -mips-tail-calls -O3 -verify-machineinstrs < %s | FileCheck %s --check-prefix=R6
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -mattr=+micromips -mips-tail-calls -O3 -verify-machineinstrs < %s | FileCheck %s --check-prefix=R6
; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r2 -mattr=+micromips -mips-tail-calls -O3 -verify-machineinstrs < %s | FileCheck %s --check-prefix=R2
;
; Compact return and register-tailcall must not emit the pre-R6 JRC16_MM
; encoding on R6. That halfword decodes as MOVEP, not a return. The verifier
; does not currently check MIPS MC feature predicates, so check the opcode.
define void @empty() {
; R6-LABEL: empty:
; R6: jrc16 $ra
; R6: .end empty
; R2-LABEL: empty:
; R2: jrc $ra
; R2: .end empty
  ret void
}
define void @tail(ptr %f) {
; R6-LABEL: tail:
; R6: jrc16
; R6: .end tail
; R2-LABEL: tail:
; R2: jrc
; R2: .end tail
  musttail call void %f(ptr %f)
  ret void
}
