;; A provider exception passes through an imported table and a local table.
;; table.copy must retain the reference in the destination module's GC store.
(module
  (import "P" "raise" (func $raise (param i32)))
  (import "P" "clear" (func $clear))
  (import "P" "t" (tag $t (param i32)))
  (import "P" "tab" (table $shared 1 4 exnref))
  (table $local 1 4 exnref)
  (func (export "_start")
    (local $saved (ref null exn))
    block $outer (result i32)
      try_table (catch $t $outer)
        block $inner (result i32 (ref exn))
          try_table (catch_ref $t $inner)
            i32.const 43
            call $raise
          end
          unreachable
        end
        local.set $saved
        drop
        i32.const 0
        local.get $saved
        table.set $shared
        i32.const 0
        i32.const 0
        i32.const 1
        table.copy $local $shared
        ref.null exn
        local.set $saved
        i32.const 0
        table.get $local
        local.set $saved
        call $clear
        i32.const 0
        ref.null exn
        table.set $local
        local.get $saved
        throw_ref
      end
      unreachable
    end
    i32.const 43
    i32.ne
    if unreachable end))
