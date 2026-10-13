(module
  ;; Real aggregate cohort and live reference locals force precise GC-root
  ;; prologues across recursive entries and the Core 3 typed tail transfer.
  (type $payload (struct (field (mut i32))))
  (func $helper (result i32)
    (local $held (ref null $payload))
    i32.const 7 struct.new $payload local.set $held
    local.get $held struct.get $payload 0)
  (func $recursive (param $remaining i32) (result i32)
    (local $held (ref null $payload))
    local.get $remaining struct.new $payload local.set $held
    local.get $remaining i32.eqz
    if (result i32)
      return_call $helper
    else
      local.get $remaining i32.const 1 i32.sub call $recursive
      ;; Keep a real reference live across the recursive call, then verify its
      ;; numeric field. This is not an unused type-section-only GC witness.
      local.get $held struct.get $payload 0 local.get $remaining i32.ne
      if unreachable end
    end)
  (func (export "_start") (result i32)
    (local $held (ref null $payload))
    i32.const 42 struct.new $payload local.set $held
    nop i32.const 3 call $recursive
    i32.const 3 call $recursive i32.add
    local.get $held struct.get $payload 0 i32.const 42 i32.ne
    if unreachable end))
