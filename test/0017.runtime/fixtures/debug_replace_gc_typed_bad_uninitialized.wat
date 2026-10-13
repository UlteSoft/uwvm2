;; Intentionally invalid: the exact non-null local has no default value and
;; local.get must not read it before local.set. Official validation must fail.
;; https://webassembly.github.io/spec/core/valid/instructions.html#variable-instructions
(module
  (import "wasi_snapshot_preview1" "fd_write"
    (func $write (param i32 i32 i32 i32) (result i32)))
  (type $s (struct (field i32)))
  (type $other (struct (field i64)))
  (memory (export "memory") 1)
  (data (i32.const 128) "value=0\n")
  (func $target (param (ref null $s)) (result i32)
    (local (ref $s))
    local.get 1
    struct.get $s 0)
  (func (export "_start")
    i32.const 134
    i32.const 0
    struct.new $s
    call $target
    i32.const 48
    i32.add
    i32.store8
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 8 i32.store
    i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop))
