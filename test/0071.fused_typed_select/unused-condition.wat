(module
  (func $empty_start (export "_start"))
  (func $unused i64.const 0 i64.const 1 i64.const 1 select (result i64) drop)
  (start $empty_start))
