(module
  (type $answer_type (func (result i32)))
  (func $answer (type $answer_type) i32.const 42)
  (elem declare func $answer)
  (global $selected (ref func) (ref.func $answer))
  (table 1 funcref)
  ;; Element expressions use the complete module context, including this defined global.
  (elem (table 0) (i32.const 0) (ref func) (global.get $selected))
  (func (export "_start")
    i32.const 0
    call_indirect (type $answer_type)
    i32.const 42
    i32.ne
    if unreachable end))
