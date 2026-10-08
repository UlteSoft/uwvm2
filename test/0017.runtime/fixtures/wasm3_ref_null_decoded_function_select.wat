(module
  (type $target (func (result i32)))
  (func $callee (type $target) i32.const 47)
  (elem declare func $callee)
  (func $probe (result i32)
    ref.null $target
    ref.func $callee
    i32.const 0
    select (result (ref null $target))
    ref.as_non_null
    call_ref $target)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
