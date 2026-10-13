(module
  (type $cell (struct (field (mut i32))))
  (func (export "_start") (result externref) call $child)
  (func $child (result externref)
    i64.const 99 nop drop
    struct.new_default $cell extern.convert_any))
