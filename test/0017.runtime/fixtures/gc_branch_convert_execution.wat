(module
  (func $roundtrip (export "roundtrip") (result i32)
    i32.const 91
    ref.i31
    extern.convert_any
    any.convert_extern
    ref.cast (ref i31)
    i31.get_u)

  (func $null_extern_to_any (export "null_extern_to_any") (result i32)
    ref.null extern
    any.convert_extern
    ref.is_null)

  (func $null_any_to_extern (export "null_any_to_extern") (result i32)
    ref.null any
    extern.convert_any
    ref.is_null)

  (func $branch_roundtrip (export "branch_roundtrip") (result i32)
    (block $matched (result (ref i31))
      i32.const 47
      ref.i31
      extern.convert_any
      any.convert_extern
      br_on_cast $matched (ref null any) (ref i31)
      drop
      i32.const 0
      ref.i31)
    i31.get_u)

  (func (export "_start")
    call $roundtrip
    i32.const 91
    i32.ne
    if unreachable end
    call $null_extern_to_any
    i32.const 1
    i32.ne
    if unreachable end
    call $null_any_to_extern
    i32.const 1
    i32.ne
    if unreachable end
    call $branch_roundtrip
    i32.const 47
    i32.ne
    if unreachable end))
