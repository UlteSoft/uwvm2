(module
  (type $t (func (param i32) (result i32)))
  (import "A" "target" (func $imported (type $t)))
  (elem declare func $imported)
  ;; The consumer's function index 1 is a decoy. Provider index 1 must not be
  ;; reinterpreted as this function when ref.func names an imported function.
  (func $decoy (type $t) local.get 0 i32.const 11 i32.add)
  (func $tail (type $t)
    local.get 0 ref.func $imported return_call_ref $t)
  (func (export "_start")
    i32.const 0 ref.func $imported call_ref $t
    i32.const 73 i32.ne if unreachable end
    i32.const 0 ref.func $imported call_ref $t
    i32.const 73 i32.ne if unreachable end
    i32.const 0 call $tail
    i32.const 73 i32.ne if unreachable end))
