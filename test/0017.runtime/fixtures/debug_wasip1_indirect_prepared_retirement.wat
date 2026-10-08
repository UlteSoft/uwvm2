;; Real WASIp1 imports consume the manager's edited owned environment AFTER
;; resume. GC syntax/typed roots ensure this is a Core3 stopped-state test.
(module
  (type $node (struct (field i32)))
  (import "wasi_snapshot_preview1" "args_sizes_get" (func $as (param i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "args_get" (func $ag (param i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "environ_sizes_get" (func $es (param i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "environ_get" (func $eg (param i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_close" (func $close (param i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_read" (func $read (param i32 i32 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 600) "xyz!")
  (func (export "run") (result i32)
    (local $node (ref $node))
    i32.const 7 struct.new $node local.set $node
    nop ;; Actual opcode ordinal3: real node root and owned env are live.
    call $mutate_managed
    i32.const 0 i32.const 4 call $as if unreachable end
    i32.const 16 i32.const 128 call $ag if unreachable end
    i32.const 8 i32.const 12 call $es if unreachable end
    i32.const 32 i32.const 256 call $eg if unreachable end
    i32.const 0 i32.load i32.const 2 i32.ne if unreachable end
    i32.const 4 i32.load i32.const 12 i32.ne if unreachable end
    i32.const 8 i32.load i32.const 1 i32.ne if unreachable end
    i32.const 12 i32.load i32.const 19 i32.ne if unreachable end
    i32.const 20 i32.load i32.const 134 i32.ne if unreachable end
    i32.const 134 i32.load i32.const 0x65746661 i32.ne if unreachable end
    i32.const 138 i32.load8_u i32.const 114 i32.ne if unreachable end
    i32.const 139 i32.load8_u if unreachable end
    i32.const 32 i32.load i32.const 256 i32.ne if unreachable end
    i32.const 271 i32.load i32.const 0x0077656e i32.ne if unreachable end
    ;; Original tracked WASIp1 guest close drops the live FD root after capture.
    i32.const 151 call $close if unreachable end
    i32.const 64 i32.const 512 i32.store
    i32.const 68 i32.const 3 i32.store
    i32.const 3 i32.const 64 i32.const 1 i32.const 72 call $read if unreachable end
    i32.const 72 i32.load i32.const 3 i32.ne if unreachable end
    i32.const 512 i32.load8_u i32.const 65 i32.ne if unreachable end
    i32.const 513 i32.load8_u if unreachable end
    i32.const 514 i32.load8_u i32.const 66 i32.ne if unreachable end
    ;; FD2 aliases FD3: its shared cursor already reached EOF after first read.
    i32.const 2 i32.const 64 i32.const 1 i32.const 76 call $read if unreachable end
    i32.const 76 i32.load if unreachable end
    local.get $node struct.get $node 0 i32.const 84 i32.add)
  (func $mutate_managed
    i32.const 80 i32.const 600 i32.store
    i32.const 84 i32.const 4 i32.store
    i32.const 3 i32.const 80 i32.const 1 i32.const 88 call $write if unreachable end
    nop ;; Byte offset33: real guest WASI write has returned, ready for restore.
  )
  ;; R36 private view coverage: null, final defined and/or native import leaf.
  (table $indirect-bindings 3 funcref)
  (elem (table $indirect-bindings) (i32.const 1) func $as $mutate_managed)
)
