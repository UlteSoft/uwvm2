(module
  (type $result_i32 (func (result i32)))
  (import "P" "target" (func $target (type $result_i32)))
  ;; The global points at A's imported-function slot, which ultimately resolves to P.target.
  (global $target_ref (export "target_ref") funcref
    ref.func $target)
  (global (export "mutable_target_ref") (mut funcref)
    ref.null func))
