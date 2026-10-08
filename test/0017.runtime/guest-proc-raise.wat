(module
  (import "wasi_snapshot_preview1" "proc_raise" (func $raise (param i32) (result i32)))
  (memory (export "memory") 1)
  (func (export "_start") i32.const 2 call $raise drop unreachable))
