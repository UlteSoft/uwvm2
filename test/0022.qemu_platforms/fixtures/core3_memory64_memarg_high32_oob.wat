;; Core3 memarg offset remains u64 even on a native 32-bit target.
;; The last i32.load adds offset 2^32 to address zero without truncation.
(module
  (memory (export "memory") i64 1 1)
  (func (export "_start")
    i64.const 0
    i32.const 123
    i32.store
    i64.const 0
    i32.load
    i32.const 123
    i32.ne
    if unreachable end
    i64.const 0
    i32.load offset=4294967296
    drop))
