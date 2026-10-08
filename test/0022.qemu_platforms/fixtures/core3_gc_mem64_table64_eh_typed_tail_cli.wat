;; Source-only Core3 CLI execution candidate. Official parse/validate and real
;; target execution are required; this does not qualify debugger event capture.
;; Normative text forms: https://webassembly.github.io/spec/core/text/modules.html
;; https://webassembly.github.io/spec/core/text/instructions.html
(module
  (type $box (struct (field i32)))
  (type $plus (func (param i32) (result i32)))
  (tag $event (param i32))
  (memory i64 1)
  (table i64 1 funcref)
  (func $plus7 (type $plus) (param i32) (result i32)
    local.get 0 i32.const 7 i32.add)
  (func $raise (param i32)
    local.get 0 throw $event)
  (func $run (result i32)
    (local $object (ref null $box))
    (local $value i32)
    i32.const 31 struct.new $box local.set $object
    local.get $object ref.as_non_null struct.get $box 0 local.set $value
    i64.const 0 local.get $value i32.store
    i64.const 0 i32.load local.set $value
    local.get $value i32.const 31 i32.ne if unreachable end
    i64.const 0 table.get 0 ref.is_null if unreachable end
    i64.const 16 v128.const i32x4 1 2 3 4 v128.store
    i64.const 16 v128.load i32x4.extract_lane 0 i32.const 1 i32.ne if unreachable end
    block $caught (result i32)
      try_table (catch $event $caught)
        i32.const 42 call $raise
      end
      unreachable
    end
    local.set $value
    local.get $object ref.as_non_null struct.get $box 0
    i32.const 31 i32.ne if unreachable end
    local.get $value return_call $plus7)
  (func $via_table (param i32) (result i32)
    local.get 0
    i64.const 0 table.get 0 ref.cast (ref $plus)
    return_call_ref $plus)
  (func (export "_start")
    call $run i32.const 49 i32.ne if unreachable end
    i32.const 35 ref.func $plus7 call_ref $plus
    i32.const 42 i32.ne if unreachable end
    i32.const 35 call $via_table
    i32.const 42 i32.ne if unreachable end
    table.size i64.const 1 i64.ne if unreachable end
    memory.size i64.const 1 i64.ne if unreachable end)
  (elem (i64.const 0) func $plus7))
