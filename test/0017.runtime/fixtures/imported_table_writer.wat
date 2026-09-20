(module
  (type $result_i32 (func (result i32)))
  (import "A" "shared" (table 1 funcref))
  (func $target (type $result_i32) (result i32)
    i32.const 83)
  ;; B writes B.target into A's table during instantiation.
  (elem (i32.const 0) func $target))
