(module
  (memory i64 1 1 shared)
  (func (export "wait") (result i32)
    i64.const 0
    i32.const 0
    i64.const 200000000
    memory.atomic.wait32)
  (func (export "notify") (result i32)
    i64.const 0 i32.const 1 memory.atomic.notify))
