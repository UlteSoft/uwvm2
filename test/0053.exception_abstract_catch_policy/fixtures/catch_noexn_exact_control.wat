(module
  (tag $payload (param (ref null noexn)))
  (func (export "_start")
    (block $caught (result (ref null noexn))
      try_table (catch $payload $caught)
        ref.null noexn
        throw $payload
      end
      unreachable)
    ref.is_null
    i32.eqz
    if unreachable end))
