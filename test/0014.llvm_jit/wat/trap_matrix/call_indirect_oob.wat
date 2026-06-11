(module
  (type $v (func))
  (table 1 funcref)

  (func $leaf (type $v)
    i32.const 1
    call_indirect (type $v))

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
