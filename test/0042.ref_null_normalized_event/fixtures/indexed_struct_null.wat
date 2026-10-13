(module
  (type $box (struct (field i32)))
  (func (export "_start")
    ref.null $box ref.is_null i32.const 1 i32.ne if unreachable end))
