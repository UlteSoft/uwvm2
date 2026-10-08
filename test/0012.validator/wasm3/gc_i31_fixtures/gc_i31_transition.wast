;; New standard Core3 syntax; pending actual official WAST execution.
(module
  (func (export "signed") (param i32) (result i32) local.get 0 ref.i31 i31.get_s)
  (func (export "unsigned") (param i32) (result i32) local.get 0 ref.i31 i31.get_u)
  (func (export "null") (result i32) ref.null i31 i31.get_u))
(assert_return (invoke "signed" (i32.const -1)) (i32.const -1))
(assert_return (invoke "unsigned" (i32.const -1)) (i32.const 2147483647))
(assert_return (invoke "signed" (i32.const -2147483648)) (i32.const 0))
(assert_return (invoke "signed" (i32.const 1073741824)) (i32.const -1073741824))
(assert_trap (invoke "null") "null i31 reference")
(assert_invalid (module (func i64.const 7 ref.i31 drop)) "type mismatch")
(assert_invalid (module (func ref.null any i31.get_s drop)) "type mismatch")
(assert_invalid (module (func unreachable ref.as_non_null ref.i31 drop)) "type mismatch")
