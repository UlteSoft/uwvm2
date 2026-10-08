;; Core3 memory64 with the Threads extension takes an i64 atomic address.
;; 2^32 is 8-byte aligned. The last i64.atomic.load must trap memory OOB,
;; never succeed at truncated address zero or fail as an unaligned access.
(module
  (memory (export "memory") i64 1 1 shared)
  (func (export "_start")
    i64.const 0
    i64.const 123
    i64.atomic.store
    i64.const 0
    i64.atomic.load
    i64.const 123
    i64.ne
    if unreachable end
    i64.const 4294967296
    i64.atomic.load
    drop))
