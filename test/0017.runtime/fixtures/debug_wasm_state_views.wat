;; Modern Core3 state for real stopped-query tests. The debug reader must use
;; genuine compiled packets/GC roots, never parse this fixture to invent values.
(module
  (rec
    (type $node (struct
      (field (mut (ref null $node)))
      (field (mut i8))
      (field v128))))
  (type $numbers (array (mut i64)))
  (tag $event (param i32 (ref null $node) v128))
  (table $nodes (export "nodes64") i64 2 4 (ref null $node))
  (global $object (mut (ref null $node)) (ref.null $node))
  (global $alias (mut (ref null $node)) (ref.null $node))
  (global $numeric (mut i64) (i64.const -1))
  (global $vector v128 (v128.const i32x4 0x01020304 0x05060708 0x090a0b0c 0x0d0e0f10))
  (func (export "run") (result i32)
    (local $node (ref $node))
    (local $numbers (ref $numbers))
    (local $exception exnref)
    (local $external externref)
    nop ;; Nondefaultable $node/$numbers are unavailable, never fake null.
    ref.null $node
    i32.const 255
    v128.const i32x4 1 2 3 4
    struct.new $node
    local.set $node
    local.get $node
    local.get $node
    struct.set $node 0 ;; A real GC cycle; global and table aliases preserve it.
    local.get $node global.set $object
    local.get $node global.set $alias
    i64.const 0 local.get $node table.set $nodes
    i64.const 1 local.get $node table.set $nodes
    i64.const 10 i64.const 20 i64.const 30
    array.new_fixed $numbers 3
    local.set $numbers
    local.get $node extern.convert_any local.set $external
    i64.const 1234
    local.get $node
    nop ;; Actual operands: i64(1234), the same struct root as both globals.
    drop drop
    (block $caught (result exnref)
      (try_table (catch_all_ref $caught)
        i32.const 77
        local.get $node
        v128.const i32x4 5 6 7 8
        throw $event)
      unreachable)
    local.set $exception
    nop ;; No active handler: exn reference must show its real tag and payload.
    i32.const 42))
