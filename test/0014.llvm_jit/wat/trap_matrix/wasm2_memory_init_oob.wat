(module
  (memory 1)
  (data $payload "\11\22\33\44")

  (func $leaf
    i32.const 0
    i32.const 3
    i32.const 2
    memory.init $payload)

  (func $middle
    call $leaf)

  (func $_start (export "_start")
    call $middle))
