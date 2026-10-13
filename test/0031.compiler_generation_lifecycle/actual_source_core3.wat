(module
  (type $box (struct (field i32)))
  (memory i64 1)
  (table i64 1 funcref)
  (func (param i32) (result i32)
    (local (ref $box)) (local i32)
    local.get 0 i32.const 5 i32.add))
