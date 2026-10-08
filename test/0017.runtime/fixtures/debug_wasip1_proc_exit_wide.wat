(module
  (import "wasi_snapshot_preview1" "proc_exit" (func $exit (param i32)))
  (func $leaf (param $code i32)
    local.get $code
    call $exit
    unreachable)
  (func $outer (param $code i32)
    local.get $code
    call $leaf)
  (func (export "_start")
    i32.const 255
    call $outer))
