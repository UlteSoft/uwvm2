(module
  (type $padding (func (param i32)))
  (global $prefix (mut i32) (i32.const 0))
  (global $after (mut i32) (i32.const 0))
  (func (export "child") (param $counter i64) (result i64) (local $hold (ref i31))
    global.get $prefix i32.const 1 i32.add global.set $prefix
    i32.const 7 ref.i31 local.set $hold
    i64.const 99 nop
    global.get $after i32.const 1 i32.add global.set $after
    local.get $counter i64.add
    local.get $hold i31.get_u i64.extend_i32_u i64.add)
  (func (result i32) global.get $prefix)
  (func (result i32) global.get $after))
