;; Initializing an exn local inside an enclosing try_table does not escape it.
(module
  (tag $value)
  (func $probe (local $exception (ref exn))
    try_table
      block $captured (result (ref exn))
        try_table (catch_all_ref $captured)
          throw $value
        end
        unreachable
      end
      local.set $exception
    end
    local.get $exception drop)
  (func (export "_start") call $probe))
