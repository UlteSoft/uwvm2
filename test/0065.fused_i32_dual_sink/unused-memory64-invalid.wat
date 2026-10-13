(module
  (memory i64 1)
  (func (export "_start"))
  ;; Unused body is invalid: memory64 requires an i64 address.
  (func (result i32) i32.const 0 i32.load)
)
