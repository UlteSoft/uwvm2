(module
  (type $result_i32 (func (result i32)))
  (func $target (type $result_i32) (result i32)
    i32.const 73)
  (global $target_ref (export "target_ref") funcref
    ref.func $target)
  (global (export "mutable_target_ref") (mut funcref)
    ref.null func))
