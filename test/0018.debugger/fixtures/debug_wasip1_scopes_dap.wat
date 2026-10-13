(module
  (import "wasi_snapshot_preview1" "args_sizes_get" (func $sizes (param i32 i32) (result i32)))
  (memory (export "memory") 1)
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (global $result (mut i32) (i32.const 0))
  (func $spin (export "spin") (result i32) (local $marker i32)
    i32.const 17 local.set $marker
    i32.const 0 i32.const 4 call $sizes drop
    (loop $again
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)
    local.get $marker)
  (func (export "_start") call $spin global.set $result))
