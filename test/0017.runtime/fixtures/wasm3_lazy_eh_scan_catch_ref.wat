(module
  (tag $e (param i32))
  (func $raise i32.const 42 throw $e)
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    block $caught (result i32 exnref)
      try_table (catch_ref $e $caught)
        call $raise
      end
      unreachable
    end
    drop drop
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
