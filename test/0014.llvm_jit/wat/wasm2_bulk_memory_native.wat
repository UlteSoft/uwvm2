(module
  (memory 1)
  (data $payload "\11\22\33\44")

  (func $_start (export "_start")
    i32.const 8
    i32.const 0
    i32.const 4
    memory.init $payload
    data.drop $payload

    i32.const 12
    i32.const 8
    i32.const 4
    memory.copy

    i32.const 16
    i32.const 0xaa
    i32.const 4
    memory.fill

    i32.const 8
    i32.load
    i32.const 0x44332211
    i32.ne
    if
      unreachable
    end

    i32.const 12
    i32.load
    i32.const 0x44332211
    i32.ne
    if
      unreachable
    end

    i32.const 16
    i32.load
    i32.const 0xaaaaaaaa
    i32.ne
    if
      unreachable
    end))
