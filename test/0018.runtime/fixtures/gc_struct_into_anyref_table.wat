(module
  (type $record (struct (field i32)))
  (table $values 2 anyref)
  (elem (table $values) (i32.const 0) (ref $record)
    (item (struct.new $record (i32.const 211))))
  (elem $later (ref $record)
    (item (struct.new $record (i32.const 212))))
  (func (export "_start")
    (if (i32.ne
          (struct.get $record 0 (ref.cast (ref $record) (table.get $values (i32.const 0))))
          (i32.const 211))
      (then unreachable))
    (table.init $values $later (i32.const 1) (i32.const 0) (i32.const 1))
    (if (i32.ne
          (struct.get $record 0 (ref.cast (ref $record) (table.get $values (i32.const 1))))
          (i32.const 212))
      (then unreachable))))
