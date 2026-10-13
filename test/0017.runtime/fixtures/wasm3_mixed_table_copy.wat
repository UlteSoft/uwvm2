;; The count stays i32 in both directions between a table32 and table64.
;; Real indirect calls verify the copied identities, rather than two nulls.
;; https://webassembly.github.io/spec/core/valid/instructions.html#valid-table-copy
(module
  (type $signature (func (result i32)))
  (table $t32 2 funcref)
  (table $t64 i64 2 funcref)
  (func $a (type $signature) i32.const 42)
  (func $b (type $signature) i32.const 17)
  (elem declare func $a $b)
  (func $copy_to64
    i64.const 1
    i32.const 0
    i32.const 1 ;; mixed-copy-count
    table.copy $t64 $t32)
  (func $copy_to32
    i32.const 1
    i64.const 0
    i32.const 1
    table.copy $t32 $t64)
  (func (export "_start")
    i32.const 0 ref.func $a table.set $t32
    i64.const 0 ref.func $b table.set $t64
    call $copy_to64
    call $copy_to32
    i64.const 1 call_indirect $t64 (type $signature)
    i32.const 42 i32.ne if unreachable end
    i32.const 1 call_indirect $t32 (type $signature)
    i32.const 17 i32.ne if unreachable end))
