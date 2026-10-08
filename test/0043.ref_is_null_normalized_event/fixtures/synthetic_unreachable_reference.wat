(module
  (func (export "_start")
    block $skip
      br $skip
      ref.is_null drop
    end
    ref.null extern ref.is_null
    i32.const 1 i32.ne if unreachable end))
