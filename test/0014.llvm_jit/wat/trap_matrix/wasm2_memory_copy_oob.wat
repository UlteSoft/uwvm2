(module
  (memory 1)

  (func $leaf
    i32.const 65535
    i32.const 0
    i32.const 2
    memory.copy)

  (func $middle
    call $leaf)

  (func $_start (export "_start")
    call $middle))
