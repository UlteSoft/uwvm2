(module
  (memory 1)
  (func (export "_start") (local $added v128)
    i32.const 0
    v128.const i32x4 1 2 3 4
    v128.store align=4

    i32.const 0
    v128.load align=4
    i32.const 5
    i32x4.splat
    i32x4.add
    local.set $added
    local.get $added
    v128.const i32x4 6 7 8 9
    i32x4.eq
    i32x4.all_true
    i32.eqz
    if
      unreachable
    end))
