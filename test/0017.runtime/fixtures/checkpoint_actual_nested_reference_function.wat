(module
  (type $identity (func (result i32)))
  (elem declare func $target)
  (func (export "_start") (result funcref) call $child)
  (func $child (result funcref) i64.const 99 nop drop ref.func $target)
  (func $target (type $identity) (result i32) i32.const 42))
