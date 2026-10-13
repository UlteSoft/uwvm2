(module
  (func (export "cast_branch_success") (result i32)
    (block $matched (result (ref i31))
      i32.const 7
      ref.i31
      br_on_cast $matched (ref null any) (ref i31)
      drop
      i32.const 0
      ref.i31)
    i31.get_u)

  (func (export "cast_branch_failure") (result i32)
    (block $failed (result (ref null any))
      ref.null any
      br_on_cast_fail $failed (ref null any) (ref i31)
      drop
      ref.null any)
    ref.is_null)

  (func (export "_start")
    call 0
    i32.const 7
    i32.ne
    if unreachable end
    call 1
    i32.const 1
    i32.ne
    if unreachable end))
