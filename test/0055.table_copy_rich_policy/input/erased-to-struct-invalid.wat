(module
  (type $box (struct (field i32)))
  (table $erased 1 funcref)
  (table $typed 1 (ref null $box))
  ;; Function and struct heaps both use the current legacy carrier 0x70.
  (func (export "_start")
    i32.const 0 i32.const 0 i32.const 0 table.copy $typed $erased))
