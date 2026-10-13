;; Core 3 table.copy uses min(destination width, source width) for length.
;; table.init keeps i32 source/length even when its destination table is i64.
;; https://webassembly.github.io/spec/core/valid/instructions.html#table-instructions
(module
  (type $sig (func (result i32)))
  (table $wide i64 4 8 funcref)
  (table $narrow 4 funcref)
  (func $first (type $sig) i32.const 11)
  (func $second (type $sig) i32.const 22)
  (elem $items func $first $second)
  (func (export "_start")
    i64.const 0 i32.const 0 i32.const 2 table.init $wide $items
    elem.drop $items
    i64.const 0 call_indirect $wide (type $sig)
    i32.const 11 i32.ne if unreachable end
    i64.const 1 call_indirect $wide (type $sig)
    i32.const 22 i32.ne if unreachable end

    i32.const 0 i64.const 1 i32.const 1 table.copy $narrow $wide
    i32.const 0 call_indirect $narrow (type $sig)
    i32.const 22 i32.ne if unreachable end
    i64.const 2 i32.const 0 i32.const 1 table.copy $wide $narrow
    i64.const 2 call_indirect $wide (type $sig)
    i32.const 22 i32.ne if unreachable end

    ;; Overlapping rightward copy must preserve the original second function.
    i64.const 1 i64.const 0 i64.const 2 table.copy $wide $wide
    i64.const 1 call_indirect $wide (type $sig)
    i32.const 11 i32.ne if unreachable end
    i64.const 2 call_indirect $wide (type $sig)
    i32.const 22 i32.ne if unreachable end

    i64.const 2 ref.null func i64.const 1 table.fill $wide
    i64.const 2 table.get $wide ref.is_null i32.eqz if unreachable end
    ref.func $second i64.const 1 table.grow $wide
    i64.const 4 i64.ne if unreachable end
    table.size $wide i64.const 5 i64.ne if unreachable end
    i64.const 4 call_indirect $wide (type $sig)
    i32.const 22 i32.ne if unreachable end
    i64.const 3 ref.func $first table.set $wide
    i64.const 3 call_indirect $wide (type $sig)
    i32.const 11 i32.ne if unreachable end))
