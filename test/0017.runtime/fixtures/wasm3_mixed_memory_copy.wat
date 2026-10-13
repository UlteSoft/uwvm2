;; Core 3: destination/source use their own address widths; length uses min.
;; https://webassembly.github.io/spec/core/valid/instructions.html#valid-memory-copy
(module
  (memory $m32 1)
  (memory $m64 i64 1)
  (func $copy_to64
    i64.const 4
    i32.const 0
    i32.const 1 ;; mixed-copy-count
    memory.copy $m64 $m32)
  (func $copy_to32
    i32.const 4
    i64.const 0
    i32.const 1
    memory.copy $m32 $m64)
  (func (export "_start")
    i32.const 0 i32.const 42 i32.store8 $m32
    i64.const 0 i32.const 17 i32.store8 $m64
    call $copy_to64
    call $copy_to32
    i64.const 4 i32.load8_u $m64 i32.const 42 i32.ne
    if unreachable end
    i32.const 4 i32.load8_u $m32 i32.const 17 i32.ne
    if unreachable end))
