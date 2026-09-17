; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r2 -mattr=+mips16 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r2 -mattr=+mips16 -verify-machineinstrs < %s | FileCheck %s
; Constrained metadata is not a native ABI argument. Do not manufacture
; __call_stub_fp_llvm.experimental.constrained.* declarations for it.
define void @divide(ptr %out, ptr %in) strictfp {
; CHECK-LABEL: divide:
; CHECK-NOT: __call_stub_fp_llvm
; CHECK: jal
; CHECK-NOT: __call_stub_fp_llvm
; CHECK: .end divide
  %a = load double, ptr %in, align 1
  %next = getelementptr i8, ptr %in, i32 8
  %b = load double, ptr %next, align 1
  %q = call double @llvm.experimental.constrained.fdiv.f64(double %a, double %b,
      metadata !"round.dynamic", metadata !"fpexcept.strict")
  store double %q, ptr %out, align 1
  ret void
}
declare double @llvm.experimental.constrained.fdiv.f64(double, double, metadata, metadata)
