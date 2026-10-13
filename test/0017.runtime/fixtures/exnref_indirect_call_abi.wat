;; Core 3 exnref values must retain their complete ABI slot through an indirect call.
(module
  (type $identity (func (param exnref) (result exnref)))
  (func $identity (type $identity)
    local.get 0)
  (table 1 funcref)
  (elem (i32.const 0) $identity)
  (func (export "_start")
    ref.null exn
    i32.const 0
    call_indirect (type $identity)
    ref.is_null
    i32.eqz
    if unreachable end))
