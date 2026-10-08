(module
  (func (export "_start")
    ref.null func ref.is_null i32.const 1 i32.ne if unreachable end
    ref.null extern ref.is_null i32.const 1 i32.ne if unreachable end))
