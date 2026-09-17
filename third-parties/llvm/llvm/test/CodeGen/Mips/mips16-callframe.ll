; RUN: llc -mtriple=mipsel-linux-gnu -mcpu=mips32r2 -mattr=+mips16 -O3 -verify-machineinstrs < %s | FileCheck %s
; RUN: llc -mtriple=mips-linux-gnu -mcpu=mips32r2 -mattr=+mips16 -O3 -verify-machineinstrs < %s | FileCheck %s
; Soft-float argument preparation can split a select after call-frame setup.
; Each newly created diamond block must inherit its active frame size.
define void @minimum_quiet(ptr %out, ptr %in) {
; CHECK-LABEL: minimum_quiet:
; CHECK: jal
; CHECK: .end minimum_quiet
  %a = load <2 x i64>, ptr %in, align 1
  %bp = getelementptr i8, ptr %in, i32 16
  %b = load <2 x i64>, ptr %bp, align 1
  %af = bitcast <2 x i64> %a to <2 x double>
  %bf = bitcast <2 x i64> %b to <2 x double>
  %less = fcmp olt <2 x double> %af, %bf
  %min = select <2 x i1> %less, <2 x i64> %a, <2 x i64> %b
  %zeros = or <2 x i64> %a, %b
  %equal = fcmp oeq <2 x double> %af, %bf
  %chosen = select <2 x i1> %equal, <2 x i64> %zeros, <2 x i64> %min
  %nan = fcmp uno <2 x double> %af, %bf
  %result = select <2 x i1> %nan, <2 x i64> splat (i64 9221120237041090560), <2 x i64> %chosen
  store <2 x i64> %result, ptr %out, align 1
  ret void
}
