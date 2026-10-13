;; Core 3 array segments: byte offsets, element offsets, drop state, and identity.
(module
  (type $half (array (mut i16)))
  (type $functions (array (mut funcref)))
  (memory 1)
  (data $bytes "\01\02\03\04\05\06")
  (func $target)
  (elem $functions_segment funcref (ref.func $target) (ref.null func))
  (func (export "_start") (local $a (ref $half)) (local $f (ref $functions))
    i32.const 1
    i32.const 2
    array.new_data $half $bytes
    local.set $a
    local.get $a
    i32.const 0
    array.get_u $half
    i32.const 0x0302
    i32.ne
    if unreachable end
    local.get $a
    i32.const 1
    array.get_u $half
    i32.const 0x0504
    i32.ne
    if unreachable end
    local.get $a
    i32.const 1
    i32.const 0
    i32.const 1
    array.init_data $half $bytes
    local.get $a
    i32.const 1
    array.get_u $half
    i32.const 0x0201
    i32.ne
    if unreachable end
    i32.const 0
    i32.const 2
    array.new_elem $functions $functions_segment
    local.set $f
    local.get $f
    i32.const 0
    array.get $functions
    ref.is_null
    if unreachable end
    local.get $f
    i32.const 1
    i32.const 0
    i32.const 1
    array.init_elem $functions $functions_segment
    local.get $f
    i32.const 1
    array.get $functions
    ref.is_null
    if unreachable end
    data.drop $bytes
    i32.const 0
    i32.const 0
    array.new_data $half $bytes
    array.len
    if unreachable end
    elem.drop $functions_segment
    i32.const 0
    i32.const 0
    array.new_elem $functions $functions_segment
    array.len
    if unreachable end)
  (func (export "trap_data")
    data.drop $bytes
    i32.const 0
    i32.const 1
    array.new_data $half $bytes
    drop)
  (func (export "trap_elem")
    elem.drop $functions_segment
    i32.const 0
    i32.const 1
    array.new_elem $functions $functions_segment
    drop))
