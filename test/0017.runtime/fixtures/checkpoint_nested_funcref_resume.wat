(module
  (type $sig (func (result i32)))
  (elem declare func $one $two)
  (func (export "run") (result i32)
    ref.func $one i32.const 0
    if (param (ref $sig)) (result i32)
      nop call_ref $sig
    else
      nop call_ref $sig
    end)
  (func $one (type $sig) i32.const 11)
  (func $two (type $sig) i32.const 22))
