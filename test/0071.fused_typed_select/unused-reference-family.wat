(module
  (func $empty_start (export "_start"))
  (func $unused ref.null extern ref.null eq i32.const 0 select (result eqref) drop)
  (start $empty_start))
