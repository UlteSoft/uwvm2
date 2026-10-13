(module
  (table $values 2 anyref)
  (elem (table $values) (i32.const 0) i31ref
    (item (ref.i31 (i32.const 170))))
  (elem $later i31ref
    (item (ref.i31 (i32.const 187))))
  (func (export "_start")
    (if (i32.ne
          (i31.get_u (ref.cast i31ref (table.get $values (i32.const 0))))
          (i32.const 170))
      (then unreachable))
    (table.init $values $later (i32.const 1) (i32.const 0) (i32.const 1))
    (if (i32.ne
          (i31.get_u (ref.cast i31ref (table.get $values (i32.const 1))))
          (i32.const 187))
      (then unreachable))))
