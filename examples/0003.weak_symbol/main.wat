(module
  (import "weak.example" "add_i32" (func $add_i32 (param i32 i32) (result i32)))
  (import "weak.example" "inspect_memory" (func $inspect_memory (result i32)))
  (import "weak.example" "probe_host_apis" (func $probe_host_apis (param i32 i32) (result i32)))
  (memory 1)
  (data (i32.const 0) "ABCD")

  (func $assert_eq (param i32 i32)
    local.get 0
    local.get 1
    i32.ne
    if
      unreachable
    end)

  (func $start
    i32.const 11
    i32.const 5
    call $add_i32
    i32.const 16
    call $assert_eq

    call $inspect_memory
    i32.const 266
    call $assert_eq

    i32.const 1
    i32.load8_u
    i32.const 90
    call $assert_eq

    i32.const 32
    i32.const 36
    call $probe_host_apis
    i32.const 0
    call $assert_eq)

  (start $start))
