(module
  (type $empty (func))
  (memory (export "memory") 1 1)
  (table (export "table") 1 1 funcref)
  (func $seed (type $empty))
  (elem (i32.const 0) func $seed)
  (data (i32.const 4) "\c3")
  (func $start
    i32.const 0 call_indirect (type $empty)
    i32.const 3 i32.const 113 i32.store8)
  (start $start))
