;; A Core 3 legacy 0x60 function type transports exnref through a real call.
(module
  (type $identity (func (param exnref) (result exnref)))
  (func $identity (type $identity)
    local.get 0)
  (func (export "_start")
    ref.null exn
    call $identity
    ref.is_null
    i32.eqz
    if unreachable end))
