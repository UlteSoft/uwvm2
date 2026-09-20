(module
  (func (export "_start")
    v128.const i32x4 6 7 8 9
    v128.const i32x4 6 7 8 9
    i32x4.eq
    i32x4.all_true
    i32.eqz
    if
      unreachable
    end))
