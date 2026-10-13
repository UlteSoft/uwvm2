(module
  (rec (type $t (func (param i32) (result i32))))
  (func $increment (type $t)
    local.get 0
    i32.const 1
    i32.add)
  (elem declare func $increment)
  (func (export "_start")
    i32.const 41
    ref.func $increment
    call_ref $t
    i32.const 42
    i32.ne
    if unreachable end))
