(module
  (memory i64 1)
  (table i64 1 funcref)
  (func $probe (result i32)
    i64.const 0 i32.const 42 i32.store
    i64.const 0 ref.null func table.set 0
    i64.const 0 table.get 0 drop
    i64.const 0 i32.load)
  (func (export "_start") call $probe drop))
