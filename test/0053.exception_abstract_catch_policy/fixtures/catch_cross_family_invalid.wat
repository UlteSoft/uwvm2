(module
  (tag $payload (param (ref null noexn)))
  (func (export "_start")
    (block $caught (result externref)
      ;; Equal pointer-size carriers cannot join exn and extern heap families.
      try_table (catch $payload $caught)
        unreachable
      end
      unreachable)
    drop))
