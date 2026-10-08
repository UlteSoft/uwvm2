(module
  ;; Core 3 table definitions may broadcast a GC constant expression. The
  ;; ref.i31 constructor belongs to GC, independently of extended-const.
  (table $values 3 i31ref (ref.i31 (i32.const 19)))
  (func (export "_start")
    i32.const 0
    table.get $values
    i31.get_s
    i32.const 19
    i32.ne
    if unreachable end
    i32.const 2
    table.get $values
    i31.get_s
    i32.const 19
    i32.ne
    if unreachable end))
