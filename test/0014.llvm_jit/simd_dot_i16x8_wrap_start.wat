(module
  (func (export "_start")
    (local $dot v128)

    v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768
    v128.const i16x8 -32768 -32768 -32768 -32768 -32768 -32768 -32768 -32768
    i32x4.dot_i16x8_s
    local.set $dot

    local.get $dot
    i32x4.extract_lane 0
    i32.const -2147483648
    i32.ne
    if
      unreachable
    end

    local.get $dot
    i32x4.extract_lane 3
    i32.const -2147483648
    i32.ne
    if
      unreachable
    end))
