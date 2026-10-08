(module
  ;; Plain function signatures have no GC/group prefix. Execute with GC disabled
  ;; and exceptions/function-references enabled: both lazy modes MUST call probe.
  (func $probe (result i32)
    i32.const 0 if
      ref.null noexn ref.as_non_null throw_ref
    end
    i32.const 42)
  (func (export "_start")
    call $probe i32.const 42 i32.ne if unreachable end))
