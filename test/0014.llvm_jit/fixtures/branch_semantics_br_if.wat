(module
  (type $t (func (result i32)))
  (func $seven (type $t) i32.const 7)
  (elem declare func $seven)
  (func $probe (param $take i32) (result i32)
    (local $fallthrough i32)
    block (result funcref)
      ref.func $seven
      local.get $take
      br_if 0
      i32.const 1
      local.set $fallthrough
    end
    ref.is_null
    if unreachable end
    local.get $fallthrough)
  (func (export "_start")
    i32.const 0 call $probe i32.const 1 i32.ne if unreachable end
    i32.const 1 call $probe i32.const 0 i32.ne if unreachable end))
