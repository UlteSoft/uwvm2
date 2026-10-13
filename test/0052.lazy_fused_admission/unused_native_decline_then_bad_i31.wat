;; A native lowering decline in the first function must not skip typing the
;; later unused invalid body. No pure validator or guest execution is involved.
(module
  (tag $problem)
  (func $declined (throw $problem))
  (func $invalid i64.const 7 ref.i31 drop)
  (func (export "answer") (result i32) i32.const 42))
