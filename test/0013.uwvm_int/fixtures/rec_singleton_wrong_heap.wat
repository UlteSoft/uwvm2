(module
  (rec (type $t (func (param (ref null $t)) (result (ref null $t)))))
  (func $identity (type $t)
    local.get 0)
  (elem declare func $identity)
  (func (export "_start")
    ref.null func
    ref.func $identity
    call_ref $t
    drop))
