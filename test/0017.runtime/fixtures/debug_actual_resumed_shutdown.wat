(module
  (func (export "run") (result i32)
    call 1)
  (func (result i32) (local $ref (ref i31))
    i32.const 73
    ref.i31
    local.set $ref
    nop
    loop $forever
      nop
      br $forever
    end
    local.get $ref
    i31.get_s))
