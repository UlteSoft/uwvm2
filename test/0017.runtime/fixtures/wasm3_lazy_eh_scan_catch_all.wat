(module
  (tag $e (param i32))
  (func $raise i32.const 42 throw $e)
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    block $caught
      try_table (catch_all $caught)
        call $raise
      end
      unreachable
    end
    
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
