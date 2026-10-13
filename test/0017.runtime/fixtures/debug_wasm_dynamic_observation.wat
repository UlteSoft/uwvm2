(module
  (type $number (func (param i32) (result i32)))
  (table 1 funcref)
  (func $leaf (type $number) (param i32) (result i32)
    local.get 0
    i32.const 7
    i32.add)
  (elem (i32.const 0) $leaf)
  (func (export "_start")
    i32.const 35
    i32.const 0
    call_indirect (type $number)
    drop
    i32.const 35
    ref.func $leaf
    call_ref $number
    drop))
