(module
  (func (export "_start"))
  ;; Unused modern GC body: ref.i31 consumes i32, not i64.
  (func (result i32) i64.const 42 ref.i31 i31.get_u)
)
