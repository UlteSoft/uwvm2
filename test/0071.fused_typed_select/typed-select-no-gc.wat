;; GC/function-references OFF: no rich types; noexn is still a subtype of exn.
(module
  (func $called_start (export "_start")
    ref.null noexn ref.null noexn i32.const 1 select (result exnref)
    ref.is_null i32.const 1 i32.ne if unreachable end)
  (start $called_start))
