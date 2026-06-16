(module
  (type $v (func))

  (func $leaf (type $v)
    i32.const 123
    i32.const 0
    i32.div_s
    drop)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
