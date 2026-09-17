(module
  (global $source v128 (v128.const i32x4 1 -2 305419896 -2147483648))
  (global $saved (mut v128) (v128.const i32x4 0 0 0 0))
  (func $start
    global.get $source
    global.set $saved
    global.get $saved
    i32x4.extract_lane 0
    i32.const 1
    i32.ne
    if unreachable end
    global.get $saved
    i32x4.extract_lane 1
    i32.const -2
    i32.ne
    if unreachable end
    global.get $saved
    i32x4.extract_lane 2
    i32.const 305419896
    i32.ne
    if unreachable end
    global.get $saved
    i32x4.extract_lane 3
    i32.const -2147483648
    i32.ne
    if unreachable end
    ;; A second store must replace all lanes, including the high half of the opaque LLVM integer.
    v128.const i32x4 -1 -1 -1 -1
    global.set $saved
    global.get $saved
    i32x4.extract_lane 3
    i32.const -1
    i32.ne
    if unreachable end)
  (start $start))
