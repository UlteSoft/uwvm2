;; Core3 table64 index remains i64 even on a native 32-bit target.
;; Table zero contains one null reference; the last table.get must trap table OOB.
(module
  (table i64 1 1 funcref)
  (func (export "_start")
    i64.const 0
    table.get 0
    ref.is_null
    i32.eqz
    if unreachable end
    i64.const 4294967296
    table.get 0
    drop))
