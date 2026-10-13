(module
  (func (export "block_any") (result i32)
    (block (result anyref)
      ref.null any)
    ref.is_null)

  (func (export "block_i31") (result i32)
    (block (result (ref i31))
      i32.const 17
      ref.i31)
    i31.get_u)

  (func (export "_start")
    call 0
    i32.const 1
    i32.ne
    if unreachable end
    call 1
    i32.const 17
    i32.ne
    if unreachable end))
