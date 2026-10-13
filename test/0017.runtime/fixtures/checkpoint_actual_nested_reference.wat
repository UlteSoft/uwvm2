(module
  (type $cell (struct (field (mut i32))))
  (func (export "_start") (result anyref) call $child)
  (func $child (result (ref $cell))
    i64.const 99
    nop
    drop
    struct.new_default $cell))
