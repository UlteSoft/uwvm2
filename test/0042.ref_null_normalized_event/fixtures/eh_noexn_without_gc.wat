(module
  (func (export "_start")
    ref.null noexn ref.is_null i32.const 1 i32.ne if unreachable end
    ref.null exn ref.is_null i32.const 1 i32.ne if unreachable end))
