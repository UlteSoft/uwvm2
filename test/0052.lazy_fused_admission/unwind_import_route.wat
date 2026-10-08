(module
  (type $sig (func (param i64) (result i64)))
  (import "checked-route-provider" "alias" (func $alias (type $sig)))
  (memory i64 1)
  (func $target (type $sig)
    local.get 0 i64.const 2 i64.add)
  (func $answer (export "answer") (result i64)
    i64.const 0 call $target drop
    i64.const 40 call $alias))
