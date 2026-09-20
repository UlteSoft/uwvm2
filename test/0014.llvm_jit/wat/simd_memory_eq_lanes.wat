(module
  (memory 1)
  (func (export "_start") (local $value v128)
    i32.const 0
    v128.const i32x4 1 2 3 4
    v128.store align=4

    v128.const i32x4 6 7 8 9
    i32.const 0
    v128.load align=4
    i32.const 5
    i32x4.splat
    i32x4.add
    i32x4.eq
    local.set $value

    local.get $value
    i32x4.extract_lane 0
    i32.const -1
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 1
    i32.const -1
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 2
    i32.const -1
    i32.ne
    if unreachable end
    local.get $value
    i32x4.extract_lane 3
    i32.const -1
    i32.ne
    if unreachable end))
