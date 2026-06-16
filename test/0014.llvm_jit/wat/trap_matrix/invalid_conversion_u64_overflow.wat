(module
  (type $v (func))

  (func $leaf (type $v)
    f64.const 1e40
    i64.trunc_f64_u
    drop)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
