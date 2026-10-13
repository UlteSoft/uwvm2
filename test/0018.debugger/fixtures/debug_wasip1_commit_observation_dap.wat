(module
  (import "wasi_snapshot_preview1" "fd_pread" (func $pread (param i32 i32 i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_pwrite" (func $pwrite (param i32 i32 i32 i64 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_seek" (func $seek (param i32 i64 i32 i32) (result i32)))
  (import "wasi_snapshot_preview1" "fd_tell" (func $tell (param i32 i32) (result i32)))
  (memory (export "memory") 1)
  (global $keep (mut i32) (i32.const 1))
  (global $count (mut i32) (i32.const 0))
  (global $fd (mut i32) (i32.const 2147483647))
  (global $write (mut i32) (i32.const 0))
  (global $position (mut i32) (i32.const -1))
  (global $scratch (mut i32) (i32.const 0))
  (func $spin (export "spin") (local $marker i32)
    i32.const 17 local.set $marker
    (loop $again
      global.get $position i32.const 0 i32.ge_s
      (if (then
        global.get $fd global.get $position i64.extend_i32_u i32.const 0 i32.const 64 call $seek drop
        i32.const -1 global.set $position))
      global.get $write
      (if (then
        i32.const 40 global.get $fd i32.const 16 i32.const 1 i64.const 0 i32.const 44 call $pwrite i32.store
        i32.const 0 global.set $write))
      i32.const 32 global.get $fd i32.const 0 i32.const 1 i64.const 0 i32.const 36 call $pread i32.store
      i32.const 48 global.get $fd i32.const 64 call $tell i32.store
      i32.const 512 global.get $scratch i32.store8
      global.get $count i32.const 1 i32.add global.set $count
      global.get $keep br_if $again))
  (func (export "_start")
    i32.const 0 i32.const 128 i32.store
    i32.const 4 i32.const 4 i32.store
    i32.const 16 i32.const 256 i32.store
    i32.const 20 i32.const 4 i32.store
    call $spin)
  (data (i32.const 256) "\ff\80\01\00"))
