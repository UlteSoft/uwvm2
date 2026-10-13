(module
  (tag $payload (param (ref null noexn)))
  (func (export "_start") nop)
  (func $never_called
    (block $caught (result (ref exn))
      try_table (catch $payload $caught)
        unreachable
      end
      unreachable)
    drop))
