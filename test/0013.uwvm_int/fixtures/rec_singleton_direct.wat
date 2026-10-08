(module
  (rec (type $t (func (result i32))))
  (func $answer (type $t)
    i32.const 42)
  (func (export "_start")
    call $answer
    i32.const 42
    i32.ne
    if unreachable end))
