(module
  (type $target (func (result i32)))
  (func $callee (type $target) i32.const 47)
  (elem declare func $callee)
  (func $probe (param (ref $target)) (result i32)
    local.get 0 i32.const 0 if else end call_ref $target)
  (func (export "_start")
    ref.func $callee call $probe i32.const 47 i32.ne if unreachable end))
