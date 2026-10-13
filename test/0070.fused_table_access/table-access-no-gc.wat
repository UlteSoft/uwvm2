(module (table $exn i64 1 exnref)
 (func $start (export "_start") i64.const 0 ref.null noexn table.set $exn
  i64.const 0 table.get $exn ref.is_null i32.const 1 i32.ne if unreachable end)
 (start $start))
