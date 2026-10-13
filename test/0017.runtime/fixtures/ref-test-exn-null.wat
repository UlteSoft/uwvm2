(module
  (func (export "_start")
    ref.null exn
    ref.test (ref null exn)
    i32.eqz
    if unreachable end))
