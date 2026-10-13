(module
  (tag $t (export "t") (param i32))
  (func (export "raise") (param i32)
    local.get 0
    throw $t))
