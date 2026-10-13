;; Normal Core3 execution/output candidate. Official parse/validate and real
;; target execution are required; this source file is not an execution result.
;; Normative text forms: https://webassembly.github.io/spec/core/text/modules.html
;; https://webassembly.github.io/spec/core/text/instructions.html
;; Existing WASIp1 output ABI: https://raw.githubusercontent.com/WebAssembly/wasi-libc/main/libc-bottom-half/sources/__wasilibc_real.c
(module
  (type $box (struct (field i32)))
  (type $plus (func (param i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_write"
    (func $fd_write (param i32 i32 i32 i32) (result i32)))
  (tag $event (param i32))
  ;; WASIp1 uses only memory 0 with its existing 32-bit ciovec ABI.
  (memory $stdio (export "memory") 1)
  ;; Core3 assertions below explicitly use this second 64-bit memory.
  (memory $wide i64 1)
  (data (memory $stdio) (i32.const 32) "uwvm-core3-normal\n")
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
    i64.const 0 local.get $value i32.store $wide
    i64.const 0 i32.load $wide local.set $value
    local.get $value i32.const 31 i32.ne if unreachable end
    i64.const 0 table.get 0 ref.is_null if unreachable end
    i64.const 16 v128.const i32x4 1 2 3 4 v128.store $wide
    i64.const 16 v128.load $wide i32x4.extract_lane 0 i32.const 1 i32.ne if unreachable end
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
    memory.size $wide i64.const 1 i64.ne if unreachable end
    memory.size $stdio i32.const 1 i32.ne if unreachable end
    ;; One memory32 ciovec: pointer 32, length 18; nwritten at 8.
    ;; The output is emitted only after every modern Core assertion succeeds.
    i32.const 0 i32.const 32 i32.store $stdio
    i32.const 4 i32.const 18 i32.store $stdio
    i32.const 8 i32.const -1 i32.store $stdio
    i32.const 1 i32.const 0 i32.const 1 i32.const 8 call $fd_write
    i32.eqz i32.eqz if unreachable end
    i32.const 8 i32.load $stdio i32.const 18 i32.ne if unreachable end)
  (elem (i64.const 0) func $plus7))
