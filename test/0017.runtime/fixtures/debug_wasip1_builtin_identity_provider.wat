;; Same original builtin function, distinct real receiving instance.
(module
  (type $sizes (func (param i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "args_sizes_get" (func $as (type $sizes)))
  (export "args_sizes_get" (func $as))
  (memory (export "memory") 1)
  (func (export "run") (result i32)
    i32.const 0 i32.const 4 call $as if unreachable end
    i32.const 0 i32.load i32.const 2 i32.ne if unreachable end
    i32.const 4 i32.load i32.const 13 i32.ne if unreachable end
    i32.const 91))
