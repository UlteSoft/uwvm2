(module
  (type $v (func))
  (table 1 funcref)
  (func $target (type $v))
  (elem $payload func $target)

  (func $leaf
    i32.const 0
    i32.const 0
    i32.const 2
    table.init $payload)

  (func $middle
    call $leaf)

  (func $_start (export "_start")
    call $middle))
