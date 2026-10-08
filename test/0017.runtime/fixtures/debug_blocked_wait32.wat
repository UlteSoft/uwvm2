(module
  (memory 1 1 shared)
  (func (export "wait") (result i32)
    i32.const 0
    i32.const 0
    i64.const -1
    memory.atomic.wait32)
  (func (export "notify") (result i32)
    i32.const 0 i32.const 1 memory.atomic.notify))
