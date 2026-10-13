(module
  (type $cells (array (mut i32)))
  (func (export "_start") (result anyref) call $child)
  (func $child (result (ref $cells))
    i64.const 99 nop drop
    i32.const 42 i32.const 2 array.new $cells))
