(module
  (type $expected_signature (func (param i32) (result i32)))
  (import "ref-provider" "plus" (func $plus (type $expected_signature)))
  (elem declare func $plus)
  (func (export "_start")
    i32.const 35 ref.func $plus call_ref $expected_signature
    i32.const 42 i32.ne if unreachable end)
)
