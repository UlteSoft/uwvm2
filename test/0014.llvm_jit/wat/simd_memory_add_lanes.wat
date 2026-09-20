(module
  (memory 1)
  (func (export "_start") (local $value v128)
    i32.const 0
    v128.const i32x4 1 2 3 4
    v128.store align=4

    i32.const 0
    v128.load align=4
    i32.const 5
    i32x4.splat
    i32x4.add
    local.set $value

    local.get $value
    i32x4.extract_lane 0
    i32.const 6
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 1
    i32.const 7
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 2
    i32.const 8
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 3
    i32.const 9
    i32.ne
    if unreachable end))
