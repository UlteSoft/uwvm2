;; Memory64 uses i64 addresses and returns an i64 page count.
(module
  (memory i64 1)
  (func (export "_start")
    i64.const 16
    i32.const 0x12345678
    i32.store
    i64.const 16
    i32.load
    i32.const 0x12345678
    i32.ne
    if unreachable end
    memory.size
    i64.const 1
    i64.ne
    if unreachable end))
