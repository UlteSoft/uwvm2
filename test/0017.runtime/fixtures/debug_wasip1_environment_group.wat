(module
  (import "wasi_snapshot_preview1" "fd_read" (func $read (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1)
  (func (export "run") (result i32)
    nop nop nop
    i32.const 64 i32.const 128 i32.store
    i32.const 68 i32.const 3 i32.store
    i32.const 2 i32.const 64 i32.const 1 i32.const 72 call $read
    if unreachable end
    i32.const 72 i32.load i32.const 3 i32.ne if unreachable end
    i32.const 128 i32.load8_u i32.const 65 i32.ne if unreachable end
    i32.const 129 i32.load8_u if unreachable end
    i32.const 130 i32.load8_u i32.const 66 i32.ne if unreachable end
    i32.const 91))
