(module
  (func $empty_start (export "_start"))
  (func $unused i32.const 0 i32.const 1 ref.null noexn select (result i32) drop)
  (start $empty_start))
