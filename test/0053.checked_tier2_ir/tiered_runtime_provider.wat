(module $checked_tier2_provider
  (memory i64 1)
  ;; One real loop creates the original same-walk OSR descriptor. The original
  ;; small-hot-loop shape (96..640 bytes, no FP) can promote this T0 callee; no
  ;; fake scheduler claim, raw symbol address or test-only entry patch is used.
  (func $increment (export "increment") (param $value i32) (result i32) (local $once i32)
    (loop $actual
      local.get $once i32.const 1 i32.add local.tee $once
      i32.const 1 i32.lt_u br_if $actual)
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop nop
    local.get $value i32.const 1 i32.add))
