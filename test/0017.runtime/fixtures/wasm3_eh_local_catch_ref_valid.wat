;; Non-null exn locals are exception types, independent of the GC feature gate.
(module
  (tag $value)
  (func $probe (local $exception (ref exn))
    block $captured (result (ref exn))
      try_table (catch_all_ref $captured)
        throw $value
      end
      unreachable
    end
    local.set $exception
    block $handled
      try_table (catch $value $handled)
        local.get $exception throw_ref
      end
      unreachable
    end)
  (func (export "_start") call $probe))
