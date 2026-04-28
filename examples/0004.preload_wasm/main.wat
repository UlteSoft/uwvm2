(module
  (import "lib.test" "get_argc" (func $get_argc (result i32)))
  (import "lib.test" "get_argv_buf_size" (func $get_argv_buf_size (result i32)))

  (func $assert_eq (param i32 i32)
    local.get 0
    local.get 1
    i32.ne
    if
      unreachable
    end)

  (func $start
    call $get_argc
    i32.const 3
    call $assert_eq

    call $get_argv_buf_size
    i32.const 16
    call $assert_eq)

  (start $start))
