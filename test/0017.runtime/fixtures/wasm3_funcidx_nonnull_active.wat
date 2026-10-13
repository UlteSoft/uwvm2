(module
  (type $answer_type (func (result i32)))
  (func $answer (type $answer_type) i32.const 42)
  (table 1 (ref func) (ref.func $answer))
  ;; Legacy funcidx encoding has the Core 3 type (ref func), not funcref.
  (elem (i32.const 0) func $answer)
  (func (export "_start")
    i32.const 0
    call_indirect (type $answer_type)
    i32.const 42
    i32.ne
    if unreachable end))
