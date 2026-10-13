(module
  (tag $raised)
  (func (export "_start") (result exnref) call $child)
  (func $child (result exnref)
    i64.const 99 nop drop
    try_table (result exnref) (catch_all_ref 0)
      throw $raised
    end))
