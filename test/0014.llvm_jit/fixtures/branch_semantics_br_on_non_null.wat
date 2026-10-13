(module
  (type $t (func (result i32)))
  (func $seven (type $t) i32.const 7)
  (elem declare func $seven)
  (func $probe (param $make_nonnull i32) (result i32)
    block (result funcref)
      local.get $make_nonnull
      if (result (ref null $t))
        ref.func $seven
      else
        ref.null $t
      end
      br_on_non_null 0
      ref.null func
    end
    ref.is_null)
  (func (export "_start")
    i32.const 0 call $probe i32.const 1 i32.ne if unreachable end
    i32.const 1 call $probe i32.const 0 i32.ne if unreachable end))
