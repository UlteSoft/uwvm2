(module
  (type $s (struct (field i32)))
  (func $later (result i32) i32.const 47)
  (func $probe (result i32)
    ref.null $s ref.null $s i32.const 0 select (result (ref $s)) drop
    call $later)
  (func (export "_start")
    call $probe i32.const 47 i32.ne if unreachable end))
