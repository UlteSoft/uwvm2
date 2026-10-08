(module
  (type $ints (array (mut i32)))
  (type $any (array (mut anyref)))
  (type $eq (array eqref))

  (func (export "fill") (result i32)
    (local (ref null $ints))
    i32.const 3
    array.new_default $ints
    local.set 0
    local.get 0
    i32.const 0
    i32.const 7
    i32.const 3
    array.fill $ints
    local.get 0
    i32.const 1
    array.get $ints)

  (func (export "copy") (result i32)
    (local (ref null $ints))
    (local (ref null $ints))
    i32.const 11
    i32.const 3
    array.new $ints
    local.set 0
    i32.const 3
    array.new_default $ints
    local.set 1
    local.get 1
    i32.const 0
    local.get 0
    i32.const 0
    i32.const 3
    array.copy $ints $ints
    local.get 1
    i32.const 2
    array.get $ints)

  (func (export "copy_covariant") (result i32)
    (local (ref null $any))
    (local (ref null $eq))
    i32.const 1
    array.new_default $any
    local.set 0
    i32.const 13
    ref.i31
    i32.const 1
    array.new $eq
    local.set 1
    local.get 0
    i32.const 0
    local.get 1
    i32.const 0
    i32.const 1
    array.copy $any $eq
    local.get 0
    i32.const 0
    array.get $any
    ref.test (ref i31))

  (func (export "copy_overlap") (result i32)
    (local (ref null $ints))
    i32.const 1
    i32.const 2
    i32.const 3
    i32.const 4
    array.new_fixed $ints 4
    local.set 0
    local.get 0
    i32.const 1
    local.get 0
    i32.const 0
    i32.const 3
    array.copy $ints $ints
    local.get 0
    i32.const 3
    array.get $ints)

  (func (export "_start")
    call 0
    i32.const 7
    i32.ne
    if unreachable end
    call 1
    i32.const 11
    i32.ne
    if unreachable end
    call 2
    i32.const 1
    i32.ne
    if unreachable end
    call 3
    i32.const 3
    i32.ne
    if unreachable end))
