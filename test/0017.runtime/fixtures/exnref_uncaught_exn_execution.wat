;; catch_all_ref creates a VM-managed exnref token; an outer uncaught exception
;; carries that token and must report it without following guest-controlled bits.
(module
  (tag $base)
  (tag $outer (param (ref exn)))
  (func $throwing (export "_start")
    block $caught (result (ref exn))
      try_table (catch_all_ref $caught)
        throw $base
      end
      unreachable
    end
    throw $outer))
