; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -mattr=+micromips -O3 -relocation-model=pic -verify-machineinstrs -show-mc-encoding < %s | FileCheck %s --check-prefixes=CHECK,LE
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -mattr=+micromips -O3 -relocation-model=pic -verify-machineinstrs -show-mc-encoding < %s | FileCheck %s --check-prefixes=CHECK,BE
;
; Probe MC encoding, not only the textual instruction names. show-mc-encoding
; invokes the emitter that writes object bytes, including relocation fixups.
; Register allocation/prologue/PseudoCVT expansion may choose standard opcodes
; after DAG selection; they still need microMIPS encodings in the MC emitter.
define void @convert(ptr %out, i32 %a) {
; CHECK-LABEL: convert:
; LE: cvt.s.w{{.*}}# encoding: [0x00,0x54,0x7b,0x3b]
; BE: cvt.s.w{{.*}}# encoding: [0x54,0x00,0x3b,0x7b]
; CHECK: .end convert
  %f = sitofp i32 %a to float
  store float %f, ptr %out
  ret void
}
define void @mask(ptr %out, double %a, double %b) {
; CHECK-LABEL: mask:
; LE: mfc1{{.*}}# encoding: [0x40,0x54,0x3b,0x20]
; BE: mfc1{{.*}}# encoding: [0x54,0x40,0x20,0x3b]
; CHECK: .end mask
  %p = fcmp une double %a, %b
  %i = sext i1 %p to i32
  store i32 %i, ptr %out
  ret void
}
define float @preserve_f32(float %a) {
; CHECK-LABEL: preserve_f32:
; CHECK: kind: fixup_MICROMIPS_HI16
; LE: mov.s{{.*}}# encoding: [0x8c,0x56,0x7b,0x00]
; BE: mov.s{{.*}}# encoding: [0x56,0x8c,0x00,0x7b]
; LE: mov.s{{.*}}# encoding: [0x14,0x54,0x7b,0x00]
; BE: mov.s{{.*}}# encoding: [0x54,0x14,0x00,0x7b]
; CHECK: .end preserve_f32
  call void @external()
  ret float %a
}
define double @preserve_f64(double %a) {
; CHECK-LABEL: preserve_f64:
; CHECK: kind: fixup_MICROMIPS_HI16
; LE: mov.d{{.*}}# encoding: [0x8c,0x56,0x7b,0x20]
; BE: mov.d{{.*}}# encoding: [0x56,0x8c,0x20,0x7b]
; LE: mov.d{{.*}}# encoding: [0x14,0x54,0x7b,0x20]
; BE: mov.d{{.*}}# encoding: [0x54,0x14,0x20,0x7b]
; CHECK: .end preserve_f64
  call void @external()
  ret double %a
}
declare void @external()
