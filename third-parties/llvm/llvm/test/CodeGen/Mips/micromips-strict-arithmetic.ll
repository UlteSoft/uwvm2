; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r2 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r2 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
;
; Strict arithmetic is Legal in the MIPS lowering contract, so microMIPS must
; match it as well as ordinary arithmetic. Discarding the final result must not
; discard any of the exception-producing operations or their ordered chain.
define void @strict_f32(float %a, float %b) strictfp {
; CHECK-LABEL: strict_f32:
; CHECK: add.s
; CHECK: sub.s
; CHECK: mul.s
; CHECK: div.s
; CHECK: sqrt.s
; CHECK: .end strict_f32
  %c = call float @llvm.experimental.constrained.fadd.f32(float %a, float %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %d = call float @llvm.experimental.constrained.fsub.f32(float %c, float %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %e = call float @llvm.experimental.constrained.fmul.f32(float %d, float %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %f = call float @llvm.experimental.constrained.fdiv.f32(float %e, float %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %g = call float @llvm.experimental.constrained.sqrt.f32(float %f, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret void
}
define void @strict_f64(double %a, double %b) strictfp {
; CHECK-LABEL: strict_f64:
; CHECK: add.d
; CHECK: sub.d
; CHECK: mul.d
; CHECK: div.d
; CHECK: sqrt.d
; CHECK: .end strict_f64
  %c = call double @llvm.experimental.constrained.fadd.f64(double %a, double %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %d = call double @llvm.experimental.constrained.fsub.f64(double %c, double %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %e = call double @llvm.experimental.constrained.fmul.f64(double %d, double %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %f = call double @llvm.experimental.constrained.fdiv.f64(double %e, double %b, metadata !"round.dynamic", metadata !"fpexcept.strict")
  %g = call double @llvm.experimental.constrained.sqrt.f64(double %f, metadata !"round.dynamic", metadata !"fpexcept.strict")
  ret void
}
declare float @llvm.experimental.constrained.fadd.f32(float, float, metadata, metadata)
declare float @llvm.experimental.constrained.fsub.f32(float, float, metadata, metadata)
declare float @llvm.experimental.constrained.fmul.f32(float, float, metadata, metadata)
declare float @llvm.experimental.constrained.fdiv.f32(float, float, metadata, metadata)
declare float @llvm.experimental.constrained.sqrt.f32(float, metadata, metadata)
declare double @llvm.experimental.constrained.fadd.f64(double, double, metadata, metadata)
declare double @llvm.experimental.constrained.fsub.f64(double, double, metadata, metadata)
declare double @llvm.experimental.constrained.fmul.f64(double, double, metadata, metadata)
declare double @llvm.experimental.constrained.fdiv.f64(double, double, metadata, metadata)
declare double @llvm.experimental.constrained.sqrt.f64(double, metadata, metadata)
