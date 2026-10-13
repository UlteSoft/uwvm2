; A non-moving collector may ask LLVM to record both halves of UWVM's
; integer-carried reference explicitly. This is exploratory IR, not product JIT.
; The value is live across the helper call so ABI preservation can be inspected.
declare void @allocator(ptr) nounwind
declare void @llvm.experimental.stackmap(i64, i32, ...)

define i64 @integer_reference_safepoint(i128 %tagged_ref, i64 %seed) nounwind {
entry:
  %payload = trunc i128 %tagged_ref to i64
  %shifted = lshr i128 %tagged_ref, 64
  %kind = trunc i128 %shifted to i64
  call void (i64, i32, ...) @llvm.experimental.stackmap(i64 9001, i32 0, i64 %payload, i64 %kind)
  call void @allocator(ptr null)
  %a = xor i64 %payload, %seed
  %b = add i64 %a, %kind
  ret i64 %b
}
