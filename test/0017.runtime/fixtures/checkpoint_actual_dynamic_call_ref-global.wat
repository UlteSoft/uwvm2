(module
  (type $identity_type (func (param i32) (result i32)))
  (type $child_type (func (param i64) (result i64)))
  (elem declare func $identity $child $wrong_child)
  (memory 1)
  (global $root_prefix (mut i32) (i32.const 0))
  (global $child_prefix (mut i32) (i32.const 0))
  (global $after_capture (mut i32) (i32.const 0))
  (global $wrong_calls (mut i32) (i32.const 0))
  (global $dispatch (mut (ref $child_type)) (ref.func $child))
  (func (export "_start") (result i32)
    global.get $root_prefix i32.const 1 i32.add global.set $root_prefix
    i32.const 0 i32.const 1234 i32.store
    i64.const 1000
    i64.const 70 global.get $dispatch call_ref $child_type
    i64.add i32.wrap_i64
    ref.func $identity call_ref $identity_type)
  (func $child (type $child_type) (param $counter i64) (result i64)
    (local $hold (ref i31))
    global.get $child_prefix i32.const 1 i32.add global.set $child_prefix
    i32.const 7 ref.i31 local.set $hold
    i64.const 99
    nop
    global.get $after_capture i32.const 1 i32.add global.set $after_capture
    local.get $counter i64.add
    local.get $hold i31.get_u i64.extend_i32_u i64.add)
  (func (result i32) global.get $root_prefix)
  (func (result i32) global.get $child_prefix)
  (func (result i32) i32.const 0 i32.load)
  (func i32.const 0 i32.const 4321 i32.store)
  (func $identity (type $identity_type) (param $value i32) (result i32) local.get $value)
  (func (result i32) global.get $after_capture)
  (func $wrong_child (type $child_type) (param i64) (result i64)
    global.get $wrong_calls i32.const 1 i32.add global.set $wrong_calls i64.const 666)
  (func ref.func $wrong_child global.set $dispatch)
  (func (result i32) global.get $wrong_calls)
  (func (result i32) i64.const 70 global.get $dispatch call_ref $child_type i32.wrap_i64))
