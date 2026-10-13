(module
  (memory 1 1 shared)
  (tag $t (param i32))
  (func $raise i32.const 42 throw $t)
  (func $atomic (result i32)
    i32.const 0 i32.const 42 i32.atomic.store
    i32.const 0 i32.atomic.load)
  (func (export "_start")
    block $out (result i32)
      try_table (catch $t $out) call $raise end unreachable
    end
    i32.const 42 i32.ne if unreachable end
    call $atomic i32.const 42 i32.ne if unreachable end))
