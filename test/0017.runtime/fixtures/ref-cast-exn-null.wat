(module
  (func (export "_start")
    ref.null exn
    ref.cast (ref null exn)
    ref.is_null
    i32.eqz
    if unreachable end))
