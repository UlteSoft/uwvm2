;; A Core 3 exnref table uses i64 addresses without narrowing table indices.
(module
  (table i64 1 4 exnref)
  (func (export "_start")
    i64.const 0
    ref.null exn
    table.set 0
    i64.const 0
    table.get 0
    ref.is_null
    i32.eqz
    if unreachable end
    ref.null exn
    i64.const 1
    table.grow 0
    i64.const 1
    i64.ne
    if unreachable end
    table.size 0
    i64.const 2
    i64.ne
    if unreachable end))
