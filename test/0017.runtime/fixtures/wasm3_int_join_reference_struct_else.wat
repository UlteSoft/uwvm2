(module
  (type $box (struct (field i32)))
  (func $probe (param (ref $box)) (result i32)
    local.get 0 i32.const 0 if else end struct.get $box 0)
  (func (export "_start")
    i32.const 47 struct.new $box call $probe i32.const 47 i32.ne if unreachable end))
