; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -target-abi=o32 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -target-abi=o32 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips64el-linux-gnuabi64 -mcpu=mips64r6 -target-abi=n64 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips64-linux-gnuabi64 -mcpu=mips64r6 -target-abi=n64 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r6 -mattr=+micromips -target-abi=o32 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r6 -mattr=+micromips -target-abi=o32 -verify-machineinstrs < %s | FileCheck %s

; R6 FP comparisons produce 0/-1, unlike integer comparisons (0/1).
; Legalizing UNE through the inverse EQ must XOR with all ones, not 1.
; A zext-to-i32 scalar test alone hides the error because only bit zero is
; observed. Sign-extended Wasm SIMD masks expose -2 instead of 0 for EQ inputs.
; Unaligned byte-buffer parameters also avoid testing a platform vector ABI.
define void @fcmp_ne_s(ptr %out, ptr %left, ptr %right) {
; CHECK-LABEL: fcmp_ne_s:
; CHECK-NOT: xori
; CHECK: cmp.eq.s
; CHECK: {{not|nor}}
; CHECK-NOT: xori
  %a = load <4 x float>, ptr %left, align 1
  %b = load <4 x float>, ptr %right, align 1
  %cmp = fcmp une <4 x float> %a, %b
  %mask = sext <4 x i1> %cmp to <4 x i32>
  store <4 x i32> %mask, ptr %out, align 1
  ret void
}

define void @fcmp_ne_d(ptr %out, ptr %left, ptr %right) {
; CHECK-LABEL: fcmp_ne_d:
; CHECK-NOT: xori
; CHECK: cmp.eq.d
; CHECK: {{not|nor}}
; CHECK-NOT: xori
  %a = load <2 x double>, ptr %left, align 1
  %b = load <2 x double>, ptr %right, align 1
  %cmp = fcmp une <2 x double> %a, %b
  %mask = sext <2 x i1> %cmp to <2 x i64>
  store <2 x i64> %mask, ptr %out, align 1
  ret void
}

; The same inversion path handles strict FP nodes. Preserve their chain and
; use the FP operand type rather than its integer comparison result here too.
define i32 @strict_ne_s(float %a, float %b) strictfp {
; CHECK-LABEL: strict_ne_s:
; CHECK-NOT: xori
; CHECK: cmp.eq.s
; CHECK: {{not|nor}}
; CHECK-NOT: xori
  %cmp = call i1 @llvm.experimental.constrained.fcmp.f32(float %a, float %b,
      metadata !"une", metadata !"fpexcept.ignore")
  %mask = sext i1 %cmp to i32
  ret i32 %mask
}

declare i1 @llvm.experimental.constrained.fcmp.f32(float, float, metadata, metadata)

; An unused result does not make a strict comparison dead. Quiet EQ must
; still raise Invalid for sNaN; signaling EQ also raises it for qNaN.
; Checking both functions catches a selector that drops the strict chain
; even if every observed comparison result is otherwise correct.
define void @strict_unused_quiet_s(float %a, float %b) strictfp {
; CHECK-LABEL: strict_unused_quiet_s:
; CHECK: cmp.eq.s
  %cmp = call i1 @llvm.experimental.constrained.fcmp.f32(float %a, float %b,
      metadata !"oeq", metadata !"fpexcept.strict")
  ret void
}

define void @strict_unused_signaling_d(double %a, double %b) strictfp {
; CHECK-LABEL: strict_unused_signaling_d:
; CHECK: cmp.seq.d
  %cmp = call i1 @llvm.experimental.constrained.fcmps.f64(double %a, double %b,
      metadata !"oeq", metadata !"fpexcept.strict")
  ret void
}

declare i1 @llvm.experimental.constrained.fcmps.f64(double, double, metadata, metadata)
