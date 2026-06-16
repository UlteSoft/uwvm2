(module
  (type $v (func))

  (func $leaf (type $v)
    i64.const -9223372036854775808
    i64.const -1
    i64.div_s
    drop)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
