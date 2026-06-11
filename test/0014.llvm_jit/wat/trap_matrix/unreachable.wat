(module
  (type $v (func))

  (func $leaf (type $v)
    unreachable)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
