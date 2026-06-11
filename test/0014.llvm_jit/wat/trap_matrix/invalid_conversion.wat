(module
  (type $v (func))

  (func $leaf (type $v)
    f32.const nan
    i32.trunc_f32_s
    drop)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
