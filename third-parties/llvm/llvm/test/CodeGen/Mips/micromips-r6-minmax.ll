; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -mattr=+micromips -verify-machineinstrs < %s | FileCheck %s
;
; R6 min/max and canonicalization are Legal in both encodings. The standard
; encoding patterns are guarded by NotInMicroMips and cannot implement these.
define float @f32(float %a, float %b, float %c) {
; CHECK-LABEL: f32:
; CHECK: min.s
; CHECK: max.s
; CHECK: min.s
; CHECK: .end f32
  %lo = call float @llvm.minnum.f32(float %a, float %b)
  %hi = call float @llvm.maxnum.f32(float %lo, float %c)
  %q = call float @llvm.canonicalize.f32(float %hi)
  ret float %q
}
define double @f64(double %a, double %b, double %c) {
; CHECK-LABEL: f64:
; CHECK: min.d
; CHECK: max.d
; CHECK: min.d
; CHECK: .end f64
  %lo = call double @llvm.minnum.f64(double %a, double %b)
  %hi = call double @llvm.maxnum.f64(double %lo, double %c)
  %q = call double @llvm.canonicalize.f64(double %hi)
  ret double %q
}
declare float @llvm.minnum.f32(float, float)
declare float @llvm.maxnum.f32(float, float)
declare float @llvm.canonicalize.f32(float)
declare double @llvm.minnum.f64(double, double)
declare double @llvm.maxnum.f64(double, double)
declare double @llvm.canonicalize.f64(double)
