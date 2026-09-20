(module
  (table 1 funcref)

  (func $leaf
    i32.const 1
    table.get 0
    drop)

  (func $middle
    call $leaf)

  (func $_start (export "_start")
    call $middle))
