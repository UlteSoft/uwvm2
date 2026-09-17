(module
  (table 1 funcref)

  (func $leaf
    i32.const 0
    i32.const 0
    i32.const 2
    table.copy)

  (func $middle
    call $leaf)

  (func $_start (export "_start")
    call $middle))
