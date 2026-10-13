(module
  (type $padding (func))
  (type $actual_signature (func (param i32) (result i32)))
  (func $plus (export "plus") (type $actual_signature) (param i32) (result i32)
    local.get 0 i32.const 7 i32.add)
)
