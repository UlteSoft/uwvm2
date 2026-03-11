(module
  (import "dl.example" "add_i32" (func $add_i32 (param i32 i32) (result i32)))
  (import "dl.example" "inspect_memory" (func $inspect_memory (result i32)))
  (memory 1)
  (data (i32.const 0) "ABCD")

  (func $start
    i32.const 7
    i32.const 9
    call $add_i32
    i32.const 16
    i32.ne
    if
      unreachable
    end

    call $inspect_memory
    i32.const 266
    i32.ne
    if
      unreachable
    end

    i32.const 1
    i32.load8_u
    i32.const 90
    i32.ne
    if
      unreachable
    end)

  (start $start))
