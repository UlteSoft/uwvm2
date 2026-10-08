;; Actual full-debug native lifecycle witness: Core 3 tail-call executes after
;; the genuine single-instruction trap is released from a closed pause domain.
(module
  (func $callee (result i32)
    i32.const 13)
  (func $caller (result i32)
    i32.const 0
    drop
    return_call $callee))
