(module
  (type $answer_type (func (result i32)))
  (func $answer (type $answer_type) i32.const 42)
  (table 1 (ref func) (ref.func $answer))
  (elem $payload func $answer)
  (func (export "_start")
    i32.const 0
    i32.const 0
    i32.const 1
    table.init $payload
    i32.const 0
    call_indirect (type $answer_type)
    i32.const 42
    i32.ne
    if unreachable end))
