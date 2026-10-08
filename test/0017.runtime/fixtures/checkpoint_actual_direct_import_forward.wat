(module
  (type $child_type (func (param i64) (result i64)))
  (type $identity_type (func (param i32) (result i32)))
  (import "checkpoint-direct-alias" "child" (func $child (type $child_type)))
  (elem declare func $identity)
  (memory 1)
  (global $prefix (mut i32) (i32.const 0))
  (func (export "_start") (result i32)
    global.get $prefix i32.const 1 i32.add global.set $prefix
    i32.const 0 i32.const 1234 i32.store
    i64.const 1000 i64.const 70 call $child
    i64.add i32.wrap_i64
    ref.func $identity call_ref $identity_type)
  (func (result i32) global.get $prefix)
  (func (result i32) i32.const 0 i32.load)
  (func i32.const 0 i32.const 4321 i32.store)
  (func $identity (type $identity_type) (param $value i32) (result i32) local.get $value))
