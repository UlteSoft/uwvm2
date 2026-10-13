(module
  (func (export "_start") (result anyref) call $child)
  (func $child (result anyref) i64.const 99 nop drop ref.null any))
