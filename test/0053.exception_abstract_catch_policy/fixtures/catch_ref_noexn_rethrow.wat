(module
  (tag $payload (param (ref null noexn)))
  (func (export "_start")
    (block $recaught (result exnref)
      (block $first (result exnref (ref exn))
        try_table (catch_ref $payload $first)
          ref.null noexn
          throw $payload
        end
        unreachable)
      ;; Real payload and nonnull exception reference become this try's inputs.
      try_table (param exnref (ref exn)) (catch $payload $recaught)
        throw_ref
      end
      unreachable)
    ref.is_null
    i32.eqz
    if unreachable end))
