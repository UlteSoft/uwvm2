(module
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (global $result (mut i32) (i32.const 0))
  (func $spin (export "spin") (result i32)
    (loop $again
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again)
    i32.const 77)
  (func (export "_start") call $spin global.set $result))
