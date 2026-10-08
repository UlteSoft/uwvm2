(module
  (func $empty_start (export "_start"))
  (func $unused f64.const 0 i64.const 0 select (result i64) drop)
  (start $empty_start))
