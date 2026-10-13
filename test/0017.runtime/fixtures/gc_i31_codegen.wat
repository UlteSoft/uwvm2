;; Keep these typed exports live so the full JIT emits dynamic i31 machine code.
(module
  (func (export "sign_roundtrip") (param i32) (result i32)
    local.get 0 ref.i31 i31.get_s)
  (func (export "unsigned_roundtrip") (param i32) (result i32)
    local.get 0 ref.i31 i31.get_u)
  (func (export "_start")
    i32.const -1 call 0
    i32.const -1 i32.ne if unreachable end
    i32.const -1 call 1
    i32.const 2147483647 i32.ne if unreachable end))
