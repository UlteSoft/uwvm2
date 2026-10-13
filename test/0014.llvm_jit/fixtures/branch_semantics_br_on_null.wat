(module
  (type $t (func (result i32)))
  (func $seven (type $t) i32.const 7)
  (elem declare func $seven)
  (func $probe (param $make_null i32) (result i32)
    block (result i32)
      i32.const 17
      local.get $make_null
      if (result (ref null $t))
        ref.null $t
      else
        ref.func $seven
      end
      br_on_null 0
      drop
      drop
      i32.const 29
    end)
  (func (export "_start")
    i32.const 1 call $probe i32.const 17 i32.ne if unreachable end
    i32.const 0 call $probe i32.const 29 i32.ne if unreachable end))
