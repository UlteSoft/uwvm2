(module
  (func (export "_start") (result anyref) call $child)
  (func $child (result (ref i31)) i64.const 99 nop drop i32.const 7 ref.i31))
