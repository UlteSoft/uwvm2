(module
  (memory 1)
  (func (export "_start")
    i32.const 0
    v128.const i32x4 1 2 3 4
    v128.store align=4

    i32.const 0
    v128.load align=4
    i32.const 5
    i32x4.splat
    i32x4.add
    i32x4.extract_lane 2
    i32.const 8
    i32.ne
    if
      unreachable
    end))
