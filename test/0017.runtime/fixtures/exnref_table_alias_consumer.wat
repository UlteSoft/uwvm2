;; P -> B -> C aliases one table. C retains table.get's complete exception
;; carrier while B clears the provider-owned slot, then rethrows it.
(module
  (import "P" "raise" (func $raise (param i32)))
  (import "P" "t" (tag $t (param i32)))
  (import "B" "tab" (table $shared 1 4 exnref))
  (import "B" "clear" (func $clear))
  (func (export "_start")
    (local $saved (ref null exn))
    block $outer (result i32)
      try_table (catch $t $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $t $inner)
            i32.const 53
            call $raise
          end
          unreachable
        end
        local.set $saved
        drop
        i32.const 0
        local.get $saved
        table.set $shared
        ref.null exn
        local.set $saved
        i32.const 0
        table.get $shared
        local.set $saved
        call $clear
        local.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 53
    i32.ne
    if unreachable end))
