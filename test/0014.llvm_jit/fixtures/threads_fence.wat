;; Threads: a fence is valid without any memory and preserves live stack values.
(module
  (func (export "_start")
    i32.const 42
    atomic.fence
    i32.const 43
    i32.const 1
    i32.sub
    i32.ne
    if unreachable end))
