(module
  (func (export "_start")
    block $skip
      br $skip
      i64.const 0
      ref.is_null drop
    end))
