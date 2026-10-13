(module
  (func (export "_start")
    i32.const 0
    i32.const 9
    ref.i31
    drop
    drop

    i64.const 1
    i32.const 9
    ref.i31
    drop
    drop

    f32.const 1
    i32.const 9
    ref.i31
    drop
    drop

    f64.const 1
    i32.const 9
    ref.i31
    drop
    drop

    i32.const 2
    i32.const 10
    ref.i31
    extern.convert_any
    any.convert_extern
    ref.cast (ref i31)
    i31.get_u
    i32.const 10
    i32.ne
    if unreachable end
    drop))
