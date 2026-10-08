(module
  (tag $problem)
  (func (export "_start")
    (block $caught (result exnref)
      (try_table (catch_all_ref $caught)
        throw $problem)
      unreachable)
    ref.is_null if unreachable end
    ref.null noexn ref.is_null
    i32.const 1 i32.ne if unreachable end))
