(module
(type $node (struct (field i32)))
(import "wasi_snapshot_preview1" "fd_tell" (func $tell (param i32 i32) (result i32)))
(import "wasi_snapshot_preview1" "path_open" (func $open (param i32 i32 i32 i32 i32 i64 i64 i32 i32) (result i32)))
(import "wasi_snapshot_preview1" "fd_seek" (func $seek (param i32 i64 i32 i32) (result i32)))
(import "wasi_snapshot_preview1" "fd_renumber" (func $renumber (param i32 i32) (result i32)))
(memory (export "memory") 1)
(data (i32.const 32) "nested//./state.bin")
(func $stop (result i32)
(local $node (ref $node))
i32.const 7 struct.new $node local.set $node
nop
;; Exercise the published target table through actual guest WASI calls after
;; the genuine park: both restored aliases must share the native cursor.
i32.const 128 i64.const 0 i64.store
i32.const 91 i32.const 128 call $tell
if unreachable end
i32.const 128 i64.load i64.const 128 i64.ne
if unreachable end
i32.const 3 i64.const 129 i32.const 0 i32.const 128 call $seek
if unreachable end
i32.const 91 i32.const 136 call $tell
if unreachable end
i32.const 136 i64.load i64.const 129 i64.ne
if unreachable end
i32.const 91 i64.const 128 i32.const 0 i32.const 128 call $seek
if unreachable end
local.get $node struct.get $node 0)
(func (export "run") (result i32)
i32.const 151 i32.const 0 i32.const 32 i32.const 19 i32.const 0
i64.const 6291564 i64.const 0 i32.const 0 i32.const 16 call $open
if unreachable end
i32.const 16 i32.load i64.const 128 i32.const 0 i32.const 128 call $seek
if unreachable end
i32.const 16 i32.load i32.const 91 call $renumber
if unreachable end
call $stop))
