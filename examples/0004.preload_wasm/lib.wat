(module
  (import "wasi_snapshot_preview1" "args_sizes_get" (func $args_sizes_get (param i32 i32) (result i32)))
  (memory 1)

  (func (export "get_argc") (result i32)
    i32.const 0
    i32.const 4
    call $args_sizes_get
    if
      unreachable
    end
    i32.const 0
    i32.load)

  (func (export "get_argv_buf_size") (result i32)
    i32.const 0
    i32.const 4
    call $args_sizes_get
    if
      unreachable
    end
    i32.const 4
    i32.load))
