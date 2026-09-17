; RUN: llc -mtriple=msp430 -O3 -verify-machineinstrs < %s | FileCheck %s
; Soft-float calls require stack arguments. Selection of NaN quiet bits can
; split a block in the middle of that call-frame setup, before its matching
; teardown. The new blocks must inherit the active (nonzero) frame size.
declare <2 x double> @llvm.minimum.v2f64(<2 x double>, <2 x double>)
define void @minimum_quiet(ptr %out, ptr %in) {
; CHECK-LABEL: minimum_quiet:
; CHECK: call
; CHECK: ret
  %a = load <2 x double>, ptr %in, align 1
  %bp = getelementptr i8, ptr %in, i16 16
  %b = load <2 x double>, ptr %bp, align 1
  %min = call <2 x double> @llvm.minimum.v2f64(<2 x double> %a, <2 x double> %b)
  %raw = bitcast <2 x double> %min to <2 x i64>
  %nan = fcmp uno <2 x double> %a, %b
  %quiet = select <2 x i1> %nan, <2 x i64> splat (i64 2251799813685248), <2 x i64> zeroinitializer
  %result = or <2 x i64> %raw, %quiet
  store <2 x i64> %result, ptr %out, align 1
  ret void
}
