;; Real builtin import returns EBADF. The guest verifies the numeric ABI result.
(module
  (import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))
  (memory (export "memory") 1)
  (func (export "_start")
    i32.const -1 call $close i32.const 8 i32.ne if unreachable end))
