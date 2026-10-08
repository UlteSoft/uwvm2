(module
  (tag $e (param i32))
  (func $raise i32.const 42 throw $e)
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    block $caught (result exnref)
      try_table (catch_all_ref $caught)
        call $raise
      end
      unreachable
    end
    drop
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
