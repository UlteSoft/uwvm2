(module
  (memory i64 1)
  (data (i64.const 0) "\11\22\33\44")
  (func $helper (result i32)
    i64.const 0 i32.load drop
    i32.const 7)
  (func (export "_start") (result i32) call $helper))
