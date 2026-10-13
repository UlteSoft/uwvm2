(module
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    ref.null noexn ref.null noexn i32.const 1 select (result (ref null noexn)) drop
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
