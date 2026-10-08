;; Core3 address remains i64 even on a native 32-bit target.
;; The low-address witness succeeds; the last i32.load must trap memory OOB.
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
    i64.const 4294967296
    i32.load
    drop))
