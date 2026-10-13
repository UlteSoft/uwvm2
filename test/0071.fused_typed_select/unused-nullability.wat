(module
  (func $empty_start (export "_start"))
  (func $unused ref.null i31 ref.null i31 i32.const 1 select (result (ref i31)) drop)
  (start $empty_start))
