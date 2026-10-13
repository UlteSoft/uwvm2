(module
  (func $leaf (export "numeric_leaf") (param i32) (result i32)
    local.get 0
    i32.const 7
    i32.add)
  (func (export "_start")
    i32.const 5
    call $leaf
    i32.const 12
    i32.ne
    if unreachable end
    i32.const 5
    call $leaf
    i32.const 13
    i32.ne
    if unreachable end))
