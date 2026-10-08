(module
  (type $t (func))
  (func $f (param (ref null $t)) (result funcref) local.get 0)
  (func (param funcref) (result funcref)
    ref.null $t
    local.get 0
    br_on_null 0
    drop
    call $f))
