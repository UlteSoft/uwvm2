(module
  ;; Matching cannot cross heap hierarchies, even with no retained GC context.
  (func $probe (result i32)
    i32.const 0 if
      ref.null noextern ref.as_non_null throw_ref
    end
    i32.const 42)
  (func (export "_start")
    call $probe i32.const 42 i32.ne if unreachable end))
