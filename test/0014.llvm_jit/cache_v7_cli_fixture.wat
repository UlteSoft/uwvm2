;; SOURCE ONLY. Build with a pinned actual wasm-tools in the original cgroup.
;; The real WASIp1 import avoids pretending a hand-built cache_context is a Source.
(module
  (import "wasi_snapshot_preview1" "fd_write" (func $write (param i32 i32 i32 i32) (result i32)))
  (memory (export "memory") 1)
  (data (i32.const 0) "\20\00\00\00\0f\00\00\00")
  (data (i32.const 32) "CACHE-ORACLE42\0a")
  (func $answer (param $x i32) (result i32)
    local.get $x i32.const 2 i32.mul)
  (func (export "_start")
    i32.const 21 call $answer i32.const 42 i32.ne
    if unreachable end
    i32.const 1 i32.const 0 i32.const 1 i32.const 16 call $write
    if unreachable end))
