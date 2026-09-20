(module
  (func (export "_start") (local $lane i32)
    i32.const 8
    local.set $lane

    v128.const i32x4 6 7 8 9
    v128.const i32x4 6 7 8 9
    i32x4.eq
    i32x4.all_true

    local.get $lane
    i32.const 8
    i32.eq
    i32.and
    i32.eqz
    if
      unreachable
    end))
