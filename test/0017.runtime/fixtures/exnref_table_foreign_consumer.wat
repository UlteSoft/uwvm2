;; A consumer-created exception is stored in a provider-owned imported table.
;; The provider's table root must retain the foreign token before publication.
(module
  (import "P" "tab" (table $shared 1 4 exnref))
  (import "P" "clear" (func $clear))
  (tag $local (param i32))
  (func (export "_start")
    (local $saved (ref null exn))
    block $outer (result i32)
      try_table (catch $local $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $local $inner)
            i32.const 47
            throw $local
          end
          unreachable
        end
        local.set $saved
        drop
        i32.const 0
        local.get $saved
        table.set $shared
        local.get $saved
        i32.const 1
        table.grow $shared
        i32.const 1
        i32.ne
        if unreachable end
        i32.const 1
        local.get $saved
        i32.const 1
        table.fill $shared
        i32.const 0
        i32.const 1
        i32.const 1
        table.copy $shared $shared
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
    i32.const 47
    i32.ne
    if unreachable end))
