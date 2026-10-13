(module $checked_tier2_consumer
  (import "P" "increment" (func $increment (param i32) (result i32)))
  (memory i64 1)
  (func $_start (export "_start") (local $value i32)
    ;; The foreign import uses the real original tiered bridge on each call;
    ;; its actual native switch counter requests provider T2 at the real
    ;; centralized threshold. Same-module typed fastcalls do not provide that
    ;; signal and must not be presented as a reliable T2 request fixture.
    (loop $calls
      local.get $value call $increment local.set $value
      local.get $value i32.const 4000000 i32.lt_u br_if $calls)
    ;; Reference-types typed select syntax and real guest checksum. No host
    ;; sleep or debugger/checkpoint/native memory operation requests promotion.
    local.get $value i32.const 0 i32.const 1 select (result i32)
    i32.const 4000000 i32.ne if unreachable end))
