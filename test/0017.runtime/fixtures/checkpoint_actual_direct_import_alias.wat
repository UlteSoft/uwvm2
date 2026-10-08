(module
  (type $padding (func (result i32)))
  (import "checkpoint-direct-provider" "child" (func $actual (param i64) (result i64)))
  (export "child" (func $actual)))
