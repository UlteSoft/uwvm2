(module
  (type $v (func))
  (memory 1)

  (func $leaf (type $v)
    i32.const -1
    i32.const 1
    i32.store)

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
