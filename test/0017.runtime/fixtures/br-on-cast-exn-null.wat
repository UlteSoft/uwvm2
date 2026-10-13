(module
  (func (export "_start")
    (block $matched (result (ref null exn))
      ref.null exn
      br_on_cast $matched (ref null exn) (ref null exn)
      drop
      ref.null exn)
    ref.is_null
    i32.eqz
    if unreachable end
    (block $failed (result (ref null exn))
      ref.null exn
      br_on_cast_fail $failed (ref null exn) (ref exn)
      drop
      ref.null exn)
    ref.is_null
    i32.eqz
    if unreachable end))
