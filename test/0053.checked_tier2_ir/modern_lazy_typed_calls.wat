;; Real lazy-later local call, recursion, typed reference and tail-call syntax.
(module
  (type $answer (func (param i32) (result i32)))
  (memory i64 1)
  (elem declare func $sum)
  (func $sum (type $answer)
    local.get 0 i32.eqz
    if (result i32) i32.const 42
    else local.get 0 i32.const 1 i32.sub call $sum end)
  (func $tail (type $answer) local.get 0 return_call $sum)
  (func (export "_start")
    i32.const 3 call $tail i32.const 42 i32.ne if unreachable end
    i32.const 2 ref.func $sum call_ref $answer
    i32.const 42 i32.ne if unreachable end
    i32.const 9 ref.i31 i31.get_u i32.const 9 i32.ne if unreachable end))
