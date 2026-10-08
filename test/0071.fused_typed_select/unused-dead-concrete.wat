(module
  (func $empty_start (export "_start"))
  (func $unused unreachable i64.const 0 f64.const 0 i32.const 1 select (result i64) drop)
  (start $empty_start))
