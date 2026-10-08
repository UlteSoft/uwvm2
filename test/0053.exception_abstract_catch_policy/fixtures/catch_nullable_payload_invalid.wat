(module
  (tag $payload (param (ref null noexn)))
  (func (export "_start")
    (block $caught (result (ref exn))
      ;; A nullable payload cannot populate a nonnullable label, even dead code.
      try_table (catch $payload $caught)
        unreachable
      end
      unreachable)
    drop))
