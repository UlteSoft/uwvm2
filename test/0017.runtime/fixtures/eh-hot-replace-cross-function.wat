;; The debugger replaces $raise before guest entry. The replacement keeps the
;; exact () -> () Wasm ABI and changes only the thrown i32 payload from 29 to
;; 30. Two mutable-table calls preserve both generated cross-function edges.
(module
  (type $void (func))
  (import "wasi_snapshot_preview1" "fd_write"
    (func $write (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 128) "eh-replaced-value=29\0a")
  (table 2 funcref)
  (elem (i32.const 0) $raise $relay)
  (tag $event (param i32))
  (func $raise (type $void)
    i32.const 29
    throw $event)
  (export "hot_target" (func $raise))
  (func $relay (type $void)
    i32.const 0
    call_indirect (type $void))
  (func (export "_start") (local $payload i32)
    (block $caught (result i32)
      try_table (catch $event $caught)
        i32.const 1
        call_indirect (type $void)
      end
      unreachable)
    local.set $payload
    i32.const 146
    local.get $payload
    i32.const 10
    i32.div_u
    i32.const 48
    i32.add
    i32.store8
    i32.const 147
    local.get $payload
    i32.const 10
    i32.rem_u
    i32.const 48
    i32.add
    i32.store8
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 21 i32.store
    i32.const 1 i32.const 0 i32.const 1 i32.const 100 call $write drop))
