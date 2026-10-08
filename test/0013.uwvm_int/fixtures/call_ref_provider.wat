(module
  (type $t (func (param i32) (result i32)))
  ;; The provider target is function index 1. The consumer deliberately has a
  ;; different function at index 1, exposing any caller-index reinterpretation.
  (func $decoy (type $t) local.get 0 i32.const 19 i32.add)
  (func $target (export "target") (type $t)
    local.get 0 i32.const 73 i32.add))
