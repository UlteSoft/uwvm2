; Real LLVM backend object emission, never a VM or native-step authorization.
; Four definitions: original + internal same-ABI resume + raw adapter + unknown.
; ALL must have actual text begin/end relocation records in the opted-in module.
define dso_local i32 @wasm_body(i32 %x) #0 {
entry:
  %y = add i32 %x, 7
  ret i32 %y
}
define internal i32 @wasm_body.checkpoint.resume.v2(i32 %x) #1 {
entry:
  %y = add i32 %x, 13
  ret i32 %y
}
define dso_local i32 @wasm_body.checkpoint.resume.v2.raw(i32 %x) #2 {
entry:
  %y = call i32 @wasm_body.checkpoint.resume.v2(i32 %x)
  ret i32 %y
}
define dso_local i32 @unqualified_helper(i32 %x) #3 {
entry:
  %y = xor i32 %x, 19
  ret i32 %y
}
attributes #0 = { noinline "uwvm.native.code-owner.role"="1" }
attributes #1 = { noinline "uwvm.native.code-owner.role"="2" }
attributes #2 = { noinline "uwvm.native.code-owner.role"="3" }
attributes #3 = { noinline "uwvm.native.code-owner.role"="0" }
!llvm.module.flags = !{!0}
!0 = !{i32 1, !"uwvm.native.code-owner.table.version", i32 2}

; Extra actual object alias has no MachineFunction endpoint row. Ledger must
; retain/reject the additional function claim even if st_size is zero.
@unexpected_alias = alias i32 (i32), ptr @wasm_body
