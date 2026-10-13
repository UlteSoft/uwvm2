;; Core 3: catch_ref must produce a stable reference that survives both local
;; and mutable-global storage before throw_ref rethrows the same tag/payload.
(module
  (tag $t (param i32))
  (global $saved (mut (ref null exn)) (ref.null exn))
  (func (export "_start")
    (local $saved_local (ref null exn))
    block $outer (result i32)
      try_table (catch $t $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $t $inner)
            i32.const 31
            throw $t
          end
          unreachable
        end
        local.set $saved_local
        drop
        local.get $saved_local
        global.set $saved
        global.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 31
    i32.ne
    if unreachable end))
