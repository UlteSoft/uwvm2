(module
  (import "provider_ok" "f" (func $f (param i64) (result i64)))
  (func (export "call_f") (param i64) (result i64)
    local.get 0
    call $f))

