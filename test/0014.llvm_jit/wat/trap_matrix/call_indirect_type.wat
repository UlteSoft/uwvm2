(module
  (type $v (func))
  (type $i (func (param i32)))
  (table 1 funcref)
  (elem (i32.const 0) $target)

  (func $target (type $i) (param $x i32)
    local.get $x
    drop)

  (func $leaf (type $v)
    i32.const 0
    call_indirect (type $v))

  (func $mid (type $v) call $leaf)
  (func $top (type $v) call $mid)
  (func $_start (export "_start") (type $v) call $top))
