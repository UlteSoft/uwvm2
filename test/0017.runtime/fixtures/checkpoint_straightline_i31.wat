;; Core3 nondefaultable i31 local and an actual live i64 operand at nop.
;; Source callbacks 0 and6 are two distinct real private pause episodes.
(module
  (func (export "run") (result i32)
    (local (ref i31)) (local i64)
    i32.const -1
    ref.i31
    local.set 0
    i64.const 70
    local.set 1
    i64.const 80
    nop
    drop
    local.get 0
    i31.get_s
    i32.const 43
    i32.add))
