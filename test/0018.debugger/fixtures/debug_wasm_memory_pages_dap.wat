(module
  (memory $mem32 (export "memory32") 2)
  (memory $mem64 (export "memory64") i64 2)
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (global $result (mut i32) (i32.const 0))
  (func $spin (export "spin") (result i32) (local $marker i32)
    i32.const 17 local.set $marker
    (loop $again
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)
    local.get $marker)
  (func (export "_start")
    i32.const 0 i32.const 81 i32.const 131072 memory.fill $mem32
    i64.const 0 i32.const 167 i64.const 131072 memory.fill $mem64
    call $spin global.set $result))
