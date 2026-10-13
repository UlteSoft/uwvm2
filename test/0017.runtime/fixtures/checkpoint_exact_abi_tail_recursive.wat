(module
  (type $tail_type (func (param i64) (result i64)))
  (table $tail_table 1 funcref)
  (elem (table $tail_table) (i32.const 0) func $tail_final)
  (elem declare func $tail_final)
  (type $identity_type (func (param i32) (result i32)))
  (elem declare func $identity)
  (memory 1)
  (global $root_prefix (mut i32) (i32.const 0))
  (global $child_prefix (mut i32) (i32.const 0))
  (global $after_capture (mut i32) (i32.const 0))
  (func (export "_start") (result i32)
    global.get $root_prefix i32.const 1 i32.add global.set $root_prefix
    i32.const 0 i32.const 1234 i32.store
    i64.const 1000
    i64.const 70 call $child
    i64.add i32.wrap_i64
    ref.func $identity call_ref $identity_type)
  (func $child (param $counter i64) (result i64)
    (local $hold (ref i31))
    global.get $child_prefix i32.const 1 i32.add global.set $child_prefix
    i32.const 7 ref.i31 local.set $hold
    i64.const 99
    nop
    global.get $after_capture i32.const 1 i32.add global.set $after_capture
    local.get $counter i64.add
    local.get $hold i31.get_u i64.extend_i32_u i64.add
    return_call $tail_final)
  (func (result i32) global.get $root_prefix)
  (func (result i32) global.get $child_prefix)
  (func (result i32) i32.const 0 i32.load)
  (func i32.const 0 i32.const 4321 i32.store)
  (func $identity (type $identity_type) (param $value i32) (result i32) local.get $value)
  (func (result i32) global.get $after_capture)
  (func $tail_final (type $tail_type) (param $value i64) (result i64)
    local.get $value i64.const 160 i64.le_u
    if (result i64)
      local.get $value
    else
      local.get $value i64.const 1 i64.sub call $tail_final i64.const 1 i64.add
    end)
)
