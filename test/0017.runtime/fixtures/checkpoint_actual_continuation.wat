(module
  (memory 1)
  (global $prefix (mut i32) (i32.const 0))
  (func (export "_start") (result i32)
    (local $counter i64) (local $hold (ref i31))
    global.get $prefix i32.const 1 i32.add global.set $prefix
    i32.const 0 i32.const 1234 i32.store
    i32.const 7 ref.i31 local.set $hold
    i64.const 70 local.set $counter
    i64.const 99
    nop
    local.get $counter i64.add i32.wrap_i64
    local.get $hold i31.get_u i32.add)
  (func (result i32) global.get $prefix)
  (func (result i32) i32.const 0 i32.load)
  (func i32.const 0 i32.const 4321 i32.store))
